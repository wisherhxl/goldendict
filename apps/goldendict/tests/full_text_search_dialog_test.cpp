// SPDX-License-Identifier: GPL-3.0-or-later

#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QtTest>

#include <condition_variable>
#include <mutex>
#include <utility>

#include "full_text_search_dialog.h"

namespace goldendict::app {
namespace {

class ControllableDictionaryService final
    : public goldendict::core::DictionaryService {
   public:
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
            queries_.push_back(query);
        }
        changed_.notify_all();
        if (query.text == "blocked") {
            std::unique_lock lock(mutex_);
            while (!cancellation->IsCancellationRequested())
                changed_.wait_for(lock, std::chrono::milliseconds(5));
            cancellation_seen_ = true;
            changed_.notify_all();
            changed_.wait(lock, [this]() { return release_cancelled_; });
            goldendict::core::FullTextResponse cancelled;
            cancelled.errors.push_back(
                {goldendict::core::FullTextErrorCode::kCancelled, "old",
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

    bool WaitForQueries(std::size_t count) const {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(
            lock, std::chrono::seconds(2),
            [this, count]() { return queries_.size() >= count; });
    }

    bool WaitForCancellation() const {
        std::unique_lock lock(mutex_);
        return changed_.wait_for(lock, std::chrono::seconds(2),
                                 [this]() { return cancellation_seen_; });
    }

    void ReleaseCancelledRequest() {
        {
            const std::lock_guard lock(mutex_);
            release_cancelled_ = true;
        }
        changed_.notify_all();
    }

    std::vector<goldendict::core::FullTextQuery> Queries() const {
        const std::lock_guard lock(mutex_);
        return queries_;
    }

    goldendict::core::FullTextResponse response_;

   private:
    mutable std::mutex mutex_;
    mutable std::condition_variable changed_;
    mutable std::vector<goldendict::core::FullTextQuery> queries_;
    mutable bool cancellation_seen_ = false;
    mutable bool release_cancelled_ = false;
};

QPushButton* SearchButton(FullTextSearchDialog* dialog) {
    return dialog->findChild<QPushButton*>(
        QStringLiteral("fullTextSearchButton"));
}

QPushButton* CancelButton(FullTextSearchDialog* dialog) {
    return dialog->findChild<QPushButton*>(
        QStringLiteral("fullTextCancelButton"));
}

QProgressBar* Progress(FullTextSearchDialog* dialog) {
    return dialog->findChild<QProgressBar*>(
        QStringLiteral("fullTextSearchProgress"));
}

}  // namespace

class FullTextSearchDialogTest final : public QObject {
    Q_OBJECT

   private slots:
    void SubmitsExactComposedAndProjectedQueryAndRetainsResponse();
    void ReplacesRunningAndPendingGenerationsAndSuppressesStaleCompletion();
    void CancellationIsIdempotentAndRestoresIdleState();
    void ServiceReplacementDetachAndDestructionSuppressLateDelivery();
};

void FullTextSearchDialogTest::
    SubmitsExactComposedAndProjectedQueryAndRetainsResponse() {
    ControllableDictionaryService service;
    service.response_.partial = true;
    service.response_.results.push_back({});
    service.response_.results.front().headword = "retained";
    service.response_.errors.push_back(
        {goldendict::core::FullTextErrorCode::kMalformedIndex, "broken",
         "backend error"});
    goldendict::core::ApplicationPreferences preferences;
    preferences.full_text_match_case = true;
    preferences.full_text_ignore_diacritics = true;
    preferences.full_text_ignore_word_order = true;
    preferences.full_text_use_maximum_word_distance = true;
    preferences.full_text_maximum_word_distance = 7U;
    preferences.full_text_use_maximum_articles = true;
    preferences.full_text_maximum_articles_per_dictionary = 23U;
    FullTextSearchDialog dialog(preferences, &service);
    dialog.InitializeQuery(QStringLiteral("exact needle"));

    goldendict::core::FullTextQuery projected;
    projected.text = "stale text";
    projected.dictionary_ids = {"second", "first"};
    projected.dictionary_filter_active = true;
    dialog.SetProjectedQuery(projected);

    auto* search = SearchButton(&dialog);
    auto* cancel = CancelButton(&dialog);
    auto* progress = Progress(&dialog);
    QVERIFY(search != nullptr);
    QVERIFY(cancel != nullptr);
    QVERIFY(progress != nullptr);
    QVERIFY(search->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QVERIFY(progress->isHidden());

    search->click();
    QVERIFY(!search->isEnabled());
    QVERIFY(cancel->isEnabled());
    QVERIFY(!progress->isHidden());
    QCOMPARE(progress->minimum(), 0);
    QCOMPARE(progress->maximum(), 0);
    QVERIFY(service.WaitForQueries(1U));
    const auto submitted = service.Queries().front();
    QCOMPARE(submitted.text, std::string("exact needle"));
    QCOMPARE(submitted.mode, goldendict::core::FullTextQueryMode::kWholeWords);
    QVERIFY(submitted.match_case);
    QVERIFY(submitted.ignore_diacritics);
    QVERIFY(submitted.ignore_word_order);
    QCOMPARE(submitted.maximum_word_distance, std::optional<std::uint32_t>(7U));
    QCOMPARE(submitted.maximum_articles_per_dictionary,
             std::optional<std::size_t>(23U));
    QCOMPARE(submitted.result_limit, std::size_t{100000U});
    QCOMPARE(submitted.dictionary_ids, projected.dictionary_ids);
    QVERIFY(submitted.dictionary_filter_active);

    QTRY_VERIFY(dialog.response_.has_value());
    QVERIFY(search->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QVERIFY(progress->isHidden());
    QVERIFY(dialog.response_->partial);
    QCOMPARE(dialog.response_->results.size(), std::size_t{1});
    QCOMPARE(dialog.response_->results.front().headword,
             std::string("retained"));
    QCOMPARE(dialog.response_->errors.size(), std::size_t{1});
    QCOMPARE(dialog.response_->errors.front().dictionary_id,
             std::string("broken"));
    QCOMPARE(dialog.generation_, 1U);
}

void FullTextSearchDialogTest::
    ReplacesRunningAndPendingGenerationsAndSuppressesStaleCompletion() {
    ControllableDictionaryService service;
    service.response_.results.push_back({});
    service.response_.results.front().headword = "current";
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* query =
        dialog.findChild<QLineEdit*>(QStringLiteral("fullTextQueryText"));
    auto* search = SearchButton(&dialog);
    QVERIFY(query != nullptr);

    query->setText(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));
    QVERIFY(!search->isEnabled());
    query->setText(QStringLiteral("never starts"));
    dialog.SubmitSearch();
    query->setText(QStringLiteral("replacement"));
    dialog.SubmitSearch();
    QCOMPARE(dialog.generation_, 3U);
    QCOMPARE(dialog.active_generation_, std::optional<std::uint64_t>(3U));
    QVERIFY(service.WaitForCancellation());
    service.ReleaseCancelledRequest();
    QVERIFY(service.WaitForQueries(2U));
    QTRY_VERIFY(dialog.response_.has_value());

    const auto queries = service.Queries();
    QCOMPARE(queries.size(), std::size_t{2});
    QCOMPARE(queries[0].text, std::string("blocked"));
    QCOMPARE(queries[1].text, std::string("replacement"));
    QCOMPARE(dialog.response_->results.front().headword,
             std::string("current"));
    QVERIFY(!dialog.active_generation_.has_value());
    QVERIFY(search->isEnabled());
}

void FullTextSearchDialogTest::CancellationIsIdempotentAndRestoresIdleState() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    dialog.InitializeQuery(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));

    CancelButton(&dialog)->click();
    dialog.CancelSearch();
    QVERIFY(service.WaitForCancellation());
    QVERIFY(!dialog.active_generation_.has_value());
    QVERIFY(!dialog.response_.has_value());
    QVERIFY(SearchButton(&dialog)->isEnabled());
    QVERIFY(!CancelButton(&dialog)->isEnabled());
    QVERIFY(Progress(&dialog)->isHidden());

    service.ReleaseCancelledRequest();
    QTest::qWait(20);
    QCoreApplication::processEvents();
    QVERIFY(!dialog.response_.has_value());
    QCOMPARE(dialog.generation_, 1U);
}

void FullTextSearchDialogTest::
    ServiceReplacementDetachAndDestructionSuppressLateDelivery() {
    ControllableDictionaryService first;
    ControllableDictionaryService second;
    goldendict::core::ApplicationPreferences preferences;
    {
        FullTextSearchDialog dialog(preferences, &first);
        dialog.InitializeQuery(QStringLiteral("blocked"));
        dialog.SubmitSearch();
        QVERIFY(first.WaitForQueries(1U));
        first.ReleaseCancelledRequest();
        dialog.SetService(&second);
        QVERIFY(first.WaitForCancellation());
        QVERIFY(!dialog.active_generation_.has_value());
        QVERIFY(!dialog.response_.has_value());
        QVERIFY(SearchButton(&dialog)->isEnabled());
        dialog.DetachController();
        dialog.DetachController();
    }
    QCOMPARE(second.Queries().size(), std::size_t{0});
}

}  // namespace goldendict::app

QTEST_MAIN(goldendict::app::FullTextSearchDialogTest)

#include "full_text_search_dialog_test.moc"
