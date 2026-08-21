// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/network/network_runtime.h"

#include "http_client.h"
#include "network_cache_storage.h"
#include "network_runtime_test_access.h"
#include "network_runtime_transaction.h"

#include <QAbstractNetworkCache>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <condition_variable>
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
    enum class State {
        kPreparing,
        kReady,
        kCommitRequested,
        kAbortRequested,
        kPublished,
        kPostWorkComplete,
        kFailedBeforePublication,
        kTerminal,
    };

    mutable std::mutex mutex;
    std::condition_variable changed;
    State state = State::kPreparing;
    NetworkRuntime::CommitResult result =
        NetworkRuntime::CommitResult::kRejected;
    QThread* construction_thread = nullptr;
    QThread* owner_thread = nullptr;
    std::int64_t maximum_cache_bytes = 0;
    std::string cache_directory;
    std::string cleanup_diagnostic;
    std::function<void(CandidateResource&)> publish;
    std::function<void(CandidateResource&)> post_work;
    std::function<void()> publication_observer;
    std::function<void(bool)> destruction_observer;
    bool force_post_work_failure = false;
    bool expiry_deferred = false;
};

class CandidateDispatcher final {
   public:
    CandidateDispatcher(QObject& worker, QThread& thread)
        : worker_(&worker), thread_(&thread) {}

    void StartOnOwnerThread() {
        timer_ = std::make_unique<QTimer>();
        timer_creation_count_.fetch_add(1U);
        QObject::connect(timer_.get(), &QTimer::timeout, worker_,
                         [this]() { ProcessReadyCommands(); });
        timer_connection_count_.fetch_add(1U);
    }

    void RegisterOnOwnerThread(
        const std::shared_ptr<CandidateResource>& resource) {
        const bool was_empty = resources_.empty();
        resources_.push_back(resource);
        if (was_empty) {
            timer_->start(1);
            timer_active_.store(true);
            timer_start_count_.fetch_add(1U);
        }
    }

    void Withdraw(const std::shared_ptr<CandidateResource>& resource) noexcept {
        if (!resource) {
            return;
        }
        {
            std::lock_guard<std::mutex> state_lock(resource->mutex);
            if (resource->state == CandidateResource::State::kTerminal) {
                return;
            }
        }
        if (QThread::currentThread() == thread_) {
            AbortOnOwnerThread(resource);
            return;
        }
        std::unique_lock<std::mutex> lock(resource->mutex);
        if (resource->state != CandidateResource::State::kPublished &&
            resource->state != CandidateResource::State::kPostWorkComplete) {
            resource->state = CandidateResource::State::kAbortRequested;
        }
        resource->changed.wait(lock, [&resource]() {
            return resource->state == CandidateResource::State::kTerminal;
        });
    }

    NetworkRuntime::CommitResult Commit(
        const std::shared_ptr<CandidateResource>& resource) noexcept {
        {
            std::lock_guard<std::mutex> lock(resource->mutex);
            if (resource->state != CandidateResource::State::kReady) {
                return NetworkRuntime::CommitResult::kRejected;
            }
            resource->state = CandidateResource::State::kCommitRequested;
        }
        if (QThread::currentThread() == thread_) {
            ProcessOne(resource);
        } else {
            std::unique_lock<std::mutex> lock(resource->mutex);
            resource->changed.wait(lock, [&resource]() {
                return resource->state == CandidateResource::State::kTerminal;
            });
        }
        std::lock_guard<std::mutex> lock(resource->mutex);
        return resource->result;
    }

    void PublishOnly(const std::shared_ptr<CandidateResource>& resource) {
        {
            std::lock_guard<std::mutex> lock(resource->mutex);
            if (resource->state != CandidateResource::State::kReady)
                std::terminate();
            resource->state = CandidateResource::State::kCommitRequested;
        }
        InvokeOnOwner(resource, false);
        std::lock_guard<std::mutex> lock(resource->mutex);
        if (resource->state != CandidateResource::State::kPublished)
            std::terminate();
    }

    NetworkRuntime::CommitResult FinishPublished(
        const std::shared_ptr<CandidateResource>& resource) noexcept {
        InvokeOnOwner(resource, true);
        std::lock_guard<std::mutex> lock(resource->mutex);
        return resource->result;
    }

    void ShutdownOnOwnerThread() noexcept {
        stopping_.store(true);
        const auto resources = resources_;
        for (const auto& resource : resources) {
            AbortOnOwnerThread(resource);
        }
        resources_.clear();
        timer_->stop();
        timer_active_.store(false);
        timer_.reset();
        stopped_.store(true);
    }

    bool stopping() const noexcept { return stopping_.load(); }

    bool stopped() const noexcept { return stopped_.load(); }

    bool timer_active() const noexcept { return timer_active_.load(); }

    std::uint64_t timer_creation_count() const noexcept {
        return timer_creation_count_.load();
    }

    std::uint64_t timer_connection_count() const noexcept {
        return timer_connection_count_.load();
    }

    std::uint64_t timer_start_count() const noexcept {
        return timer_start_count_.load();
    }

    std::uint64_t timer_wakeup_count() const noexcept {
        return timer_wakeup_count_.load();
    }

   private:
    void InvokeOnOwner(const std::shared_ptr<CandidateResource>& resource,
                       bool finish) noexcept {
        if (QThread::currentThread() == thread_) {
            if (finish)
                FinishOne(resource);
            else
                PublishOne(resource);
            return;
        }
        QMetaObject::invokeMethod(
            worker_,
            [this, resource, finish]() {
                if (finish)
                    FinishOne(resource);
                else
                    PublishOne(resource);
            },
            Qt::BlockingQueuedConnection);
    }

    void PublishOne(const std::shared_ptr<CandidateResource>& resource) {
        try {
            resource->publish(*resource);
        } catch (...) {
            std::terminate();
        }
    }

    void FinishOne(
        const std::shared_ptr<CandidateResource>& resource) noexcept {
        try {
            resource->post_work(*resource);
        } catch (...) {
            std::lock_guard<std::mutex> lock(resource->mutex);
            resource->result =
                NetworkRuntime::CommitResult::kPublishedWithPostWorkFailure;
        }
        RetireAndExposeTerminal(resource);
    }

    void AbortOnOwnerThread(
        const std::shared_ptr<CandidateResource>& resource) noexcept {
        std::function<void(bool)> observer;
        {
            std::lock_guard<std::mutex> lock(resource->mutex);
            if (resource->state == CandidateResource::State::kTerminal ||
                resource->state == CandidateResource::State::kPublished ||
                resource->state ==
                    CandidateResource::State::kPostWorkComplete) {
                return;
            }
            resource->state = CandidateResource::State::kAbortRequested;
            observer = std::move(resource->destruction_observer);
        }
        if (observer) {
            try {
                observer(QThread::currentThread() == resource->owner_thread);
            } catch (...) {}
        }
        RetireAndExposeTerminal(resource);
    }

    void ProcessOne(const std::shared_ptr<CandidateResource>& resource) {
        {
            std::lock_guard<std::mutex> lock(resource->mutex);
            if (resource->state == CandidateResource::State::kAbortRequested) {
                // Finalized below without executing prepared publication.
            } else if (resource->state !=
                       CandidateResource::State::kCommitRequested) {
                return;
            }
        }
        bool abort = false;
        {
            std::lock_guard<std::mutex> lock(resource->mutex);
            abort =
                resource->state == CandidateResource::State::kAbortRequested;
        }
        if (abort) {
            AbortOnOwnerThread(resource);
            return;
        }
        try {
            resource->publish(*resource);
            resource->post_work(*resource);
        } catch (...) {
            std::lock_guard<std::mutex> lock(resource->mutex);
            if (resource->state == CandidateResource::State::kPublished ||
                resource->state ==
                    CandidateResource::State::kPostWorkComplete) {
                resource->state = CandidateResource::State::kPostWorkComplete;
                resource->result =
                    NetworkRuntime::CommitResult::kPublishedWithPostWorkFailure;
            } else {
                resource->state =
                    CandidateResource::State::kFailedBeforePublication;
                resource->result = NetworkRuntime::CommitResult::kRejected;
            }
        }
        RetireAndExposeTerminal(resource);
    }

    void ProcessReadyCommands() {
        timer_wakeup_count_.fetch_add(1U);
        const auto resources = resources_;
        for (const auto& resource : resources) {
            ProcessOne(resource);
        }
    }

    void RetireAndExposeTerminal(
        const std::shared_ptr<CandidateResource>& resource) noexcept {
        resources_.erase(std::remove(resources_.begin(), resources_.end(),
                                     resource),
                         resources_.end());
        if (resources_.empty() && timer_ && timer_->isActive()) {
            timer_->stop();
            timer_active_.store(false);
        }
        {
            std::lock_guard<std::mutex> lock(resource->mutex);
            resource->state = CandidateResource::State::kTerminal;
        }
        resource->changed.notify_all();
    }

    QObject* worker_ = nullptr;
    QThread* thread_ = nullptr;
    std::unique_ptr<QTimer> timer_;
    std::vector<std::shared_ptr<CandidateResource>> resources_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<bool> timer_active_{false};
    std::atomic<std::uint64_t> timer_creation_count_{0U};
    std::atomic<std::uint64_t> timer_connection_count_{0U};
    std::atomic<std::uint64_t> timer_start_count_{0U};
    std::atomic<std::uint64_t> timer_wakeup_count_{0U};
};

}  // namespace

class NetworkRuntime::PreparedCandidate::Impl final {
   public:
    ~Impl() {
        if (dispatcher) {
            dispatcher->Withdraw(resource);
        }
    }

    Preparation preparation;
    std::uint64_t runtime_identity = 0U;
    std::uint64_t runtime_generation = 0U;
    NetworkCacheStorageSlot::Identity storage_identity;
    std::shared_ptr<CandidateDispatcher> dispatcher;
    std::shared_ptr<CandidateResource> resource;
    bool consumed = false;
};

class NetworkRuntime::CommitReservation::Impl final {
   public:
    NetworkRuntime::Impl* owner = nullptr;
    std::unique_ptr<PreparedCandidate::Impl> candidate;
    std::uint64_t identity = 0U;
};

class NetworkRuntimeTransaction::Published::Impl final {
   public:
    NetworkRuntime::Impl* owner = nullptr;
    std::uint64_t identity = 0U;
    std::unique_ptr<NetworkRuntime::PreparedCandidate::Impl> candidate;
};

class NetworkRuntime::Impl final {
   public:
    friend class NetworkRuntimeTransaction;

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
        candidate_dispatcher_ =
            std::make_shared<CandidateDispatcher>(worker_, thread_);
        QMetaObject::invokeMethod(
            &worker_,
            [this]() {
                candidate_dispatcher_->StartOnOwnerThread();
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

    ~Impl() {
        if (reservation_active_.load())
            std::terminate();
        Shutdown();
    }

    HttpResponse Fetch(const HttpRequest& request,
                       const std::function<bool()>& is_cancelled) {
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        if (stopping_.load() || reservation_active_.load()) {
            throw HttpError(HttpErrorCode::kCancelled,
                            "HTTP runtime is unavailable for a transaction");
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
        try {
            auto candidate = PrepareCandidate(std::move(preparation));
            if (!candidate) {
                return false;
            }
            const auto result = CommitPreparedCandidate(candidate);
            return result == NetworkRuntime::CommitResult::kPublished ||
                   result == NetworkRuntime::CommitResult::
                                 kPublishedWithPostWorkFailure;
        } catch (...) {
            return false;
        }
    }

    PreparedCandidate PrepareCandidate(Preparation preparation) {
        std::lock_guard<std::mutex> fetch_lock(fetch_mutex_);
        if (stopping_.load() || reservation_active_.load() || !storage_lease_ ||
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
        candidate->dispatcher = candidate_dispatcher_;
        candidate->resource = std::make_shared<CandidateResource>();
        auto resource = candidate->resource;
        resource->cleanup_diagnostic = NetworkCacheStorage::CleanupDiagnostic();
        const std::uint32_t maximum_megabytes =
            candidate->preparation.policy.maximum_megabytes;
        NetworkDiskCacheGate* const cache_gate = cache_gate_;
        resource->publish = [this, candidate_impl = candidate.get()](
                                CandidateResource& command) {
            if (stopping_.load()) {
                std::lock_guard<std::mutex> lock(command.mutex);
                command.state =
                    CandidateResource::State::kFailedBeforePublication;
                command.result = NetworkRuntime::CommitResult::kRejected;
                return;
            }
            const bool positive = command.maximum_cache_bytes != 0;
            if (positive) {
                command.expiry_deferred = cache_gate_->PublishPositiveMaximum(
                    command.maximum_cache_bytes);
            } else {
                cache_gate_->Disable();
            }

            preparation_ = std::move(candidate_impl->preparation);
            ++generation_;
            if (generation_ == 0U) {
                generation_ = 1U;
            }
            {
                std::lock_guard<std::mutex> lock(command.mutex);
                command.state = CandidateResource::State::kPublished;
                command.result = NetworkRuntime::CommitResult::kPublished;
            }
            if (command.publication_observer)
                command.publication_observer();
        };
        resource->post_work = [this](CandidateResource& command) {
            const bool positive = command.maximum_cache_bytes != 0;
            bool post_work_failed = false;
            try {
                if (positive) {
                    if (command.expiry_deferred) {
                        cache_gate_->FinishDeferredExpiry();
                    }
                } else {
                    cache_gate_->clear();
                    if (!NetworkCacheStorage::RemoveOwnedDirectory(
                            storage_lease_)) {
                        post_work_failed = true;
                    }
                }
            } catch (...) {
                post_work_failed = true;
            }
            post_work_failed =
                post_work_failed || command.force_post_work_failure;
            if (post_work_failed) {
                preparation_.diagnostic = std::move(command.cleanup_diagnostic);
                qWarning().noquote() << QString::fromLatin1(
                    NetworkCacheStorage::CleanupDiagnostic());
            }
            {
                std::lock_guard<std::mutex> lock(command.mutex);
                command.state = CandidateResource::State::kPostWorkComplete;
                command.result = post_work_failed
                                     ? NetworkRuntime::CommitResult::
                                           kPublishedWithPostWorkFailure
                                     : NetworkRuntime::CommitResult::kPublished;
            }
        };
        std::exception_ptr failure;
        const bool posted = QMetaObject::invokeMethod(
            &worker_,
            [resource, maximum_megabytes, cache_gate,
             dispatcher = candidate_dispatcher_, &failure]() {
                try {
                    resource->construction_thread = QThread::currentThread();
                    resource->owner_thread = QThread::currentThread();
                    if (maximum_megabytes != 0U) {
                        cache_gate->PrepareExistingBindingLayout();
                        resource->maximum_cache_bytes =
                            static_cast<std::int64_t>(maximum_megabytes) *
                            kBytesPerMebibyte;
                    }
                    dispatcher->RegisterOnOwnerThread(resource);
                    {
                        std::lock_guard<std::mutex> lock(resource->mutex);
                        resource->state = CandidateResource::State::kReady;
                    }
                    resource->changed.notify_all();
                } catch (...) {
                    failure = std::current_exception();
                    {
                        std::lock_guard<std::mutex> lock(resource->mutex);
                        resource->state = CandidateResource::State::kTerminal;
                    }
                    resource->changed.notify_all();
                }
            },
            Qt::BlockingQueuedConnection);
        if (!posted) {
            throw std::runtime_error(
                "Failed to post prepared network commit command");
        }
        if (failure) {
            std::rethrow_exception(failure);
        }
        return PreparedCandidate(std::move(candidate));
    }

    NetworkRuntime::CommitResult CommitPreparedCandidate(
        PreparedCandidate& candidate) noexcept {
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        if (!CandidateIsCurrentLocked(candidate)) {
            return NetworkRuntime::CommitResult::kRejected;
        }
        candidate.impl_->consumed = true;
        return candidate_dispatcher_->Commit(candidate.impl_->resource);
    }

    CommitReservation Reserve(PreparedCandidate& candidate) {
        auto reservation = std::make_unique<CommitReservation::Impl>();
        std::lock_guard<std::mutex> reservation_lock(reservation_mutex_);
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        if (reservation_active_.load() || !CandidateIsCurrentLocked(candidate))
            return {};
        candidate.impl_->consumed = true;
        reservation_active_.store(true);
        ++reservation_identity_;
        if (reservation_identity_ == 0U)
            reservation_identity_ = 1U;
        reservation->owner = this;
        reservation->identity = reservation_identity_;
        reservation->candidate = std::move(candidate.impl_);
        return CommitReservation(std::move(reservation));
    }

    void Abort(CommitReservation& reservation) noexcept {
        if (!reservation.impl_)
            return;
        std::lock_guard<std::mutex> reservation_lock(reservation_mutex_);
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        if (reservation.impl_->owner != this || !reservation_active_.load() ||
            reservation.impl_->identity != reservation_identity_) {
            std::terminate();
        }
        reservation_active_.store(false);
        reservation.impl_.reset();
    }

    NetworkRuntime::CommitResult Publish(
        CommitReservation& reservation) noexcept {
        std::lock_guard<std::mutex> reservation_lock(reservation_mutex_);
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        if (!reservation.impl_ || reservation.impl_->owner != this ||
            !reservation_active_.load() ||
            reservation.impl_->identity != reservation_identity_ ||
            !reservation.impl_->candidate) {
            std::terminate();
        }
        auto result = candidate_dispatcher_->Commit(
            reservation.impl_->candidate->resource);
        if (result == NetworkRuntime::CommitResult::kRejected)
            std::terminate();
        reservation_active_.store(false);
        reservation.impl_.reset();
        return result;
    }

    bool CandidateIsCurrent(const PreparedCandidate& candidate) const noexcept {
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        return CandidateIsCurrentLocked(candidate);
    }

    bool ConsumePreparedCandidate(PreparedCandidate& candidate) noexcept {
        return CommitPreparedCandidate(candidate) !=
               NetworkRuntime::CommitResult::kRejected;
    }

    void Shutdown() noexcept {
        {
            std::lock_guard<std::mutex> reservation_lock(reservation_mutex_);
            if (reservation_active_.load() || stopping_.exchange(true))
                return;
        }
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        QMetaObject::invokeMethod(
            &worker_,
            [this]() {
                candidate_dispatcher_->ShutdownOnOwnerThread();
                if (storage_lease_ && manager_ != nullptr &&
                    preparation_.policy.clear_on_exit) {
                    if (cache_gate_ != nullptr) {
                        cache_gate_->Disable();
                        cache_gate_->clear();
                    }
                }
                manager_.reset();
                cache_gate_ = nullptr;
                if (manager_cache_destruction_observer_) {
                    manager_cache_destruction_observer_();
                }
            },
            Qt::BlockingQueuedConnection);
        thread_.quit();
        thread_.wait();
        if (storage_lease_ && preparation_.policy.clear_on_exit) {
            RemoveOwnedDirectory(preparation_);
        }
        storage_lease_.Release();
    }

    std::int64_t MaximumBytes() const noexcept {
        return static_cast<std::int64_t>(
                   preparation_.policy.maximum_megabytes) *
               kBytesPerMebibyte;
    }

    void RemoveOwnedDirectory(Preparation& preparation) {
        if (!storage_lease_) {
            return;
        }
        if (directory_cleanup_observer_) {
            directory_cleanup_observer_();
        }
        if (!NetworkCacheStorage::RemoveOwnedDirectory(storage_lease_)) {
            preparation.diagnostic = NetworkCacheStorage::CleanupDiagnostic();
            qWarning().noquote() << QString::fromLatin1(
                NetworkCacheStorage::CleanupDiagnostic());
        }
    }

    bool CandidateIsCurrentLocked(
        const PreparedCandidate& candidate) const noexcept {
        if (reservation_active_.load())
            return false;
        if (!candidate.impl_ || !candidate.impl_->resource) {
            return false;
        }
        if (candidate.impl_->runtime_identity != runtime_identity_) {
            return false;
        }
        if (candidate.impl_->consumed ||
            !CandidateReady(*candidate.impl_->resource)) {
            return false;
        }
        if (stopping_.load()) {
            return false;
        }
        if (candidate.impl_->runtime_generation != generation_) {
            return false;
        }
        return storage_lease_.Matches(candidate.impl_->storage_identity);
    }

    static bool CandidateReady(const CandidateResource& resource) noexcept {
        std::lock_guard<std::mutex> lock(resource.mutex);
        return resource.state == CandidateResource::State::kReady;
    }

    Preparation preparation_;
    NetworkCacheStorageSlot storage_slot_;
    NetworkCacheStorageSlot::Lease storage_lease_;
    QObject worker_;
    QThread thread_;
    std::unique_ptr<QNetworkAccessManager> manager_;
    NetworkDiskCacheGate* cache_gate_ = nullptr;
    std::function<void()> manager_cache_destruction_observer_;
    std::function<void()> directory_cleanup_observer_;
    mutable std::mutex fetch_mutex_;
    mutable std::mutex reservation_mutex_;
    std::atomic<bool> stopping_{false};
    std::shared_ptr<CandidateDispatcher> candidate_dispatcher_;
    const std::uint64_t runtime_identity_;
    std::uint64_t generation_ = 1U;
    std::atomic<bool> reservation_active_{false};
    std::uint64_t reservation_identity_ = 0U;
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
    if (!impl_ || impl_->consumed || !impl_->resource) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->resource->mutex);
    return impl_->resource->state == CandidateResource::State::kReady;
}

NetworkRuntime::CommitReservation::CommitReservation() = default;

NetworkRuntime::CommitReservation::CommitReservation(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

NetworkRuntime::CommitReservation::~CommitReservation() {
    if (impl_ != nullptr && impl_->owner != nullptr)
        impl_->owner->Abort(*this);
}

NetworkRuntime::CommitReservation::CommitReservation(
    CommitReservation&&) noexcept = default;

NetworkRuntime::CommitReservation& NetworkRuntime::CommitReservation::operator=(
    CommitReservation&& other) noexcept {
    if (this != &other && impl_ != nullptr && impl_->owner != nullptr)
        impl_->owner->Abort(*this);
    impl_ = std::move(other.impl_);
    return *this;
}

NetworkRuntime::CommitReservation::operator bool() const noexcept {
    return impl_ != nullptr;
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

NetworkRuntime::CommitResult NetworkRuntime::Commit(
    PreparedCandidate& candidate) noexcept {
    return impl_->CommitPreparedCandidate(candidate);
}

NetworkRuntime::CommitReservation NetworkRuntime::Reserve(
    PreparedCandidate& candidate) {
    return impl_->Reserve(candidate);
}

void NetworkRuntime::Abort(CommitReservation& reservation) noexcept {
    impl_->Abort(reservation);
}

NetworkRuntime::CommitResult NetworkRuntime::Publish(
    CommitReservation& reservation) noexcept {
    return impl_->Publish(reservation);
}

NetworkRuntimeTransaction::Published::Published() noexcept = default;

NetworkRuntimeTransaction::Published::Published(
    std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

NetworkRuntimeTransaction::Published::~Published() {
    if (impl_)
        std::terminate();
}

NetworkRuntimeTransaction::Published::Published(Published&&) noexcept = default;

NetworkRuntimeTransaction::Published&
NetworkRuntimeTransaction::Published::operator=(Published&& other) noexcept {
    if (this != &other && impl_)
        std::terminate();
    impl_ = std::move(other.impl_);
    return *this;
}

NetworkRuntimeTransaction::Published::operator bool() const noexcept {
    return impl_ != nullptr;
}

NetworkRuntimeTransaction::Published NetworkRuntimeTransaction::Publish(
    NetworkRuntime& runtime,
    NetworkRuntime::CommitReservation& reservation) noexcept {
    auto& owner = *runtime.impl_;
    std::lock_guard<std::mutex> reservation_lock(owner.reservation_mutex_);
    std::lock_guard<std::mutex> fetch_lock(owner.fetch_mutex_);
    if (!reservation.impl_ || reservation.impl_->owner != &owner ||
        !owner.reservation_active_.load() ||
        reservation.impl_->identity != owner.reservation_identity_ ||
        !reservation.impl_->candidate) {
        std::terminate();
    }
    owner.candidate_dispatcher_->PublishOnly(
        reservation.impl_->candidate->resource);
    auto published = std::make_unique<Published::Impl>();
    published->owner = &owner;
    published->identity = reservation.impl_->identity;
    published->candidate = std::move(reservation.impl_->candidate);
    reservation.impl_.reset();
    return Published(std::move(published));
}

NetworkRuntime::CommitResult NetworkRuntimeTransaction::Finish(
    NetworkRuntime& runtime, Published& published) noexcept {
    auto& owner = *runtime.impl_;
    std::lock_guard<std::mutex> reservation_lock(owner.reservation_mutex_);
    std::lock_guard<std::mutex> fetch_lock(owner.fetch_mutex_);
    if (!published.impl_ || published.impl_->owner != &owner ||
        !owner.reservation_active_.load() ||
        published.impl_->identity != owner.reservation_identity_ ||
        !published.impl_->candidate) {
        std::terminate();
    }
    const auto result = owner.candidate_dispatcher_->FinishPublished(
        published.impl_->candidate->resource);
    owner.reservation_active_.store(false);
    published.impl_.reset();
    return result;
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

void NetworkRuntimeTestAccess::ObservePublication(
    NetworkRuntime::PreparedCandidate& candidate,
    std::function<void()> observer) {
    if (candidate.impl_ && candidate.impl_->resource) {
        candidate.impl_->resource->publication_observer = std::move(observer);
    }
}

bool NetworkRuntimeTestAccess::DestroyOnOwnerThread(
    NetworkRuntime& runtime, NetworkRuntime::PreparedCandidate& candidate) {
    bool completed = false;
    QMetaObject::invokeMethod(
        &runtime.impl_->worker_,
        [&candidate, &completed]() {
            candidate = NetworkRuntime::PreparedCandidate{};
            completed = true;
        },
        Qt::BlockingQueuedConnection);
    return completed;
}

NetworkRuntime::CommitResult NetworkRuntimeTestAccess::CommitOnOwnerThread(
    NetworkRuntime& runtime, NetworkRuntime::PreparedCandidate& candidate) {
    auto result = NetworkRuntime::CommitResult::kRejected;
    QMetaObject::invokeMethod(
        &runtime.impl_->worker_,
        [&runtime, &candidate, &result]() {
            result = runtime.Commit(candidate);
        },
        Qt::BlockingQueuedConnection);
    return result;
}

void NetworkRuntimeTestAccess::MakeUnready(
    NetworkRuntime::PreparedCandidate& candidate) {
    if (candidate.impl_ && candidate.impl_->resource) {
        std::lock_guard<std::mutex> lock(candidate.impl_->resource->mutex);
        candidate.impl_->resource->state = CandidateResource::State::kPreparing;
    }
}

void NetworkRuntimeTestAccess::InvalidateLeaseIdentity(
    NetworkRuntime::PreparedCandidate& candidate) {
    if (candidate.impl_) {
        candidate.impl_->storage_identity.generation = 0U;
    }
}

void NetworkRuntimeTestAccess::ForcePostWorkFailure(
    NetworkRuntime::PreparedCandidate& candidate) {
    if (candidate.impl_ && candidate.impl_->resource) {
        candidate.impl_->resource->force_post_work_failure = true;
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

void NetworkRuntimeTestAccess::ObserveManagerCacheDestruction(
    NetworkRuntime& runtime, std::function<void()> observer) {
    runtime.impl_->manager_cache_destruction_observer_ = std::move(observer);
}

void NetworkRuntimeTestAccess::ObserveDirectoryCleanup(
    NetworkRuntime& runtime, std::function<void()> observer) {
    runtime.impl_->directory_cleanup_observer_ = std::move(observer);
}

bool NetworkRuntimeTestAccess::DispatcherTimerActive(
    const NetworkRuntime& runtime) {
    return runtime.impl_->candidate_dispatcher_->timer_active();
}

std::uint64_t NetworkRuntimeTestAccess::DispatcherTimerCreationCount(
    const NetworkRuntime& runtime) {
    return runtime.impl_->candidate_dispatcher_->timer_creation_count();
}

std::uint64_t NetworkRuntimeTestAccess::DispatcherTimerConnectionCount(
    const NetworkRuntime& runtime) {
    return runtime.impl_->candidate_dispatcher_->timer_connection_count();
}

std::uint64_t NetworkRuntimeTestAccess::DispatcherTimerStartCount(
    const NetworkRuntime& runtime) {
    return runtime.impl_->candidate_dispatcher_->timer_start_count();
}

std::uint64_t NetworkRuntimeTestAccess::DispatcherTimerWakeupCount(
    const NetworkRuntime& runtime) {
    return runtime.impl_->candidate_dispatcher_->timer_wakeup_count();
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
