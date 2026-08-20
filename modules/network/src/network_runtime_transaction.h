// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

#include "goldendict/network/network_runtime.h"

namespace goldendict::network {

class NetworkRuntimeTransaction final {
   public:
    class Published final {
       public:
        Published() noexcept;
        ~Published();
        Published(Published&&) noexcept;
        Published& operator=(Published&&) noexcept;
        Published(const Published&) = delete;
        Published& operator=(const Published&) = delete;
        explicit operator bool() const noexcept;

       private:
        class Impl;
        explicit Published(std::unique_ptr<Impl> impl) noexcept;
        std::unique_ptr<Impl> impl_;
        friend class NetworkRuntimeTransaction;
    };

    static Published Publish(
        NetworkRuntime& runtime,
        NetworkRuntime::CommitReservation& reservation) noexcept;
    static NetworkRuntime::CommitResult Finish(NetworkRuntime& runtime,
                                               Published& published) noexcept;
};

}  // namespace goldendict::network
