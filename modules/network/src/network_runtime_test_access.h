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
    static void ObservePublication(NetworkRuntime::PreparedCandidate& candidate,
                                   std::function<void()> observer);
    static bool DestroyOnOwnerThread(
        NetworkRuntime& runtime, NetworkRuntime::PreparedCandidate& candidate);
    static NetworkRuntime::CommitResult CommitOnOwnerThread(
        NetworkRuntime& runtime, NetworkRuntime::PreparedCandidate& candidate);
    static void MakeUnready(NetworkRuntime::PreparedCandidate& candidate);
    static void InvalidateLeaseIdentity(
        NetworkRuntime::PreparedCandidate& candidate);
    static void ForcePostWorkFailure(
        NetworkRuntime::PreparedCandidate& candidate);
    static const void* BoundDiskCache(const NetworkRuntime& runtime);
    static std::uint64_t DirectoryConfigurationCount(
        const NetworkRuntime& runtime);
    static bool DispatcherTimerActive(const NetworkRuntime& runtime);
    static std::uint64_t DispatcherTimerCreationCount(
        const NetworkRuntime& runtime);
    static std::uint64_t DispatcherTimerConnectionCount(
        const NetworkRuntime& runtime);
    static std::uint64_t DispatcherTimerStartCount(
        const NetworkRuntime& runtime);
    static std::uint64_t DispatcherTimerWakeupCount(
        const NetworkRuntime& runtime);
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_SRC_NETWORK_RUNTIME_TEST_ACCESS_H_
