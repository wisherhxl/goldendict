// SPDX-License-Identifier: GPL-3.0-or-later

#include "suggestion_worker.h"

#include <exception>
#include <utility>

SuggestionWorker::SuggestionWorker(Completion completion)
    : completion_(std::move(completion)) {
    start();
}

SuggestionWorker::~SuggestionWorker() {
    Stop();
}

bool SuggestionWorker::Token::IsCancellationRequested() const noexcept {
    return cancelled_.load();
}

void SuggestionWorker::Token::Cancel() noexcept {
    cancelled_.store(true);
}

void SuggestionWorker::Submit(
    const goldendict::core::DictionaryService* service,
    goldendict::core::SuggestionQuery query,
    goldendict::core::ArticleTabId tab_id, std::uint64_t generation) {
    auto token = std::make_shared<Token>();
    {
        const std::lock_guard lock(mutex_);
        if (stopping_)
            return;
        if (running_token_ != nullptr)
            running_token_->Cancel();
        if (pending_.has_value())
            pending_->token->Cancel();
        pending_ = Work{service, std::move(query), tab_id, generation,
                        std::move(token)};
    }
    ready_.notify_one();
}

void SuggestionWorker::Stop() {
    {
        const std::lock_guard lock(mutex_);
        if (stopping_)
            return;
        stopping_ = true;
        if (running_token_ != nullptr)
            running_token_->Cancel();
        if (pending_.has_value()) {
            pending_->token->Cancel();
            pending_.reset();
        }
    }
    ready_.notify_one();
    wait();
}

void SuggestionWorker::Cancel() {
    const std::lock_guard lock(mutex_);
    if (running_token_ != nullptr)
        running_token_->Cancel();
    if (pending_.has_value()) {
        pending_->token->Cancel();
        pending_.reset();
    }
}

void SuggestionWorker::run() {
    for (;;) {
        Work work;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock,
                        [this]() { return stopping_ || pending_.has_value(); });
            if (stopping_)
                return;
            work = std::move(*pending_);
            pending_.reset();
            running_token_ = work.token;
        }
        goldendict::core::SuggestionResponse response;
        try {
            response = work.service->Suggest(work.query, work.token.get());
        } catch (const std::exception& error) {
            response.errors.push_back(
                {goldendict::core::LookupErrorCode::kInternal,
                 {},
                 error.what()});
        }
        {
            const std::lock_guard lock(mutex_);
            if (running_token_ == work.token)
                running_token_.reset();
            if (stopping_)
                return;
        }
        completion_(work.tab_id, work.generation, std::move(response));
    }
}
