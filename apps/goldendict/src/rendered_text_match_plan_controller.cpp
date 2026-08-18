// SPDX-License-Identifier: GPL-3.0-or-later

#include "rendered_text_match_plan_controller.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include <QMetaObject>
#include <QThread>

namespace {

constexpr char kInternalErrorMessage[] =
    "Rendered-text match-plan worker failed internally.";

}  // namespace

class RenderedTextMatchPlanWorker final : public QThread {
   public:
    using Completion = RenderedTextMatchPlanController::Completion;

    explicit RenderedTextMatchPlanWorker(Completion completion)
        : completion_(std::move(completion)) {
        start();
    }

    ~RenderedTextMatchPlanWorker() override { Stop(); }

    void Submit(const goldendict::core::DesktopFacade* facade,
                goldendict::core::RenderedTextMatchPlanRequest request,
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
                Work{facade, std::move(request), generation, std::move(token)};
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

            goldendict::core::RenderedTextMatchPlanResult result;
            try {
                result = work.facade->BuildRenderedTextMatchPlan(
                    work.request, work.token.get());
            } catch (...) {
                result.error =
                    goldendict::core::RenderedTextMatchPlanError::kInternal;
                result.message = kInternalErrorMessage;
            }

            {
                const std::lock_guard lock(mutex_);
                if (running_token_ == work.token)
                    running_token_.reset();
                if (stopping_)
                    return;
            }
            completion_(work.generation, std::move(result));
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
        const goldendict::core::DesktopFacade* facade = nullptr;
        goldendict::core::RenderedTextMatchPlanRequest request;
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

RenderedTextMatchPlanController::RenderedTextMatchPlanController(
    Completion completion, QObject* parent)
    : QObject(parent), completion_(std::move(completion)) {
    StartWorker();
}

RenderedTextMatchPlanController::~RenderedTextMatchPlanController() {
    Stop();
}

void RenderedTextMatchPlanController::SetFacade(
    const goldendict::core::DesktopFacade* facade) {
    Stop();
    facade_ = facade;
    running_ = false;
    has_generation_ = false;
    if (!detached_)
        StartWorker();
}

void RenderedTextMatchPlanController::Submit(
    goldendict::core::RenderedTextMatchPlanRequest request,
    std::uint64_t generation) {
    if (detached_ || facade_ == nullptr || worker_ == nullptr)
        return;
    if (has_generation_ && generation <= generation_)
        return;
    generation_ = generation;
    has_generation_ = true;
    running_ = true;
    worker_->Submit(facade_, std::move(request), generation);
}

void RenderedTextMatchPlanController::Cancel() {
    has_generation_ = false;
    running_ = false;
    if (worker_ != nullptr)
        worker_->Cancel();
}

void RenderedTextMatchPlanController::DetachConsumer() {
    if (detached_)
        return;
    detached_ = true;
    completion_ = {};
    Stop();
}

void RenderedTextMatchPlanController::Stop() {
    has_generation_ = false;
    running_ = false;
    if (worker_ != nullptr) {
        worker_->Stop();
        worker_.reset();
    }
}

bool RenderedTextMatchPlanController::IsRunning() const noexcept {
    return running_;
}

void RenderedTextMatchPlanController::StartWorker() {
    worker_ = std::make_unique<RenderedTextMatchPlanWorker>(
        [this](std::uint64_t generation,
               goldendict::core::RenderedTextMatchPlanResult result) mutable {
            QMetaObject::invokeMethod(
                this, [this, generation, result = std::move(result)]() mutable {
                    Finish(generation, std::move(result));
                });
        });
}

void RenderedTextMatchPlanController::Finish(
    std::uint64_t generation,
    goldendict::core::RenderedTextMatchPlanResult result) {
    if (detached_ || !has_generation_ || generation != generation_)
        return;
    running_ = false;
    has_generation_ = false;
    if (completion_)
        completion_(generation, std::move(result));
}
