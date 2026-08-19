// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include "full_text_request_controller.h"
#include "rendered_text_match_plan_controller.h"

namespace {

class FakeDictionaryService final : public goldendict::core::DictionaryService {
   public:
    enum class Behavior { kImmediate, kWaitForCancellation, kThrow };

    std::vector<goldendict::core::DictionaryIdentity> GetCatalog()
        const override {
        return {};
    }

    goldendict::core::LookupResponse Lookup(
        const goldendict::core::LookupQuery&,
        const goldendict::core::CancellationToken*) const override {
        return {};
    }

    goldendict::core::SuggestionResponse Suggest(
        const goldendict::core::SuggestionQuery&,
        const goldendict::core::CancellationToken*) const override {
        return {};
    }

    goldendict::core::HeadwordEnumerationPage EnumerateHeadwords(
        const goldendict::core::HeadwordEnumerationQuery&,
        const goldendict::core::CancellationToken*) const override {
        return {};
    }

    goldendict::core::FullTextResponse SearchFullText(
        const goldendict::core::FullTextQuery& query,
        const goldendict::core::CancellationToken* cancellation)
        const override {
        {
            const std::lock_guard lock(mutex_);
            ++calls_;
            queries_.push_back(query.text);
            worker_thread_ = QThread::currentThread();
        }
        changed_.notify_all();

        if (behavior_ == Behavior::kThrow)
            throw std::runtime_error("secret /home/user/private/index.gdfts");

        if (behavior_ == Behavior::kWaitForCancellation) {
            std::unique_lock lock(mutex_);
            while (!cancellation->IsCancellationRequested())
                changed_.wait_for(lock, std::chrono::milliseconds(5));
            cancellation_seen_ = true;
            changed_.notify_all();
            changed_.wait(lock, [this]() {
                return !hold_after_cancellation_ || cancellation_released_;
            });
            lock.unlock();
            goldendict::core::FullTextResponse cancelled;
            cancelled.errors.push_back(
                {goldendict::core::FullTextErrorCode::kCancelled, "dict",
                 "cancelled"});
            return cancelled;
        }
        return response_;
    }

    std::unique_ptr<goldendict::core::LookupRequest> StartLookup(
        goldendict::core::LookupQuery) const override {
        return {};
    }

    std::vector<std::byte> GetResource(
        const goldendict::core::ResourceReference&,
        const goldendict::core::CancellationToken*) const override {
        return {};
    }

    bool WaitForCalls(int count) const {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [this, count]() { return calls_ >= count; });
    }

    bool WaitForCancellation() const {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [this]() { return cancellation_seen_; });
    }

    void ReleaseCancellation() {
        {
            const std::lock_guard lock(mutex_);
            cancellation_released_ = true;
        }
        changed_.notify_all();
    }

    int calls() const {
        const std::lock_guard lock(mutex_);
        return calls_;
    }

    std::vector<std::string> queries() const {
        const std::lock_guard lock(mutex_);
        return queries_;
    }

    QThread* worker_thread() const {
        const std::lock_guard lock(mutex_);
        return worker_thread_;
    }

    Behavior behavior_ = Behavior::kImmediate;
    goldendict::core::FullTextResponse response_;
    bool hold_after_cancellation_ = false;

   private:
    mutable std::mutex mutex_;
    mutable std::condition_variable changed_;
    mutable int calls_ = 0;
    mutable bool cancellation_seen_ = false;
    mutable bool cancellation_released_ = false;
    mutable std::vector<std::string> queries_;
    mutable QThread* worker_thread_ = nullptr;
};

class FakeDesktopFacade final : public goldendict::core::DesktopFacade {
   public:
    enum class Behavior { kImmediate, kWaitForCancellation, kThrow };

    goldendict::core::DictionaryService& GetDictionaryService() noexcept
        override {
        return service_;
    }

    const goldendict::core::DictionaryService& GetDictionaryService()
        const noexcept override {
        return service_;
    }

    std::unique_ptr<goldendict::core::HeadwordExportOperation>
    StartHeadwordExport(
        goldendict::core::HeadwordExportRequest) const override {
        return {};
    }

    goldendict::core::ArticleContent ComposeLookupPage(
        const goldendict::core::LookupResponse&) const override {
        return {};
    }

    std::optional<goldendict::core::ArticleUrl> ResolveArticleUrl(
        const std::string&) const override {
        return std::nullopt;
    }

    goldendict::core::ResolvedExactArticleTarget ResolveExactArticleTarget(
        const goldendict::core::ExactArticleTarget&) const override {
        return {};
    }

    goldendict::core::RenderedTextMatchPlanResult BuildRenderedTextMatchPlan(
        const goldendict::core::RenderedTextMatchPlanRequest& request,
        const goldendict::core::CancellationToken* cancellation)
        const override {
        {
            const std::lock_guard lock(mutex_);
            ++calls_;
            requests_.push_back(request);
            worker_thread_ = QThread::currentThread();
            explicit_cancellation_ = cancellation != nullptr;
        }
        changed_.notify_all();
        if (behavior_ == Behavior::kThrow)
            throw std::runtime_error("secret worker detail");
        if (behavior_ == Behavior::kWaitForCancellation) {
            std::unique_lock lock(mutex_);
            while (!cancellation->IsCancellationRequested())
                changed_.wait_for(lock, std::chrono::milliseconds(5));
            cancellation_seen_ = true;
            changed_.notify_all();
            changed_.wait(lock, [this]() {
                return !hold_after_cancellation_ || cancellation_released_;
            });
            goldendict::core::RenderedTextMatchPlanResult cancelled;
            cancelled.error =
                goldendict::core::RenderedTextMatchPlanError::kCancelled;
            return cancelled;
        }
        return result_;
    }

    goldendict::core::ArticleTabsState GetArticleTabsState() const override {
        return {};
    }

    goldendict::core::ArticleTabSession ExportArticleTabSession()
        const override {
        return {};
    }

    goldendict::core::TabOperationResult RestoreArticleTabSession(
        const goldendict::core::ArticleTabSession&) override {
        return {};
    }

    goldendict::core::TabOperationResult OpenArticleTab(
        const goldendict::core::TabNavigationState&,
        goldendict::core::TabOpenPolicy, goldendict::core::TabActivationPolicy,
        goldendict::core::TabPlacementPolicy) override {
        return {};
    }

    goldendict::core::TabOperationResult ActivateArticleTab(
        goldendict::core::ArticleTabId) override {
        return {};
    }

    goldendict::core::TabOperationResult CloseArticleTab(
        goldendict::core::ArticleTabId) override {
        return {};
    }

    goldendict::core::TabOperationResult CloseOtherArticleTabs(
        goldendict::core::ArticleTabId) override {
        return {};
    }

    goldendict::core::TabOperationResult GoBackInArticleTab(
        goldendict::core::ArticleTabId) override {
        return {};
    }

    goldendict::core::TabOperationResult GoForwardInArticleTab(
        goldendict::core::ArticleTabId) override {
        return {};
    }

    bool WaitForCalls(int count) const {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [this, count]() { return calls_ >= count; });
    }

    bool WaitForCancellation() const {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [this]() { return cancellation_seen_; });
    }

    void ReleaseCancellation() {
        {
            const std::lock_guard lock(mutex_);
            cancellation_released_ = true;
        }
        changed_.notify_all();
    }

    int calls() const {
        const std::lock_guard lock(mutex_);
        return calls_;
    }

    std::vector<goldendict::core::RenderedTextMatchPlanRequest> requests()
        const {
        const std::lock_guard lock(mutex_);
        return requests_;
    }

    QThread* worker_thread() const {
        const std::lock_guard lock(mutex_);
        return worker_thread_;
    }

    bool explicit_cancellation() const {
        const std::lock_guard lock(mutex_);
        return explicit_cancellation_;
    }

    mutable FakeDictionaryService service_;
    Behavior behavior_ = Behavior::kImmediate;
    goldendict::core::RenderedTextMatchPlanResult result_;
    bool hold_after_cancellation_ = false;

   private:
    mutable std::mutex mutex_;
    mutable std::condition_variable changed_;
    mutable int calls_ = 0;
    mutable bool cancellation_seen_ = false;
    mutable bool cancellation_released_ = false;
    mutable bool explicit_cancellation_ = false;
    mutable std::vector<goldendict::core::RenderedTextMatchPlanRequest>
        requests_;
    mutable QThread* worker_thread_ = nullptr;
};

goldendict::core::FullTextQuery Query(std::string text) {
    goldendict::core::FullTextQuery query;
    query.text = std::move(text);
    return query;
}

}  // namespace

class FullTextRequestControllerTest final : public QObject {
    Q_OBJECT

   private slots:
    void CompletesOnGuiThreadAndPreservesResponse();
    void ReplacementCancelsRunningAndPendingRequests();
    void ExplicitCancellationIsIdempotent();
    void ContainsAndRedactsExceptions();
    void RejectsStaleAndDetachedCompletions();
    void ReplacesServiceOnlyAfterJoin();
    void DestructionAndStopJoinWorker();
    void MatchPlanUsesByValueWorkerAndGuiDelivery();
    void MatchPlanPreservesTypedResults();
    void MatchPlanCancelsReplacementPendingDetachAndFacadeChange();
    void MatchPlanContainsExceptionsAndRejectsDuplicateGenerations();
};

void FullTextRequestControllerTest::CompletesOnGuiThreadAndPreservesResponse() {
    FakeDictionaryService service;
    service.response_.partial = true;
    service.response_.results.push_back({});
    service.response_.results.front().headword = "needle";
    service.response_.errors.push_back(
        {goldendict::core::FullTextErrorCode::kDeadlineExceeded, "slow",
         "deadline"});
    service.response_.errors.push_back(
        {goldendict::core::FullTextErrorCode::kCancelled, "cancelled",
         "cancelled by core"});
    service.response_.errors.push_back(
        {goldendict::core::FullTextErrorCode::kDictionaryUnavailable, "missing",
         "unavailable"});
    service.response_.errors.push_back(
        {goldendict::core::FullTextErrorCode::kMalformedIndex, "broken",
         "backend error"});

    int completions = 0;
    QThread* completion_thread = nullptr;
    goldendict::core::FullTextResponse received;
    FullTextRequestController controller(
        [&](std::uint64_t generation,
            goldendict::core::FullTextResponse response) {
            QCOMPARE(generation, 1U);
            ++completions;
            completion_thread = QThread::currentThread();
            received = std::move(response);
        });
    controller.SetService(&service);
    controller.Submit(Query("needle"), 1U);

    QTRY_COMPARE(completions, 1);
    QCOMPARE(service.worker_thread() == QThread::currentThread(), false);
    QCOMPARE(completion_thread, QThread::currentThread());
    QCOMPARE(received.partial, true);
    QCOMPARE(received.results.size(), std::size_t{1});
    QCOMPARE(received.results.front().headword, std::string("needle"));
    QCOMPARE(received.errors.front().code,
             goldendict::core::FullTextErrorCode::kDeadlineExceeded);
    QCOMPARE(received.errors.front().dictionary_id, std::string("slow"));
    QCOMPARE(received.errors.at(1).code,
             goldendict::core::FullTextErrorCode::kCancelled);
    QCOMPARE(received.errors.at(2).code,
             goldendict::core::FullTextErrorCode::kDictionaryUnavailable);
    QCOMPARE(received.errors.at(3).code,
             goldendict::core::FullTextErrorCode::kMalformedIndex);
    QCOMPARE(received.errors.at(3).message, std::string("backend error"));
    QCOMPARE(controller.IsRunning(), false);
}

void FullTextRequestControllerTest::
    ReplacementCancelsRunningAndPendingRequests() {
    FakeDictionaryService blocked;
    blocked.behavior_ = FakeDictionaryService::Behavior::kWaitForCancellation;
    blocked.hold_after_cancellation_ = true;
    int completions = 0;
    FullTextRequestController controller(
        [&](std::uint64_t, goldendict::core::FullTextResponse) {
            ++completions;
        });
    controller.SetService(&blocked);
    controller.Submit(Query("running"), 1U);
    QVERIFY(blocked.WaitForCalls(1));
    controller.Submit(Query("never-starts"), 2U);
    controller.Submit(Query("replacement"), 3U);
    QVERIFY(blocked.WaitForCancellation());
    blocked.ReleaseCancellation();
    QTRY_COMPARE(blocked.calls(), 2);
    QCOMPARE(blocked.queries().at(1), std::string("replacement"));
    controller.Cancel();
    QVERIFY(blocked.WaitForCancellation());
    QCoreApplication::processEvents();
    QCOMPARE(completions, 0);
}

void FullTextRequestControllerTest::ExplicitCancellationIsIdempotent() {
    FakeDictionaryService service;
    service.behavior_ = FakeDictionaryService::Behavior::kWaitForCancellation;
    int completions = 0;
    FullTextRequestController controller(
        [&](std::uint64_t, goldendict::core::FullTextResponse) {
            ++completions;
        });
    controller.SetService(&service);
    controller.Submit(Query("cancel"), 1U);
    QVERIFY(service.WaitForCalls(1));
    controller.Cancel();
    controller.Cancel();
    QVERIFY(service.WaitForCancellation());
    QCoreApplication::processEvents();
    QCOMPARE(completions, 0);
    QCOMPARE(controller.IsRunning(), false);
}

void FullTextRequestControllerTest::ContainsAndRedactsExceptions() {
    FakeDictionaryService service;
    service.behavior_ = FakeDictionaryService::Behavior::kThrow;
    goldendict::core::FullTextResponse received;
    int completions = 0;
    FullTextRequestController controller(
        [&](std::uint64_t, goldendict::core::FullTextResponse response) {
            received = std::move(response);
            ++completions;
        });
    controller.SetService(&service);
    controller.Submit(Query("throw"), 1U);

    QTRY_COMPARE(completions, 1);
    QCOMPARE(received.results.empty(), true);
    QCOMPARE(received.errors.size(), std::size_t{1});
    QCOMPARE(received.errors.front().code,
             goldendict::core::FullTextErrorCode::kInternal);
    QCOMPARE(received.errors.front().dictionary_id.empty(), true);
    QCOMPARE(received.errors.front().message,
             std::string("Full-text search failed internally."));
    QVERIFY(received.errors.front().message.find("/home/") ==
            std::string::npos);
}

void FullTextRequestControllerTest::RejectsStaleAndDetachedCompletions() {
    FakeDictionaryService service;
    int completions = 0;
    FullTextRequestController controller(
        [&](std::uint64_t, goldendict::core::FullTextResponse) {
            ++completions;
        });
    controller.SetService(&service);
    controller.Submit(Query("old"), 1U);
    QVERIFY(service.WaitForCalls(1));
    controller.Submit(Query("new"), 2U);
    QTRY_COMPARE(service.calls(), 2);
    QTRY_COMPARE(completions, 1);

    controller.Submit(Query("detached"), 3U);
    QVERIFY(service.WaitForCalls(3));
    controller.DetachConsumer();
    QCoreApplication::processEvents();
    QCOMPARE(completions, 1);
}

void FullTextRequestControllerTest::ReplacesServiceOnlyAfterJoin() {
    FakeDictionaryService old_service;
    old_service.behavior_ =
        FakeDictionaryService::Behavior::kWaitForCancellation;
    FakeDictionaryService new_service;
    int completions = 0;
    FullTextRequestController controller(
        [&](std::uint64_t, goldendict::core::FullTextResponse) {
            ++completions;
        });
    controller.SetService(&old_service);
    controller.Submit(Query("old"), 1U);
    QVERIFY(old_service.WaitForCalls(1));
    controller.SetService(&new_service);
    QVERIFY(old_service.WaitForCancellation());
    controller.Submit(Query("new"), 2U);
    QTRY_COMPARE(completions, 1);
    QCOMPARE(new_service.calls(), 1);
}

void FullTextRequestControllerTest::DestructionAndStopJoinWorker() {
    FakeDictionaryService service;
    service.behavior_ = FakeDictionaryService::Behavior::kWaitForCancellation;
    {
        FullTextRequestController controller(
            [](std::uint64_t, goldendict::core::FullTextResponse) {});
        controller.SetService(&service);
        controller.Submit(Query("shutdown"), 1U);
        QVERIFY(service.WaitForCalls(1));
        controller.Stop();
        controller.Stop();
    }
    QVERIFY(service.WaitForCancellation());
}

void FullTextRequestControllerTest::MatchPlanUsesByValueWorkerAndGuiDelivery() {
    FakeDesktopFacade facade;
    facade.result_.ranges.push_back({1U, 2U, "bc"});
    int completions = 0;
    QThread* completion_thread = nullptr;
    goldendict::core::RenderedTextMatchPlanResult received;
    RenderedTextMatchPlanController controller(
        [&](std::uint64_t generation,
            goldendict::core::RenderedTextMatchPlanResult result) {
            QCOMPARE(generation, 1U);
            ++completions;
            completion_thread = QThread::currentThread();
            received = std::move(result);
        });
    controller.SetFacade(&facade);
    goldendict::core::RenderedTextMatchPlanRequest request;
    request.rendered_text = "abcd";
    request.query_text = "bc";
    request.match_case = true;
    request.ignore_diacritics = true;
    controller.Submit(request, 1U);
    request.rendered_text = "mutated";
    request.query_text = "mutated";
    request.ignore_diacritics = false;

    QTRY_COMPARE(completions, 1);
    QCOMPARE(facade.worker_thread() == QThread::currentThread(), false);
    QCOMPARE(completion_thread, QThread::currentThread());
    QVERIFY(facade.explicit_cancellation());
    QCOMPARE(facade.requests().front().rendered_text, std::string("abcd"));
    QCOMPARE(facade.requests().front().query_text, std::string("bc"));
    QCOMPARE(facade.requests().front().match_case, true);
    QCOMPARE(facade.requests().front().ignore_diacritics, true);
    QCOMPARE(received.ranges.size(), std::size_t{1});
    QCOMPARE(received.ranges.front().literal, std::string("bc"));
}

void FullTextRequestControllerTest::MatchPlanPreservesTypedResults() {
    const std::vector errors = {
        goldendict::core::RenderedTextMatchPlanError::kNone,
        goldendict::core::RenderedTextMatchPlanError::kNone,
        goldendict::core::RenderedTextMatchPlanError::kInvalidRequest,
        goldendict::core::RenderedTextMatchPlanError::kMalformedPattern,
        goldendict::core::RenderedTextMatchPlanError::kCancelled,
        goldendict::core::RenderedTextMatchPlanError::kDeadlineExceeded,
        goldendict::core::RenderedTextMatchPlanError::kResourceLimit,
        goldendict::core::RenderedTextMatchPlanError::kInternal};
    for (std::size_t index = 0; index < errors.size(); ++index) {
        FakeDesktopFacade facade;
        facade.result_.error = errors[index];
        facade.result_.message = "typed";
        if (index == 1U)
            facade.result_.ranges.push_back({0U, 1U, "x"});
        int completions = 0;
        goldendict::core::RenderedTextMatchPlanResult received;
        RenderedTextMatchPlanController controller(
            [&](std::uint64_t,
                goldendict::core::RenderedTextMatchPlanResult result) {
                received = std::move(result);
                ++completions;
            });
        controller.SetFacade(&facade);
        goldendict::core::RenderedTextMatchPlanRequest request;
        request.rendered_text = index == 1U ? "x" : "";
        request.query_text = "x";
        controller.Submit(std::move(request), index + 1U);
        QTRY_COMPARE(completions, 1);
        QCOMPARE(received.error, errors[index]);
        QCOMPARE(received.message, std::string("typed"));
        QCOMPARE(received.ranges.size(), facade.result_.ranges.size());
        if (!received.ranges.empty()) {
            QCOMPARE(received.ranges.front().byte_offset,
                     facade.result_.ranges.front().byte_offset);
            QCOMPARE(received.ranges.front().byte_length,
                     facade.result_.ranges.front().byte_length);
            QCOMPARE(received.ranges.front().literal,
                     facade.result_.ranges.front().literal);
        }
    }
}

void FullTextRequestControllerTest::
    MatchPlanCancelsReplacementPendingDetachAndFacadeChange() {
    FakeDesktopFacade old_facade;
    old_facade.behavior_ = FakeDesktopFacade::Behavior::kWaitForCancellation;
    old_facade.hold_after_cancellation_ = true;
    FakeDesktopFacade replacement_facade;
    int completions = 0;
    RenderedTextMatchPlanController controller(
        [&](std::uint64_t, goldendict::core::RenderedTextMatchPlanResult) {
            ++completions;
        });
    controller.SetFacade(&old_facade);
    goldendict::core::RenderedTextMatchPlanRequest request;
    request.rendered_text = "text";
    request.query_text = "running";
    request.ignore_diacritics = false;
    controller.Submit(request, 1U);
    QVERIFY(old_facade.WaitForCalls(1));
    request.query_text = "pending";
    request.ignore_diacritics = false;
    controller.Submit(request, 2U);
    request.query_text = "latest";
    request.ignore_diacritics = true;
    controller.Submit(request, 3U);
    QVERIFY(old_facade.WaitForCancellation());
    old_facade.ReleaseCancellation();
    QTRY_COMPARE(old_facade.calls(), 2);
    QCOMPARE(old_facade.requests().back().query_text, std::string("latest"));
    QCOMPARE(old_facade.requests().back().ignore_diacritics, true);
    controller.Cancel();
    QVERIFY(old_facade.WaitForCancellation());
    controller.SetFacade(&replacement_facade);
    request.query_text = "replacement";
    request.ignore_diacritics = false;
    controller.Submit(request, 4U);
    QTRY_COMPARE(completions, 1);
    QCOMPARE(replacement_facade.calls(), 1);
    QCOMPARE(replacement_facade.requests().front().ignore_diacritics, false);
    controller.DetachConsumer();
    controller.Submit(request, 5U);
    QCoreApplication::processEvents();
    QCOMPARE(completions, 1);
}

void FullTextRequestControllerTest::
    MatchPlanContainsExceptionsAndRejectsDuplicateGenerations() {
    FakeDesktopFacade facade;
    facade.behavior_ = FakeDesktopFacade::Behavior::kThrow;
    int completions = 0;
    goldendict::core::RenderedTextMatchPlanResult received;
    RenderedTextMatchPlanController controller(
        [&](std::uint64_t,
            goldendict::core::RenderedTextMatchPlanResult result) {
            received = std::move(result);
            ++completions;
        });
    controller.SetFacade(&facade);
    goldendict::core::RenderedTextMatchPlanRequest request;
    request.rendered_text = "text";
    request.query_text = "query";
    controller.Submit(request, 7U);
    controller.Submit(request, 7U);
    controller.Submit(request, 6U);
    QTRY_COMPARE(completions, 1);
    QCOMPARE(facade.calls(), 1);
    QCOMPARE(received.error,
             goldendict::core::RenderedTextMatchPlanError::kInternal);
    QCOMPARE(received.message,
             std::string("Rendered-text match-plan worker failed internally."));
}

QTEST_GUILESS_MAIN(FullTextRequestControllerTest)

#include "full_text_request_controller_test.moc"
