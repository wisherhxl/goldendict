// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_NETWORK_RUNTIME_H_
#define GOLDENDICT_NETWORK_NETWORK_RUNTIME_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace goldendict::network {

class NetworkRuntimeTestAccess;
class NetworkRuntimeTransaction;
struct HttpRequest;
struct HttpResponse;

struct NetworkCachePolicy {
    std::uint32_t maximum_megabytes = 0U;
    bool clear_on_exit = false;
};

class NetworkRuntime final {
    class Impl;

   public:
    enum class CommitResult {
        kRejected,
        kPublished,
        kPublishedWithPostWorkFailure,
    };

    struct Preparation {
        NetworkCachePolicy policy;
        std::string cache_directory;
        bool cache_available = false;
        std::string diagnostic;
    };

    class PreparedCandidate final {
       public:
        PreparedCandidate();
        ~PreparedCandidate();

        PreparedCandidate(const PreparedCandidate&) = delete;
        PreparedCandidate& operator=(const PreparedCandidate&) = delete;
        PreparedCandidate(PreparedCandidate&&) noexcept;
        PreparedCandidate& operator=(PreparedCandidate&&) noexcept;

        explicit operator bool() const noexcept;

       private:
        friend class NetworkRuntime;
        friend class NetworkRuntime::Impl;
        friend class NetworkRuntimeTestAccess;
        friend class NetworkRuntimeTransaction;
        class Impl;
        explicit PreparedCandidate(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> impl_;
    };

    class CommitReservation final {
       public:
        CommitReservation();
        ~CommitReservation();
        CommitReservation(const CommitReservation&) = delete;
        CommitReservation& operator=(const CommitReservation&) = delete;
        CommitReservation(CommitReservation&&) noexcept;
        CommitReservation& operator=(CommitReservation&&) noexcept;

        explicit operator bool() const noexcept;

       private:
        friend class NetworkRuntime;
        friend class NetworkRuntime::Impl;
        friend class NetworkRuntimeTransaction;
        class Impl;
        explicit CommitReservation(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> impl_;
    };

    static Preparation Prepare(NetworkCachePolicy policy,
                               const std::string& cache_root);
    static std::shared_ptr<NetworkRuntime> Create(Preparation preparation);
    ~NetworkRuntime();

    NetworkRuntime(const NetworkRuntime&) = delete;
    NetworkRuntime& operator=(const NetworkRuntime&) = delete;
    HttpResponse Fetch(const HttpRequest& request,
                       const std::function<bool()>& is_cancelled = {});
    bool Activate(Preparation preparation) noexcept;
    PreparedCandidate PrepareCandidate(Preparation preparation);
    CommitResult Commit(PreparedCandidate& candidate) noexcept;
    CommitReservation Reserve(PreparedCandidate& candidate);
    void Abort(CommitReservation& reservation) noexcept;
    CommitResult Publish(CommitReservation& reservation) noexcept;
    void Shutdown() noexcept;
    std::int64_t maximum_cache_bytes() const noexcept;
    const std::string& cache_directory() const noexcept;
    const std::string& diagnostic() const noexcept;

   private:
    friend class NetworkRuntimeTestAccess;
    friend class NetworkRuntimeTransaction;
    explicit NetworkRuntime(Preparation preparation);
    std::unique_ptr<Impl> impl_;
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_NETWORK_RUNTIME_H_
