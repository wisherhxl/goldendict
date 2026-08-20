// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/network/network_runtime.h"

#include "http_client.h"
#include "network_cache_storage.h"
#include "network_runtime_test_access.h"

#include <QAbstractNetworkCache>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QThread>

#include <algorithm>
#include <atomic>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace goldendict::network {
namespace {

constexpr std::int64_t kBytesPerMebibyte = 1024LL * 1024LL;

std::uint64_t NextRuntimeIdentity() noexcept {
    static std::atomic<std::uint64_t> next{1U};
    std::uint64_t identity = next.fetch_add(1U);
    if (identity == 0U) {
        identity = next.fetch_add(1U);
    }
    return identity;
}

class PreparedNetworkDiskCache final : public QNetworkDiskCache {
   public:
    void SetMaximumCacheSizeWithoutExpiry(qint64 size) {
        suppress_expiry_ = true;
        setMaximumCacheSize(size);
        suppress_expiry_ = false;
    }

    void FinishDeferredExpiry() { QNetworkDiskCache::expire(); }

   protected:
    qint64 expire() override {
        if (suppress_expiry_) {
            return maximumCacheSize();
        }
        if (cacheDirectory().isEmpty()) {
            return 0;
        }
        return QNetworkDiskCache::expire();
    }

   private:
    bool suppress_expiry_ = false;
};

// QNetworkAccessManager owns this gate for the runtime lifetime. The one disk
// cache behind it is bound during runtime construction and is never replaced;
// candidates can only prepare the layout for that existing binding.
class NetworkDiskCacheGate final : public QAbstractNetworkCache {
   public:
    NetworkDiskCacheGate(std::unique_ptr<PreparedNetworkDiskCache> cache,
                         bool enabled)
        : cache_(std::move(cache)), enabled_(enabled) {}

    QNetworkCacheMetaData metaData(const QUrl& url) override {
        return enabled_ ? cache_->metaData(url) : QNetworkCacheMetaData{};
    }

    void updateMetaData(const QNetworkCacheMetaData& metadata) override {
        if (enabled_) {
            cache_->updateMetaData(metadata);
        }
    }

    QIODevice* data(const QUrl& url) override {
        return enabled_ ? cache_->data(url) : nullptr;
    }

    bool remove(const QUrl& url) override {
        return enabled_ && cache_->remove(url);
    }

    qint64 cacheSize() const override {
        return enabled_ ? cache_->cacheSize() : 0;
    }

    QIODevice* prepare(const QNetworkCacheMetaData& metadata) override {
        return enabled_ ? cache_->prepare(metadata) : nullptr;
    }

    void insert(QIODevice* device) override { cache_->insert(device); }

    void clear() override { cache_->clear(); }

    bool PublishPositiveMaximum(qint64 maximum_cache_bytes) {
        const bool reduced = maximum_cache_bytes < cache_->maximumCacheSize();
        cache_->SetMaximumCacheSizeWithoutExpiry(maximum_cache_bytes);
        enabled_ = true;
        return reduced;
    }

    void FinishDeferredExpiry() { cache_->FinishDeferredExpiry(); }

    void Disable() noexcept { enabled_ = false; }

    void PrepareExistingBindingLayout() {
        cache_->setCacheDirectory(cache_->cacheDirectory());
        ++directory_configuration_count_;
    }

    PreparedNetworkDiskCache& disk_cache() noexcept { return *cache_; }

    std::uint64_t directory_configuration_count() const noexcept {
        return directory_configuration_count_;
    }

   private:
    std::unique_ptr<PreparedNetworkDiskCache> cache_;
    bool enabled_ = false;
    std::uint64_t directory_configuration_count_ = 1U;
};

struct CandidateResource final {
    QThread* construction_thread = nullptr;
    QThread* owner_thread = nullptr;
    std::int64_t maximum_cache_bytes = 0;
    std::string cache_directory;
    std::function<void(bool)> destruction_observer;
};

class CandidateOwner final {
   public:
    CandidateOwner(QObject& worker, QThread& thread)
        : worker_(worker), thread_(thread) {}

    void Register(const std::shared_ptr<CandidateResource>& resource) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            resources_.push_back(resource);
        }
    }

    void Destroy(const std::shared_ptr<CandidateResource>& resource) noexcept {
        if (!resource) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        ResetOnOwnerThread(resource);
        EraseExpired();
    }

    void Shutdown() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        for (const auto& weak : resources_) {
            if (const auto resource = weak.lock()) {
                ResetOnOwnerThread(resource);
            }
        }
        resources_.clear();
        running_ = false;
    }

   private:
    void ResetOnOwnerThread(
        const std::shared_ptr<CandidateResource>& resource) noexcept {
        auto reset = [resource]() {
            if (resource->destruction_observer) {
                try {
                    resource->destruction_observer(QThread::currentThread() ==
                                                   resource->owner_thread);
                } catch (...) {}
            }
        };
        if (QThread::currentThread() == &thread_) {
            reset();
            return;
        }
        QMetaObject::invokeMethod(&worker_, std::move(reset),
                                  Qt::BlockingQueuedConnection);
    }

    void EraseExpired() {
        resources_.erase(std::remove_if(resources_.begin(), resources_.end(),
                                        [](const auto& resource) {
                                            return resource.expired();
                                        }),
                         resources_.end());
    }

    QObject& worker_;
    QThread& thread_;
    std::mutex mutex_;
    bool running_ = true;
    std::vector<std::weak_ptr<CandidateResource>> resources_;
};

}  // namespace

class NetworkRuntime::PreparedCandidate::Impl final {
   public:
    ~Impl() {
        if (owner) {
            owner->Destroy(resource);
        }
    }

    Preparation preparation;
    std::uint64_t runtime_identity = 0U;
    std::uint64_t runtime_generation = 0U;
    NetworkCacheStorageSlot::Identity storage_identity;
    std::shared_ptr<CandidateOwner> owner;
    std::shared_ptr<CandidateResource> resource;
    bool consumed = false;
};

class NetworkRuntime::Impl final {
   public:
    explicit Impl(Preparation preparation)
        : preparation_(std::move(preparation)),
          storage_slot_(preparation_.cache_directory),
          storage_lease_(storage_slot_.Acquire()),
          runtime_identity_(NextRuntimeIdentity()) {
        if (!preparation_.cache_directory.empty() && !storage_lease_) {
            preparation_.cache_available = false;
            preparation_.diagnostic = NetworkCacheStorage::SetupDiagnostic();
        }
        worker_.moveToThread(&thread_);
        thread_.start();
        candidate_owner_ = std::make_shared<CandidateOwner>(worker_, thread_);
        QMetaObject::invokeMethod(
            &worker_,
            [this]() {
                manager_ = std::make_unique<QNetworkAccessManager>();
                if (storage_lease_) {
                    auto cache = std::make_unique<PreparedNetworkDiskCache>();
                    cache->setMaximumCacheSize(MaximumBytes());
                    cache->setCacheDirectory(
                        QString::fromStdString(storage_lease_.directory()));
                    const bool enabled =
                        preparation_.cache_available &&
                        preparation_.policy.maximum_megabytes != 0U;
                    auto gate = std::make_unique<NetworkDiskCacheGate>(
                        std::move(cache), enabled);
                    cache_gate_ = gate.get();
                    manager_->setCache(gate.release());
                    if (!enabled) {
                        RemoveOwnedDirectory(preparation_);
                    }
                }
            },
            Qt::BlockingQueuedConnection);
    }

    ~Impl() { Shutdown(); }

    HttpResponse Fetch(const HttpRequest& request,
                       const std::function<bool()>& is_cancelled) {
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        if (stopping_.load()) {
            throw HttpError(HttpErrorCode::kCancelled,
                            "HTTP runtime is shutting down");
        }
        HttpResponse result;
        std::exception_ptr failure;
        QMetaObject::invokeMethod(
            &worker_,
            [this, &request, &is_cancelled, &result, &failure]() {
                try {
                    result = FetchHttpWithManager(
                        *manager_, request, [this, &is_cancelled]() {
                            return stopping_.load() ||
                                   (is_cancelled && is_cancelled());
                        });
                } catch (...) {
                    failure = std::current_exception();
                }
            },
            Qt::BlockingQueuedConnection);
        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
        return result;
    }

    bool Activate(Preparation preparation) noexcept {
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        if (stopping_.load() || !storage_lease_ ||
            (preparation.policy.maximum_megabytes != 0U &&
             !preparation.cache_available) ||
            preparation.cache_directory != storage_lease_.directory()) {
            return false;
        }
        bool activated = false;
        QMetaObject::invokeMethod(
            &worker_,
            [this, &preparation, &activated]() {
                if (preparation.policy.maximum_megabytes == 0U) {
                    if (cache_gate_ != nullptr) {
                        cache_gate_->Disable();
                        activated = true;
                        cache_gate_->clear();
                        RemoveOwnedDirectory(preparation);
                    }
                } else {
                    if (cache_gate_ != nullptr) {
                        cache_gate_->PrepareExistingBindingLayout();
                        const bool expiry_deferred =
                            cache_gate_->PublishPositiveMaximum(
                                static_cast<std::int64_t>(
                                    preparation.policy.maximum_megabytes) *
                                kBytesPerMebibyte);
                        activated = true;
                        if (expiry_deferred) {
                            cache_gate_->FinishDeferredExpiry();
                        }
                    }
                }
                if (activated) {
                    preparation_ = std::move(preparation);
                    ++generation_;
                    if (generation_ == 0U) {
                        generation_ = 1U;
                    }
                }
            },
            Qt::BlockingQueuedConnection);
        return activated;
    }

    PreparedCandidate PrepareCandidate(Preparation preparation) {
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        if (stopping_.load() || !storage_lease_ ||
            preparation.cache_directory != storage_lease_.directory() ||
            (preparation.policy.maximum_megabytes != 0U &&
             !preparation.cache_available)) {
            return {};
        }

        auto candidate = std::make_unique<PreparedCandidate::Impl>();
        candidate->preparation = std::move(preparation);
        candidate->runtime_identity = runtime_identity_;
        candidate->runtime_generation = generation_;
        candidate->storage_identity = storage_lease_.identity();
        candidate->owner = candidate_owner_;
        candidate->resource = std::make_shared<CandidateResource>();
        auto resource = candidate->resource;
        const std::uint32_t maximum_megabytes =
            candidate->preparation.policy.maximum_megabytes;
        NetworkDiskCacheGate* const cache_gate = cache_gate_;
        std::exception_ptr failure;
        QMetaObject::invokeMethod(
            &worker_,
            [resource, maximum_megabytes, cache_gate, &failure]() {
                try {
                    resource->construction_thread = QThread::currentThread();
                    resource->owner_thread = QThread::currentThread();
                    if (maximum_megabytes != 0U) {
                        cache_gate->PrepareExistingBindingLayout();
                        resource->maximum_cache_bytes =
                            static_cast<std::int64_t>(maximum_megabytes) *
                            kBytesPerMebibyte;
                    }
                } catch (...) {
                    failure = std::current_exception();
                }
            },
            Qt::BlockingQueuedConnection);
        if (failure) {
            std::rethrow_exception(failure);
        }
        candidate_owner_->Register(resource);
        return PreparedCandidate(std::move(candidate));
    }

    bool CandidateIsCurrent(const PreparedCandidate& candidate) const noexcept {
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        return CandidateIsCurrentLocked(candidate);
    }

    bool ConsumePreparedCandidate(PreparedCandidate& candidate) noexcept {
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        if (!CandidateIsCurrentLocked(candidate)) {
            return false;
        }
        candidate.impl_->consumed = true;
        return true;
    }

    void Shutdown() noexcept {
        if (stopping_.exchange(true)) {
            return;
        }
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        candidate_owner_->Shutdown();
        QMetaObject::invokeMethod(
            &worker_,
            [this]() {
                if (storage_lease_ && manager_ != nullptr &&
                    preparation_.policy.clear_on_exit) {
                    if (cache_gate_ != nullptr) {
                        cache_gate_->Disable();
                        cache_gate_->clear();
                    }
                    RemoveOwnedDirectory(preparation_);
                }
                manager_.reset();
                cache_gate_ = nullptr;
            },
            Qt::BlockingQueuedConnection);
        storage_lease_.Release();
        thread_.quit();
        thread_.wait();
    }

    std::int64_t MaximumBytes() const noexcept {
        return static_cast<std::int64_t>(
                   preparation_.policy.maximum_megabytes) *
               kBytesPerMebibyte;
    }

    void RemoveOwnedDirectory(Preparation& preparation) {
        if (!storage_lease_ ||
            preparation.cache_directory != storage_lease_.directory()) {
            return;
        }
        if (!NetworkCacheStorage::RemoveOwnedDirectory(storage_lease_)) {
            preparation.diagnostic = NetworkCacheStorage::CleanupDiagnostic();
            qWarning().noquote() << QString::fromLatin1(
                NetworkCacheStorage::CleanupDiagnostic());
        }
    }

    bool CandidateIsCurrentLocked(
        const PreparedCandidate& candidate) const noexcept {
        return !stopping_.load() && candidate.impl_ &&
               !candidate.impl_->consumed &&
               candidate.impl_->runtime_identity == runtime_identity_ &&
               candidate.impl_->runtime_generation == generation_ &&
               candidate.impl_->storage_identity == storage_lease_.identity();
    }

    Preparation preparation_;
    NetworkCacheStorageSlot storage_slot_;
    NetworkCacheStorageSlot::Lease storage_lease_;
    QObject worker_;
    QThread thread_;
    std::unique_ptr<QNetworkAccessManager> manager_;
    NetworkDiskCacheGate* cache_gate_ = nullptr;
    mutable std::mutex fetch_mutex_;
    std::atomic<bool> stopping_{false};
    std::shared_ptr<CandidateOwner> candidate_owner_;
    const std::uint64_t runtime_identity_;
    std::uint64_t generation_ = 1U;
};

NetworkRuntime::Preparation NetworkRuntime::Prepare(
    NetworkCachePolicy policy, const std::string& cache_root) {
    if (policy.maximum_megabytes > 10240U) {
        throw std::invalid_argument("Network cache size is outside its bounds");
    }
    auto storage = NetworkCacheStorage::Prepare(cache_root,
                                                policy.maximum_megabytes != 0U);
    return {policy, std::move(storage.directory), storage.available,
            std::move(storage.diagnostic)};
}

std::shared_ptr<NetworkRuntime> NetworkRuntime::Create(
    Preparation preparation) {
    return std::shared_ptr<NetworkRuntime>(
        new NetworkRuntime(std::move(preparation)));
}

NetworkRuntime::NetworkRuntime(Preparation preparation)
    : impl_(std::make_unique<Impl>(std::move(preparation))) {}

NetworkRuntime::~NetworkRuntime() = default;

NetworkRuntime::PreparedCandidate::PreparedCandidate() = default;

NetworkRuntime::PreparedCandidate::PreparedCandidate(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

NetworkRuntime::PreparedCandidate::~PreparedCandidate() = default;

NetworkRuntime::PreparedCandidate::PreparedCandidate(
    PreparedCandidate&&) noexcept = default;

NetworkRuntime::PreparedCandidate& NetworkRuntime::PreparedCandidate::operator=(
    PreparedCandidate&&) noexcept = default;

NetworkRuntime::PreparedCandidate::operator bool() const noexcept {
    return impl_ && !impl_->consumed;
}

HttpResponse NetworkRuntime::Fetch(const HttpRequest& request,
                                   const std::function<bool()>& is_cancelled) {
    return impl_->Fetch(request, is_cancelled);
}

bool NetworkRuntime::Activate(Preparation preparation) noexcept {
    return impl_->Activate(std::move(preparation));
}

NetworkRuntime::PreparedCandidate NetworkRuntime::PrepareCandidate(
    Preparation preparation) {
    return impl_->PrepareCandidate(std::move(preparation));
}

bool NetworkRuntimeTestAccess::IsCurrent(
    const NetworkRuntime& runtime,
    const NetworkRuntime::PreparedCandidate& candidate) {
    return runtime.impl_->CandidateIsCurrent(candidate);
}

bool NetworkRuntimeTestAccess::Consume(
    NetworkRuntime& runtime, NetworkRuntime::PreparedCandidate& candidate) {
    return runtime.impl_->ConsumePreparedCandidate(candidate);
}

std::int64_t NetworkRuntimeTestAccess::MaximumCacheBytes(
    const NetworkRuntime::PreparedCandidate& candidate) {
    if (!candidate.impl_ || !candidate.impl_->resource) {
        return 0;
    }
    return candidate.impl_->resource->maximum_cache_bytes;
}

std::string NetworkRuntimeTestAccess::CacheDirectory(
    const NetworkRuntime::PreparedCandidate& candidate) {
    if (!candidate.impl_ || !candidate.impl_->resource) {
        return {};
    }
    return candidate.impl_->resource->cache_directory;
}

bool NetworkRuntimeTestAccess::WasConstructedOnOwnerThread(
    const NetworkRuntime& runtime,
    const NetworkRuntime::PreparedCandidate& candidate) {
    return candidate.impl_ && candidate.impl_->resource &&
           candidate.impl_->resource->construction_thread ==
               &runtime.impl_->thread_;
}

void NetworkRuntimeTestAccess::ObserveDestruction(
    NetworkRuntime::PreparedCandidate& candidate,
    std::function<void(bool)> observer) {
    if (candidate.impl_ && candidate.impl_->resource) {
        candidate.impl_->resource->destruction_observer = std::move(observer);
    }
}

const void* NetworkRuntimeTestAccess::BoundDiskCache(
    const NetworkRuntime& runtime) {
    return runtime.impl_->cache_gate_ == nullptr
               ? nullptr
               : &runtime.impl_->cache_gate_->disk_cache();
}

std::uint64_t NetworkRuntimeTestAccess::DirectoryConfigurationCount(
    const NetworkRuntime& runtime) {
    return runtime.impl_->cache_gate_ == nullptr
               ? 0U
               : runtime.impl_->cache_gate_->directory_configuration_count();
}

void NetworkRuntime::Shutdown() noexcept {
    impl_->Shutdown();
}

std::int64_t NetworkRuntime::maximum_cache_bytes() const noexcept {
    return impl_->MaximumBytes();
}

const std::string& NetworkRuntime::cache_directory() const noexcept {
    return impl_->preparation_.cache_directory;
}

const std::string& NetworkRuntime::diagnostic() const noexcept {
    return impl_->preparation_.diagnostic;
}

}  // namespace goldendict::network
