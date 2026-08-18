// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_RENDERED_TEXT_MATCH_PLAN_CONTROLLER_H_
#define GOLDENDICT_APPS_GOLDENDICT_RENDERED_TEXT_MATCH_PLAN_CONTROLLER_H_

#include <cstdint>
#include <functional>
#include <memory>

#include <QObject>

#include "goldendict/core/desktop_facade.h"

class RenderedTextMatchPlanWorker;

class RenderedTextMatchPlanController final : public QObject {
   public:
    using Completion = std::function<void(
        std::uint64_t, goldendict::core::RenderedTextMatchPlanResult)>;

    explicit RenderedTextMatchPlanController(Completion completion,
                                             QObject* parent = nullptr);
    ~RenderedTextMatchPlanController() override;

    void SetFacade(const goldendict::core::DesktopFacade* facade);
    void Submit(goldendict::core::RenderedTextMatchPlanRequest request,
                std::uint64_t generation);
    void Cancel();
    void DetachConsumer();
    void Stop();
    bool IsRunning() const noexcept;

   private:
    void StartWorker();
    void Finish(std::uint64_t generation,
                goldendict::core::RenderedTextMatchPlanResult result);

    Completion completion_;
    const goldendict::core::DesktopFacade* facade_ = nullptr;
    std::unique_ptr<RenderedTextMatchPlanWorker> worker_;
    std::uint64_t generation_ = 0U;
    bool has_generation_ = false;
    bool running_ = false;
    bool detached_ = false;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_RENDERED_TEXT_MATCH_PLAN_CONTROLLER_H_
