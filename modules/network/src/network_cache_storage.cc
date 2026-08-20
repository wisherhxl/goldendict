// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_cache_storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <mutex>
#include <unordered_map>
#include <utility>

namespace goldendict::network {
namespace {

constexpr char kOwnedDirectory[] = "qt-network-http";
constexpr char kSetupDiagnostic[] =
    "Qt Network cache setup failed; HTTP traffic will remain uncached";
constexpr char kCleanupDiagnostic[] =
    "Qt Network cache cleanup failed; cached data may remain";

struct StorageLeaseRegistry {
    std::mutex mutex;
    std::unordered_map<std::string, std::uint64_t> active;
    std::uint64_t next_generation = 1U;
};

StorageLeaseRegistry& LeaseRegistry() {
    static StorageLeaseRegistry registry;
    return registry;
}

std::string StorageKey(const std::string& directory) {
    if (directory.empty()) {
        return {};
    }
    const QFileInfo info(QString::fromStdString(directory));
    return QDir::cleanPath(info.absoluteFilePath()).toStdString();
}

std::uint64_t NextGeneration(StorageLeaseRegistry& registry) {
    const std::uint64_t generation = registry.next_generation++;
    if (registry.next_generation == 0U) {
        registry.next_generation = 1U;
    }
    return generation;
}

}  // namespace

NetworkCacheStorageSlot::Identity::operator bool() const noexcept {
    return !key.empty() && generation != 0U;
}

NetworkCacheStorageSlot::Lease::Lease(std::string directory, std::string key,
                                      std::uint64_t generation)
    : directory_(std::move(directory)),
      key_(std::move(key)),
      generation_(generation),
      active_(true) {}

NetworkCacheStorageSlot::Lease::~Lease() {
    Release();
}

NetworkCacheStorageSlot::Lease::Lease(Lease&& other) noexcept
    : directory_(std::move(other.directory_)),
      key_(std::move(other.key_)),
      generation_(other.generation_),
      active_(other.active_) {
    other.generation_ = 0U;
    other.active_ = false;
}

NetworkCacheStorageSlot::Lease& NetworkCacheStorageSlot::Lease::operator=(
    Lease&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    Release();
    directory_ = std::move(other.directory_);
    key_ = std::move(other.key_);
    generation_ = other.generation_;
    active_ = other.active_;
    other.generation_ = 0U;
    other.active_ = false;
    return *this;
}

NetworkCacheStorageSlot::Lease::operator bool() const noexcept {
    return active_;
}

const std::string& NetworkCacheStorageSlot::Lease::directory() const noexcept {
    return directory_;
}

NetworkCacheStorageSlot::Identity NetworkCacheStorageSlot::Lease::identity()
    const {
    return active_ ? Identity{key_, generation_} : Identity{};
}

bool NetworkCacheStorageSlot::Lease::Release() noexcept {
    if (!active_) {
        return false;
    }
    auto& registry = LeaseRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    const auto current = registry.active.find(key_);
    if (current == registry.active.end() || current->second != generation_) {
        active_ = false;
        return false;
    }
    registry.active.erase(current);
    active_ = false;
    return true;
}

NetworkCacheStorageSlot::NetworkCacheStorageSlot(std::string cache_directory)
    : directory_(std::move(cache_directory)), key_(StorageKey(directory_)) {}

NetworkCacheStorageSlot::Lease NetworkCacheStorageSlot::Acquire() const {
    if (key_.empty()) {
        return {};
    }
    auto& registry = LeaseRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    if (registry.active.find(key_) != registry.active.end()) {
        return {};
    }
    const std::uint64_t generation = NextGeneration(registry);
    registry.active.emplace(key_, generation);
    return Lease(directory_, key_, generation);
}

NetworkCacheStoragePreparation NetworkCacheStorage::Prepare(
    const std::string& cache_root, bool disk_cache_enabled) {
    NetworkCacheStoragePreparation result;
    if (cache_root.empty()) {
        result.diagnostic = kSetupDiagnostic;
        return result;
    }

    result.directory = QDir(QString::fromStdString(cache_root))
                           .filePath(QString::fromLatin1(kOwnedDirectory))
                           .toStdString();
    if (!disk_cache_enabled) {
        return result;
    }

    QDir directory;
    const QString owned = QString::fromStdString(result.directory);
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
    result.available = true;
    return result;
}

bool NetworkCacheStorage::RemoveOwnedDirectory(
    const NetworkCacheStorageSlot::Lease& lease) {
    if (!lease) {
        return false;
    }
    QDir owned(QString::fromStdString(lease.directory()));
    return !owned.exists() || owned.removeRecursively();
}

const char* NetworkCacheStorage::SetupDiagnostic() noexcept {
    return kSetupDiagnostic;
}

const char* NetworkCacheStorage::CleanupDiagnostic() noexcept {
    return kCleanupDiagnostic;
}

}  // namespace goldendict::network
