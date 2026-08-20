// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/network/network_runtime.h"

#include "http_client.h"
#include "network_cache_storage.h"
#include "network_runtime_test_access.h"

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
   protected:
    qint64 expire() override {
        if (cacheDirectory().isEmpty()) {
            return 0;
        }
        return QNetworkDiskCache::expire();
    }
};

struct CandidateResource final {
    std::unique_ptr<QNetworkDiskCache> cache;
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
            resource->cache.reset();
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
                if (storage_lease_ && preparation_.cache_available &&
                    preparation_.policy.maximum_megabytes != 0U) {
                    auto cache = std::make_unique<QNetworkDiskCache>();
                    cache->setCacheDirectory(
                        QString::fromStdString(storage_lease_.directory()));
                    cache->setMaximumCacheSize(MaximumBytes());
                    manager_->setCache(cache.release());
                } else if (storage_lease_ &&
                           preparation_.policy.maximum_megabytes == 0U) {
                    RemoveOwnedDirectory(preparation_);
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
                    if (manager_->cache() != nullptr) {
                        manager_->cache()->clear();
                        manager_->setCache(nullptr);
                    }
                    RemoveOwnedDirectory(preparation);
                    activated = true;
                } else {
                    auto* cache =
                        qobject_cast<QNetworkDiskCache*>(manager_->cache());
                    if (cache == nullptr) {
                        auto replacement =
                            std::make_unique<QNetworkDiskCache>();
                        replacement->setCacheDirectory(
                            QString::fromStdString(storage_lease_.directory()));
                        cache = replacement.release();
                        manager_->setCache(cache);
                    }
                    cache->setMaximumCacheSize(
                        static_cast<std::int64_t>(
                            preparation.policy.maximum_megabytes) *
                        kBytesPerMebibyte);
                    activated = true;
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
        std::exception_ptr failure;
        QMetaObject::invokeMethod(
            &worker_,
            [resource, maximum_megabytes, &failure]() {
                try {
                    resource->construction_thread = QThread::currentThread();
                    resource->owner_thread = QThread::currentThread();
                    if (maximum_megabytes != 0U) {
                        resource->cache =
                            std::make_unique<PreparedNetworkDiskCache>();
                        resource->maximum_cache_bytes =
                            static_cast<std::int64_t>(maximum_megabytes) *
                            kBytesPerMebibyte;
                        resource->cache->setMaximumCacheSize(
                            resource->maximum_cache_bytes);
                        resource->cache_directory =
                            resource->cache->cacheDirectory().toStdString();
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
                    if (manager_->cache() != nullptr) {
                        manager_->cache()->clear();
                    }
                    RemoveOwnedDirectory(preparation_);
                }
                manager_.reset();
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
    if (!candidate.impl_ || !candidate.impl_->resource ||
        !candidate.impl_->resource->cache) {
        return 0;
    }
    return candidate.impl_->resource->maximum_cache_bytes;
}

std::string NetworkRuntimeTestAccess::CacheDirectory(
    const NetworkRuntime::PreparedCandidate& candidate) {
    if (!candidate.impl_ || !candidate.impl_->resource ||
        !candidate.impl_->resource->cache) {
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
