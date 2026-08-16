// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#include "full_text_request_controller.h"

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

QTEST_GUILESS_MAIN(FullTextRequestControllerTest)

#include "full_text_request_controller_test.moc"
