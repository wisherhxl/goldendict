// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_SRC_NETWORK_RUNTIME_TEST_ACCESS_H_
#define GOLDENDICT_NETWORK_SRC_NETWORK_RUNTIME_TEST_ACCESS_H_

#include "goldendict/network/network_runtime.h"

#include <cstdint>
#include <functional>
#include <string>

namespace goldendict::network {

class NetworkRuntimeTestAccess final {
   public:
    static bool IsCurrent(const NetworkRuntime& runtime,
                          const NetworkRuntime::PreparedCandidate& candidate);
    static bool Consume(NetworkRuntime& runtime,
                        NetworkRuntime::PreparedCandidate& candidate);
    static std::int64_t MaximumCacheBytes(
        const NetworkRuntime::PreparedCandidate& candidate);
    static std::string CacheDirectory(
        const NetworkRuntime::PreparedCandidate& candidate);
    static bool WasConstructedOnOwnerThread(
        const NetworkRuntime& runtime,
        const NetworkRuntime::PreparedCandidate& candidate);
    static void ObserveDestruction(NetworkRuntime::PreparedCandidate& candidate,
                                   std::function<void(bool)> observer);
    static const void* BoundDiskCache(const NetworkRuntime& runtime);
    static std::uint64_t DirectoryConfigurationCount(
        const NetworkRuntime& runtime);
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_SRC_NETWORK_RUNTIME_TEST_ACCESS_H_
