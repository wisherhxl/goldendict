// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_cache_storage.h"

#include <QDir>
#include <QFile>

namespace goldendict::network {
namespace {

constexpr char kOwnedDirectory[] = "qt-network-http";
constexpr char kSetupDiagnostic[] =
    "Qt Network cache setup failed; HTTP traffic will remain uncached";
constexpr char kCleanupDiagnostic[] =
    "Qt Network cache cleanup failed; cached data may remain";

}  // namespace

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
    const std::string& cache_directory) {
    if (cache_directory.empty()) {
        return true;
    }
    QDir owned(QString::fromStdString(cache_directory));
    return !owned.exists() || owned.removeRecursively();
}

const char* NetworkCacheStorage::SetupDiagnostic() noexcept {
    return kSetupDiagnostic;
}

const char* NetworkCacheStorage::CleanupDiagnostic() noexcept {
    return kCleanupDiagnostic;
}

}  // namespace goldendict::network
