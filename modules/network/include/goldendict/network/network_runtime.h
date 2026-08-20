// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_NETWORK_NETWORK_RUNTIME_H_
#define GOLDENDICT_NETWORK_NETWORK_RUNTIME_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace goldendict::network {

class NetworkRuntimeTestAccess;
struct HttpRequest;
struct HttpResponse;

struct NetworkCachePolicy {
    std::uint32_t maximum_megabytes = 0U;
    bool clear_on_exit = false;
};

class NetworkRuntime final {
    class Impl;

   public:
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
        class Impl;
        explicit PreparedCandidate(std::unique_ptr<Impl> impl);
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
    void Shutdown() noexcept;
    std::int64_t maximum_cache_bytes() const noexcept;
    const std::string& cache_directory() const noexcept;
    const std::string& diagnostic() const noexcept;

   private:
    friend class NetworkRuntimeTestAccess;
    explicit NetworkRuntime(Preparation preparation);
    std::unique_ptr<Impl> impl_;
};

}  // namespace goldendict::network

#endif  // GOLDENDICT_NETWORK_NETWORK_RUNTIME_H_
