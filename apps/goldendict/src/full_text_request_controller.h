// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_FULL_TEXT_REQUEST_CONTROLLER_H_
#define GOLDENDICT_APPS_GOLDENDICT_FULL_TEXT_REQUEST_CONTROLLER_H_

#include <cstdint>
#include <functional>
#include <memory>

#include <QObject>

#include "goldendict/core/dictionary_service.h"

class FullTextRequestWorker;

class FullTextRequestController final : public QObject {
   public:
    using Completion =
        std::function<void(std::uint64_t, goldendict::core::FullTextResponse)>;

    explicit FullTextRequestController(Completion completion,
                                       QObject* parent = nullptr);
    ~FullTextRequestController() override;

    void SetService(const goldendict::core::DictionaryService* service);
    void Submit(goldendict::core::FullTextQuery query,
                std::uint64_t generation);
    void Cancel();
    void DetachConsumer();
    void Stop();
    bool IsRunning() const noexcept;

   private:
    void StartWorker();
    void Finish(std::uint64_t generation,
                goldendict::core::FullTextResponse response);

    Completion completion_;
    const goldendict::core::DictionaryService* service_ = nullptr;
    std::unique_ptr<FullTextRequestWorker> worker_;
    std::uint64_t generation_ = 0U;
    bool has_generation_ = false;
    bool running_ = false;
    bool detached_ = false;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_FULL_TEXT_REQUEST_CONTROLLER_H_
