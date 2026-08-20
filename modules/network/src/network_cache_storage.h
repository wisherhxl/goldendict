// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_NETWORK_CACHE_STORAGE_H_
#define GOLDENDICT_NETWORK_NETWORK_CACHE_STORAGE_H_

#include <cstdint>
#include <string>

namespace goldendict::network {

struct NetworkCacheStoragePreparation {
    std::string directory;
    bool available = false;
    std::string diagnostic;
};

// Process-local authority for the one directory-bound Qt Network disk cache.
// A runtime owns one slot and keeps its move-only lease until its cache has
// been destroyed.
class NetworkCacheStorageSlot final {
   public:
    struct Identity {
        std::string key;
        std::uint64_t generation = 0U;

        explicit operator bool() const noexcept;

        friend bool operator==(const Identity& left,
                               const Identity& right) noexcept {
            return left.key == right.key && left.generation == right.generation;
        }
    };

    class Lease final {
       public:
        Lease() = default;
        ~Lease();

        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;

        explicit operator bool() const noexcept;
        const std::string& directory() const noexcept;
        Identity identity() const;
        bool Release() noexcept;

       private:
        friend class NetworkCacheStorageSlot;
        Lease(std::string directory, std::string key, std::uint64_t generation);

        std::string directory_;
        std::string key_;
        std::uint64_t generation_ = 0U;
        bool active_ = false;
    };

    explicit NetworkCacheStorageSlot(std::string cache_directory);

    NetworkCacheStorageSlot(const NetworkCacheStorageSlot&) = delete;
    NetworkCacheStorageSlot& operator=(const NetworkCacheStorageSlot&) = delete;
    Lease Acquire() const;

   private:
    std::string directory_;
    std::string key_;
};

// Owns filesystem operations for the one GoldenDict-managed Qt Network cache
// directory. QObject and QNetworkDiskCache ownership remains with
// NetworkRuntime.
class NetworkCacheStorage final {
   public:
    static NetworkCacheStoragePreparation Prepare(const std::string& cache_root,
                                                  bool disk_cache_enabled);
    static bool RemoveOwnedDirectory(
        const NetworkCacheStorageSlot::Lease& lease);

    static const char* SetupDiagnostic() noexcept;
    static const char* CleanupDiagnostic() noexcept;
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_NETWORK_CACHE_STORAGE_H_
