// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_WORK_EXECUTOR_H_
#define GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_WORK_EXECUTOR_H_

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

#include "full_text_index_lifecycle.h"

namespace goldendict::core::dictionary {

// The referenced coordinator and its registered ports must outlive this
// executor. Shutdown joins the owned worker before returning.
class FullTextIndexWorkExecutor final {
   public:
    explicit FullTextIndexWorkExecutor(
        FullTextIndexLifecycleCoordinator& coordinator);
    ~FullTextIndexWorkExecutor();

    FullTextIndexWorkExecutor(const FullTextIndexWorkExecutor&) = delete;
    FullTextIndexWorkExecutor& operator=(const FullTextIndexWorkExecutor&) =
        delete;
    FullTextIndexWorkExecutor(FullTextIndexWorkExecutor&&) = delete;
    FullTextIndexWorkExecutor& operator=(FullTextIndexWorkExecutor&&) = delete;

    bool Submit(const FullTextIndexExecutionBounds& bounds);
    void Shutdown() noexcept;

   private:
    void Run();

    FullTextIndexLifecycleCoordinator& coordinator_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::optional<FullTextIndexExecutionBounds> pending_bounds_;
    std::optional<FullTextIndexWorkIdentity> active_identity_;
    bool shutting_down_ = false;
    std::thread worker_;
};

}  // namespace goldendict::core::dictionary

#endif  // GOLDENDICT_CORE_SRC_DICTIONARY_FULL_TEXT_INDEX_WORK_EXECUTOR_H_
