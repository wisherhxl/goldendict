// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/network/network_runtime.h"

#include "http_client.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QThread>

#include <atomic>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace goldendict::network {
namespace {

constexpr std::int64_t kBytesPerMebibyte = 1024LL * 1024LL;
constexpr char kOwnedDirectory[] = "qt-network-http";
constexpr char kSetupDiagnostic[] =
    "Qt Network cache setup failed; HTTP traffic will remain uncached";
constexpr char kCleanupDiagnostic[] =
    "Qt Network cache cleanup failed; cached data may remain";

}  // namespace

class NetworkRuntime::Impl final {
   public:
    explicit Impl(Preparation preparation)
        : preparation_(std::move(preparation)) {
        worker_.moveToThread(&thread_);
        thread_.start();
        QMetaObject::invokeMethod(
            &worker_,
            [this]() {
                manager_ = std::make_unique<QNetworkAccessManager>();
                if (preparation_.cache_available &&
                    preparation_.policy.maximum_megabytes != 0U) {
                    auto cache = std::make_unique<QNetworkDiskCache>();
                    cache->setCacheDirectory(
                        QString::fromStdString(preparation_.cache_directory));
                    cache->setMaximumCacheSize(MaximumBytes());
                    manager_->setCache(cache.release());
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
        if (stopping_.load() ||
            (preparation.policy.maximum_megabytes != 0U &&
             !preparation.cache_available) ||
            preparation.cache_directory != preparation_.cache_directory) {
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
                    QDir owned(
                        QString::fromStdString(preparation.cache_directory));
                    if (!preparation.cache_directory.empty() &&
                        owned.exists() && !owned.removeRecursively()) {
                        preparation.diagnostic = kCleanupDiagnostic;
                        qWarning().noquote()
                            << QString::fromLatin1(kCleanupDiagnostic);
                    }
                    activated = true;
                } else {
                    auto* cache =
                        qobject_cast<QNetworkDiskCache*>(manager_->cache());
                    if (cache == nullptr) {
                        auto replacement =
                            std::make_unique<QNetworkDiskCache>();
                        replacement->setCacheDirectory(QString::fromStdString(
                            preparation.cache_directory));
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
                }
            },
            Qt::BlockingQueuedConnection);
        return activated;
    }

    void Shutdown() noexcept {
        if (stopping_.exchange(true)) {
            return;
        }
        std::lock_guard<std::mutex> lock(fetch_mutex_);
        QMetaObject::invokeMethod(
            &worker_,
            [this]() {
                if (manager_ != nullptr && preparation_.policy.clear_on_exit) {
                    if (manager_->cache() != nullptr) {
                        manager_->cache()->clear();
                    }
                    QDir owned(
                        QString::fromStdString(preparation_.cache_directory));
                    if (!preparation_.cache_directory.empty() &&
                        owned.exists() && !owned.removeRecursively()) {
                        preparation_.diagnostic = kCleanupDiagnostic;
                        qWarning().noquote()
                            << QString::fromLatin1(kCleanupDiagnostic);
                    }
                }
                manager_.reset();
            },
            Qt::BlockingQueuedConnection);
        thread_.quit();
        thread_.wait();
    }

    std::int64_t MaximumBytes() const noexcept {
        return static_cast<std::int64_t>(
                   preparation_.policy.maximum_megabytes) *
               kBytesPerMebibyte;
    }

    Preparation preparation_;
    QObject worker_;
    QThread thread_;
    std::unique_ptr<QNetworkAccessManager> manager_;
    std::mutex fetch_mutex_;
    std::atomic<bool> stopping_{false};
};

NetworkRuntime::Preparation NetworkRuntime::Prepare(
    NetworkCachePolicy policy, const std::string& cache_root) {
    if (policy.maximum_megabytes > 10240U) {
        throw std::invalid_argument("Network cache size is outside its bounds");
    }
    Preparation result{policy, {}, false, {}};
    if (cache_root.empty()) {
        result.diagnostic = kSetupDiagnostic;
        return result;
    }
    result.cache_directory = QDir(QString::fromStdString(cache_root))
                                 .filePath(QString::fromLatin1(kOwnedDirectory))
                                 .toStdString();
    if (policy.maximum_megabytes == 0U) {
        return result;
    }
    QDir directory;
    const QString owned = QString::fromStdString(result.cache_directory);
    if (!directory.mkpath(owned)) {
        result.diagnostic = kSetupDiagnostic;
        return result;
    }
    QFile probe(QDir(owned).filePath(QStringLiteral(".goldendict-write-test")));
    if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
        probe.write("ok", 2) != 2) {
        probe.close();
        probe.remove();
        result.diagnostic = kSetupDiagnostic;
        return result;
    }
    probe.close();
    if (!probe.remove()) {
        result.diagnostic = kSetupDiagnostic;
        return result;
    }
    result.cache_available = true;
    return result;
}

std::shared_ptr<NetworkRuntime> NetworkRuntime::Create(
    Preparation preparation) {
    if (preparation.policy.maximum_megabytes == 0U &&
        !preparation.cache_directory.empty()) {
        QDir owned(QString::fromStdString(preparation.cache_directory));
        if (owned.exists() && !owned.removeRecursively()) {
            preparation.diagnostic = kCleanupDiagnostic;
            qWarning().noquote() << QString::fromLatin1(kCleanupDiagnostic);
        }
    }
    return std::shared_ptr<NetworkRuntime>(
        new NetworkRuntime(std::move(preparation)));
}

NetworkRuntime::NetworkRuntime(Preparation preparation)
    : impl_(std::make_unique<Impl>(std::move(preparation))) {}

NetworkRuntime::~NetworkRuntime() = default;

HttpResponse NetworkRuntime::Fetch(const HttpRequest& request,
                                   const std::function<bool()>& is_cancelled) {
    return impl_->Fetch(request, is_cancelled);
}

bool NetworkRuntime::Activate(Preparation preparation) noexcept {
    return impl_->Activate(std::move(preparation));
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
