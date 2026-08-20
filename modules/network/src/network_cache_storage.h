// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_NETWORK_CACHE_STORAGE_H_
#define GOLDENDICT_NETWORK_NETWORK_CACHE_STORAGE_H_

#include <string>

namespace goldendict::network {

struct NetworkCacheStoragePreparation {
    std::string directory;
    bool available = false;
    std::string diagnostic;
};

// Owns filesystem operations for the one GoldenDict-managed Qt Network cache
// directory. QObject and QNetworkDiskCache ownership remains with
// NetworkRuntime.
class NetworkCacheStorage final {
   public:
    static NetworkCacheStoragePreparation Prepare(const std::string& cache_root,
                                                  bool disk_cache_enabled);
    static bool RemoveOwnedDirectory(const std::string& cache_directory);

    static const char* SetupDiagnostic() noexcept;
    static const char* CleanupDiagnostic() noexcept;
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_NETWORK_CACHE_STORAGE_H_
