// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_SUGGESTION_WORKER_H_
#define GOLDENDICT_APPS_GOLDENDICT_SUGGESTION_WORKER_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include <QThread>

#include "goldendict/core/desktop_facade.h"
#include "goldendict/core/dictionary_service.h"

class SuggestionWorker final : public QThread {
   public:
    using Completion =
        std::function<void(goldendict::core::ArticleTabId, std::uint64_t,
                           goldendict::core::SuggestionResponse)>;

    explicit SuggestionWorker(Completion completion);
    ~SuggestionWorker() override;

    void Submit(const goldendict::core::DictionaryService* service,
                goldendict::core::SuggestionQuery query,
                goldendict::core::ArticleTabId tab_id,
                std::uint64_t generation);
    void Cancel();
    void Stop();

   protected:
    void run() override;

   private:
    class Token final : public goldendict::core::CancellationToken {
       public:
        bool IsCancellationRequested() const noexcept override;
        void Cancel() noexcept;

       private:
        std::atomic_bool cancelled_ = false;
    };

    struct Work {
        const goldendict::core::DictionaryService* service = nullptr;
        goldendict::core::SuggestionQuery query;
        goldendict::core::ArticleTabId tab_id = 0U;
        std::uint64_t generation = 0U;
        std::shared_ptr<Token> token;
    };

    Completion completion_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::optional<Work> pending_;
    std::shared_ptr<Token> running_token_;
    bool stopping_ = false;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_SUGGESTION_WORKER_H_
