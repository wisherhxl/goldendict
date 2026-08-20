// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_request_controller.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include <QMetaObject>
#include <QThread>

namespace {

constexpr char kInternalErrorMessage[] = "Full-text search failed internally.";

}  // namespace

class FullTextRequestWorker final : public QThread {
   public:
    using Completion = FullTextRequestController::Completion;

    explicit FullTextRequestWorker(Completion completion)
        : completion_(std::move(completion)) {
        start();
    }

    ~FullTextRequestWorker() override { Stop(); }

    void Submit(const goldendict::core::DictionaryService* service,
                goldendict::core::FullTextQuery query,
                std::uint64_t generation) {
        auto token = std::make_shared<Token>();
        {
            const std::lock_guard lock(mutex_);
            if (stopping_)
                return;
            if (running_token_ != nullptr)
                running_token_->Cancel();
            if (pending_.has_value())
                pending_->token->Cancel();
            pending_ =
                Work{service, std::move(query), generation, std::move(token)};
        }
        ready_.notify_one();
    }

    void Cancel() {
        const std::lock_guard lock(mutex_);
        if (running_token_ != nullptr)
            running_token_->Cancel();
        if (pending_.has_value()) {
            pending_->token->Cancel();
            pending_.reset();
        }
    }

    void Stop() {
        {
            const std::lock_guard lock(mutex_);
            if (!stopping_) {
                stopping_ = true;
                if (running_token_ != nullptr)
                    running_token_->Cancel();
                if (pending_.has_value()) {
                    pending_->token->Cancel();
                    pending_.reset();
                }
            }
        }
        ready_.notify_one();
        wait();
    }

   protected:
    void run() override {
        for (;;) {
            Work work;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock, [this]() {
                    return stopping_ || pending_.has_value();
                });
                if (stopping_)
                    return;
                work = std::move(*pending_);
                pending_.reset();
                running_token_ = work.token;
            }

            goldendict::core::FullTextResponse response;
            try {
                response =
                    work.service->SearchFullText(work.query, work.token.get());
            } catch (...) {
                response.errors.push_back(
                    {goldendict::core::FullTextErrorCode::kInternal,
                     {},
                     kInternalErrorMessage});
            }

            {
                const std::lock_guard lock(mutex_);
                if (running_token_ == work.token)
                    running_token_.reset();
                if (stopping_)
                    return;
            }
            completion_(work.generation, std::move(response));
        }
    }

   private:
    class Token final : public goldendict::core::CancellationToken {
       public:
        bool IsCancellationRequested() const noexcept override {
            return cancelled_.load();
        }

        void Cancel() noexcept { cancelled_.store(true); }

       private:
        std::atomic_bool cancelled_ = false;
    };

    struct Work {
        const goldendict::core::DictionaryService* service = nullptr;
        goldendict::core::FullTextQuery query;
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

FullTextRequestController::FullTextRequestController(Completion completion,
                                                     QObject* parent)
    : QObject(parent), completion_(std::move(completion)) {
    StartWorker();
}

FullTextRequestController::~FullTextRequestController() {
    Stop();
}

void FullTextRequestController::SetService(
    const goldendict::core::DictionaryService* service) {
    Stop();
    service_ = service;
    running_ = false;
    has_generation_ = false;
    if (!detached_)
        StartWorker();
}

void FullTextRequestController::SetBindingRegistry(
    const goldendict::widgets::WidgetsFacadeBindingRegistry*
        registry) noexcept {
    registry_ = registry;
}

void FullTextRequestController::Submit(goldendict::core::FullTextQuery query,
                                       std::uint64_t generation) {
    auto binding =
        registry_ == nullptr
            ? goldendict::widgets::WidgetsFacadeBindingRegistry::Lease{}
            : registry_->Acquire();
    const auto* service = binding ? binding->service : service_;
    if (detached_ || service == nullptr || worker_ == nullptr)
        return;
    if (has_generation_ && generation <= generation_)
        return;
    generation_ = generation;
    has_generation_ = true;
    running_ = true;
    active_binding_ = std::move(binding);
    worker_->Submit(service, std::move(query), generation);
}

void FullTextRequestController::Cancel() {
    has_generation_ = false;
    running_ = false;
    if (worker_ != nullptr)
        worker_->Cancel();
    active_binding_ = {};
}

void FullTextRequestController::DetachConsumer() {
    if (detached_)
        return;
    detached_ = true;
    completion_ = {};
    Stop();
}

void FullTextRequestController::Stop() {
    has_generation_ = false;
    running_ = false;
    if (worker_ != nullptr) {
        worker_->Stop();
        worker_.reset();
    }
    active_binding_ = {};
}

void FullTextRequestController::QuiesceBindingConsumer() noexcept {
    Stop();
    registry_ = nullptr;
}

bool FullTextRequestController::IsRunning() const noexcept {
    return running_;
}

void FullTextRequestController::StartWorker() {
    worker_ = std::make_unique<FullTextRequestWorker>(
        [this](std::uint64_t generation,
               goldendict::core::FullTextResponse response) mutable {
            QMetaObject::invokeMethod(
                this,
                [this, generation, response = std::move(response)]() mutable {
                    Finish(generation, std::move(response));
                });
        });
}

void FullTextRequestController::Finish(
    std::uint64_t generation, goldendict::core::FullTextResponse response) {
    if (detached_ || !has_generation_ || generation != generation_)
        return;
    running_ = false;
    if (completion_)
        completion_(generation, std::move(response));
    active_binding_ = {};
}
