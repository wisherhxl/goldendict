// SPDX-License-Identifier: GPL-3.0-or-later

#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QProgressBar>
#include <QProxyStyle>
#include <QPushButton>
#include <QStyleOptionViewItem>
#include <QtTest>

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "full_text_response_model.h"
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
        const std::lock_guard lock(mutex_);
        ++lookup_calls_;
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
        const std::lock_guard lock(mutex_);
        ++lookup_calls_;
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

    std::size_t LookupCalls() const {
        const std::lock_guard lock(mutex_);
        return lookup_calls_;
    }

    goldendict::core::FullTextResponse response_;

   private:
    mutable std::mutex mutex_;
    mutable std::condition_variable changed_;
    mutable std::vector<goldendict::core::FullTextQuery> queries_;
    mutable std::size_t lookup_calls_ = 0U;
    mutable bool cancellation_seen_ = false;
    mutable bool release_cancelled_ = false;
};

class RecordingItemStyle final : public QProxyStyle {
   public:
    using QProxyStyle::QProxyStyle;

    void drawControl(ControlElement element, const QStyleOption* option,
                     QPainter* painter,
                     const QWidget* widget = nullptr) const override {
        if (element == CE_ItemViewItem) {
            const auto* item =
                qstyleoption_cast<const QStyleOptionViewItem*>(option);
            if (item != nullptr) {
                item_options.push_back(*item);
            }
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    mutable std::vector<QStyleOptionViewItem> item_options;
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

FullTextResponseModel* ResponseModel(FullTextSearchDialog* dialog) {
    return dialog->findChild<FullTextResponseModel*>();
}

QListView* Results(FullTextSearchDialog* dialog) {
    return dialog->findChild<QListView*>(
        QStringLiteral("fullTextSearchResults"));
}

QLabel* ArticlesFound(FullTextSearchDialog* dialog) {
    return dialog->findChild<QLabel*>(
        QStringLiteral("fullTextArticlesFoundLabel"));
}

QLabel* PartialStatus(FullTextSearchDialog* dialog) {
    return dialog->findChild<QLabel*>(
        QStringLiteral("fullTextPartialResponseStatus"));
}

QLabel* EmptyStatus(FullTextSearchDialog* dialog) {
    return dialog->findChild<QLabel*>(
        QStringLiteral("fullTextEmptyResponseStatus"));
}

goldendict::core::FullTextResult MakeResult(const std::string& id,
                                            const std::string& headword,
                                            std::size_t ordinal) {
    goldendict::core::FullTextResult result;
    result.dictionary.id = id;
    result.dictionary.name = "Dictionary " + id;
    result.dictionary.edition = "edition-" + std::to_string(ordinal);
    result.dictionary.source = "/source/" + id;
    result.dictionary.description = "description-" + id;
    result.dictionary.article_count = 100U + ordinal;
    result.dictionary.headword_count = 200U + ordinal;
    result.dictionary.source_language = "en";
    result.dictionary.target_language = "ja";
    result.dictionary.supports_headword_enumeration = ordinal % 2U == 0U;
    result.dictionary.supports_full_text_search = true;
    result.headword = headword;
    result.document_id = "document-" + std::to_string(ordinal);
    result.match.requested_headword = "requested-" + std::to_string(ordinal);
    result.match.normalized_headword = "normalized-" + std::to_string(ordinal);
    result.match.mode = goldendict::core::MatchMode::kPrefix;
    result.match.score = 0.5 + static_cast<double>(ordinal);
    result.excerpt = "excerpt-" + std::to_string(ordinal);
    result.matches = {{ordinal, 3U, "one"}, {ordinal + 10U, 7U, "second"}};
    return result;
}

void CompareResult(const goldendict::core::FullTextResult& actual,
                   const goldendict::core::FullTextResult& expected) {
    QCOMPARE(actual.dictionary.id, expected.dictionary.id);
    QCOMPARE(actual.dictionary.name, expected.dictionary.name);
    QCOMPARE(actual.dictionary.edition, expected.dictionary.edition);
    QCOMPARE(actual.dictionary.source, expected.dictionary.source);
    QCOMPARE(actual.dictionary.description, expected.dictionary.description);
    QCOMPARE(actual.dictionary.article_count,
             expected.dictionary.article_count);
    QCOMPARE(actual.dictionary.headword_count,
             expected.dictionary.headword_count);
    QCOMPARE(actual.dictionary.source_language,
             expected.dictionary.source_language);
    QCOMPARE(actual.dictionary.target_language,
             expected.dictionary.target_language);
    QCOMPARE(actual.dictionary.supports_headword_enumeration,
             expected.dictionary.supports_headword_enumeration);
    QCOMPARE(actual.dictionary.supports_full_text_search,
             expected.dictionary.supports_full_text_search);
    QCOMPARE(actual.headword, expected.headword);
    QCOMPARE(actual.document_id, expected.document_id);
    QCOMPARE(actual.match.requested_headword,
             expected.match.requested_headword);
    QCOMPARE(actual.match.normalized_headword,
             expected.match.normalized_headword);
    QCOMPARE(actual.match.mode, expected.match.mode);
    QCOMPARE(actual.match.score, expected.match.score);
    QCOMPARE(actual.excerpt, expected.excerpt);
    QCOMPARE(actual.matches.size(), expected.matches.size());
    for (std::size_t i = 0; i < expected.matches.size(); ++i) {
        QCOMPARE(actual.matches[i].byte_offset,
                 expected.matches[i].byte_offset);
        QCOMPARE(actual.matches[i].byte_length,
                 expected.matches[i].byte_length);
        QCOMPARE(actual.matches[i].text, expected.matches[i].text);
    }
}

void CompareIntent(const FullTextResultActivationIntent& actual,
                   const goldendict::core::FullTextResult& expected_result,
                   bool expected_filter_active,
                   const std::vector<std::string>& expected_ids) {
    CompareResult(actual.result, expected_result);
    QCOMPARE(actual.dictionary_filter_active, expected_filter_active);
    QCOMPARE(actual.dictionary_ids, expected_ids);
}

}  // namespace

class FullTextSearchDialogTest final : public QObject {
    Q_OBJECT

   private slots:
    void SubmitsExactComposedAndProjectedQueryAndRetainsResponse();
    void PaintsEachResultWithIndependentDirectionAndElision();
    void KeepsSelectionAndFocusDeterministicAcrossAcceptedResponses();
    void RetainsExactAcceptedScopeForByValueActivation();
    void ProjectsCompleteCurrentResponsesAndReplacesRowsAtomically();
    void ActivatesExactCurrentResultOnceFromMouseAndKeyboard();
    void SuppressesDuplicateInvalidStaleAndCancelledActivation();
    void ReplacesRunningAndPendingGenerationsAndSuppressesStaleCompletion();
    void CancellationIsIdempotentAndRestoresIdleState();
    void ServiceReplacementDetachAndDestructionSuppressLateDelivery();
};

void FullTextSearchDialogTest::
    PaintsEachResultWithIndependentDirectionAndElision() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* model = ResponseModel(&dialog);
    auto* results = Results(&dialog);
    QVERIFY(model != nullptr);
    QVERIFY(results != nullptr);

    const QString left_to_right = QStringLiteral("alpha");
    const QString right_to_left = QString::fromUtf8(u8"مرحبا");
    const QString mixed = QString::fromUtf8(u8"مرحبا alpha");
    QVERIFY(!left_to_right.isRightToLeft());
    QVERIFY(right_to_left.isRightToLeft());
    QVERIFY(mixed.isRightToLeft());

    goldendict::core::FullTextResponse response;
    response.results = {
        MakeResult("ltr", left_to_right.toStdString(), 1U),
        MakeResult("rtl", right_to_left.toStdString(), 2U),
        MakeResult("mixed", mixed.toStdString(), 3U),
        MakeResult("duplicate", right_to_left.toStdString(), 4U),
    };
    dialog.active_generation_ = ++dialog.generation_;
    dialog.FinishSearch(dialog.generation_, response);
    QCOMPARE(model->rowCount(), 4);
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 4"));
    QVERIFY(!results->currentIndex().isValid());
    QVERIFY(!results->selectionModel()->hasSelection());

    RecordingItemStyle style;
    results->setStyle(&style);
    QImage image(320, 80, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const auto paint_rows = [&](Qt::TextElideMode elide_mode) {
        style.item_options.clear();
        for (int row = 0; row < model->rowCount(); ++row) {
            QStyleOptionViewItem option;
            option.initFrom(results);
            option.widget = results;
            option.rect = QRect(0, row * 20, image.width(), 20);
            option.textElideMode = elide_mode;
            results->itemDelegate()->paint(&painter, option,
                                           model->index(row, 0));
        }
        QCOMPARE(style.item_options.size(),
                 static_cast<std::size_t>(model->rowCount()));
    };

    paint_rows(Qt::ElideMiddle);
    QCOMPARE(style.item_options[0].text, left_to_right);
    QCOMPARE(style.item_options[0].direction, Qt::LeftToRight);
    QCOMPARE(style.item_options[0].textElideMode, Qt::ElideRight);
    QCOMPARE(style.item_options[1].text, right_to_left);
    QCOMPARE(style.item_options[1].direction, Qt::RightToLeft);
    QCOMPARE(style.item_options[1].textElideMode, Qt::ElideLeft);
    QCOMPARE(style.item_options[2].text, mixed);
    QCOMPARE(style.item_options[2].direction, Qt::RightToLeft);
    QCOMPARE(style.item_options[2].textElideMode, Qt::ElideLeft);
    QCOMPARE(style.item_options[3].text, right_to_left);
    QCOMPARE(style.item_options[3].direction, Qt::RightToLeft);
    QCOMPARE(style.item_options[3].textElideMode, Qt::ElideLeft);

    paint_rows(Qt::ElideNone);
    for (const auto& option : style.item_options) {
        QCOMPARE(option.textElideMode, Qt::ElideNone);
    }

    goldendict::core::FullTextResponse replacement;
    replacement.results = {
        MakeResult("replacement", left_to_right.toStdString(), 5U)};
    dialog.active_generation_ = ++dialog.generation_;
    dialog.FinishSearch(dialog.generation_, replacement);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 1"));
    QVERIFY(PartialStatus(&dialog)->isHidden());
    QVERIFY(EmptyStatus(&dialog)->isHidden());
    QVERIFY(!results->currentIndex().isValid());
    QVERIFY(!results->selectionModel()->hasSelection());
    QCOMPARE(model->data(model->index(0, 0), Qt::DisplayRole).toString(),
             left_to_right);
    QCOMPARE(model->data(model->index(0, 0), Qt::EditRole).toString(),
             left_to_right);
    QCOMPARE(model->data(model->index(0, 0), Qt::ToolTipRole).toString(),
             QStringLiteral("Dictionary replacement"));
    paint_rows(Qt::ElideMiddle);
    QCOMPARE(style.item_options.front().direction, Qt::LeftToRight);
    QCOMPARE(style.item_options.front().textElideMode, Qt::ElideRight);

    results->setStyle(nullptr);
}

void FullTextSearchDialogTest::
    ActivatesExactCurrentResultOnceFromMouseAndKeyboard() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* model = ResponseModel(&dialog);
    auto* results = Results(&dialog);
    const auto expected = MakeResult("exact-id", u8"café", 4U);
    service.response_.results.push_back(expected);
    dialog.InitializeQuery(QStringLiteral("needle"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));
    QTRY_COMPARE(model->rowCount(), 1);

    std::vector<FullTextResultActivationIntent> activations;
    connect(&dialog, &FullTextSearchDialog::ResultActivationRequested, &dialog,
            [&activations](FullTextResultActivationIntent intent) {
                activations.push_back(std::move(intent));
            });
    dialog.show();
    QTRY_VERIFY(results->isVisible());
    const QModelIndex index = model->index(0, 0);
    QVERIFY(index.isValid());
    const QPoint center = results->visualRect(index).center();

    QTest::mouseClick(results->viewport(), Qt::LeftButton, Qt::NoModifier,
                      center);
    QCOMPARE(activations.size(), std::size_t{1});
    QCOMPARE(results->currentIndex(), index);
    QCOMPARE(results->selectionModel()->selectedRows(), QModelIndexList{index});
    CompareIntent(activations.back(), expected, false, {});

    results->setCurrentIndex(index);
    results->setFocus();
    QTest::keyClick(results, Qt::Key_Return);
    QCOMPARE(activations.size(), std::size_t{2});
    CompareIntent(activations.back(), expected, false, {});

    QTest::keyClick(results, Qt::Key_Enter, Qt::KeypadModifier);
    QCOMPARE(activations.size(), std::size_t{3});
    CompareIntent(activations.back(), expected, false, {});
    QCOMPARE(service.LookupCalls(), std::size_t{0});
}

void FullTextSearchDialogTest::
    SuppressesDuplicateInvalidStaleAndCancelledActivation() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* model = ResponseModel(&dialog);
    auto* results = Results(&dialog);
    goldendict::core::FullTextResponse first;
    first.results.push_back(MakeResult("first", "first", 1U));
    model->Reset(first);
    dialog.accepted_activation_scope_ =
        FullTextSearchDialog::ActivationScope{false, {}};

    std::vector<FullTextResultActivationIntent> activations;
    connect(&dialog, &FullTextSearchDialog::ResultActivationRequested, &dialog,
            [&activations](FullTextResultActivationIntent intent) {
                activations.push_back(std::move(intent));
            });
    dialog.show();
    QTRY_VERIFY(results->isVisible());
    const QModelIndex stale = model->index(0, 0);
    const QPoint center = results->visualRect(stale).center();

    QTest::mouseClick(results->viewport(), Qt::LeftButton, Qt::NoModifier,
                      center);
    QTest::mouseDClick(results->viewport(), Qt::LeftButton, Qt::NoModifier,
                       center);
    QCOMPARE(activations.size(), std::size_t{1});

    results->setCurrentIndex({});
    dialog.ActivateResult({});
    dialog.ActivateResult(stale);
    QTest::keyClick(results, Qt::Key_Return);
    QCOMPARE(activations.size(), std::size_t{1});

    goldendict::core::FullTextResponse replacement;
    replacement.results.push_back(MakeResult("replacement", "replacement", 2U));
    dialog.accepted_activation_scope_.reset();
    model->Reset(replacement);
    dialog.ActivateResult(stale);
    QCOMPARE(activations.size(), std::size_t{1});

    FullTextResponseModel foreign_model(replacement);
    dialog.ActivateResult(foreign_model.index(0, 0));
    QCOMPARE(activations.size(), std::size_t{1});

    const QModelIndex replacement_index = model->index(0, 0);
    results->setCurrentIndex(replacement_index);
    dialog.ActivateResult(replacement_index);
    QCOMPARE(activations.size(), std::size_t{1});
    dialog.accepted_activation_scope_ =
        FullTextSearchDialog::ActivationScope{true, {"replacement-scope"}};
    dialog.ActivateResult(replacement_index);
    QCOMPARE(activations.size(), std::size_t{2});
    const auto copied = activations.back();
    model->Reset({});
    model->Reset(replacement);
    model->Reset({});
    CompareIntent(copied, replacement.results.front(), true,
                  {"replacement-scope"});
    dialog.ActivateResult(replacement_index);
    QCOMPARE(activations.size(), std::size_t{2});

    dialog.InitializeQuery(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));
    dialog.CancelSearch();
    QVERIFY(service.WaitForCancellation());
    QTest::keyClick(results, Qt::Key_Enter, Qt::KeypadModifier);
    QCOMPARE(activations.size(), std::size_t{2});
    service.ReleaseCancelledRequest();
    QCOMPARE(service.LookupCalls(), std::size_t{0});
}

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
    auto* response_model = ResponseModel(&dialog);
    auto* results = Results(&dialog);
    auto* articles_found = ArticlesFound(&dialog);
    auto* partial_status = PartialStatus(&dialog);
    auto* empty_status = EmptyStatus(&dialog);
    QVERIFY(search != nullptr);
    QVERIFY(cancel != nullptr);
    QVERIFY(progress != nullptr);
    QVERIFY(response_model != nullptr);
    QVERIFY(results != nullptr);
    QVERIFY(articles_found != nullptr);
    QVERIFY(partial_status != nullptr);
    QVERIFY(empty_status != nullptr);
    QCOMPARE(response_model, dialog.response_model_);
    QCOMPARE(results, dialog.results_);
    QCOMPARE(response_model->parent(), &dialog);
    QCOMPARE(results->parent(), &dialog);
    QCOMPARE(articles_found->parent(), &dialog);
    QCOMPARE(partial_status->parent(), &dialog);
    QCOMPARE(empty_status->parent(), &dialog);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QCOMPARE(partial_status->text(),
             QStringLiteral("Results may be incomplete."));
    QCOMPARE(empty_status->text(), QStringLiteral("No matches"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
    QCOMPARE(dialog.findChildren<FullTextResponseModel*>().size(), 1);
    QCOMPARE(
        dialog.findChildren<QListView*>(QString(), Qt::FindDirectChildrenOnly)
            .size(),
        1);
    QCOMPARE(results->model(), response_model);
    QCOMPARE(results->selectionBehavior(), QAbstractItemView::SelectRows);
    QCOMPARE(results->selectionMode(), QAbstractItemView::SingleSelection);
    QCOMPARE(response_model->rowCount(), 0);
    QVERIFY(!results->currentIndex().isValid());
    QVERIFY(!results->selectionModel()->hasSelection());
    dialog.show();
    QTRY_VERIFY(results->isVisible());
    QVERIFY(search->isEnabled());
    QVERIFY(!cancel->isEnabled());
    QVERIFY(progress->isHidden());

    search->click();
    QVERIFY(!search->isEnabled());
    QVERIFY(cancel->isEnabled());
    QVERIFY(!progress->isHidden());
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
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
    QCOMPARE(response_model->rowCount(), 1);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 1"));
    QVERIFY(!partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
    const auto labels = dialog.findChildren<QLabel*>();
    for (const auto* label : labels) {
        QVERIFY(!label->text().contains(QStringLiteral("broken")));
        QVERIFY(!label->text().contains(QStringLiteral("backend error")));
    }
    QCOMPARE(response_model->data(response_model->index(0, 0)).toString(),
             QStringLiteral("retained"));
    QVERIFY(!results->currentIndex().isValid());
    QVERIFY(!results->selectionModel()->hasSelection());
    QCOMPARE(dialog.generation_, 1U);
}

void FullTextSearchDialogTest::
    KeepsSelectionAndFocusDeterministicAcrossAcceptedResponses() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* query =
        dialog.findChild<QLineEdit*>(QStringLiteral("fullTextQueryText"));
    auto* model = ResponseModel(&dialog);
    auto* results = Results(&dialog);
    QVERIFY(query != nullptr);
    QVERIFY(model != nullptr);
    QVERIFY(results != nullptr);
    dialog.show();
    QTRY_VERIFY(results->isVisible());

    const auto finish_current =
        [&dialog](goldendict::core::FullTextResponse response) {
            dialog.active_generation_ = ++dialog.generation_;
            dialog.FinishSearch(dialog.generation_, std::move(response));
        };
    const auto verify_unselected = [results]() {
        QVERIFY(!results->currentIndex().isValid());
        QVERIFY(!results->selectionModel()->hasSelection());
        QCOMPARE(results->selectionModel()->selectedRows().size(), 0);
    };

    query->setFocus();
    QTRY_VERIFY(query->hasFocus());
    goldendict::core::FullTextResponse successful;
    successful.results = {MakeResult("first", "same", 1U),
                          MakeResult("second", "second", 2U)};
    finish_current(successful);
    QCOMPARE(model->rowCount(), 2);
    verify_unselected();
    QVERIFY(query->hasFocus());

    results->setCurrentIndex(model->index(0, 0));
    QCOMPARE(results->currentIndex(), model->index(0, 0));
    const QModelIndexList first_selection{model->index(0, 0)};
    QCOMPARE(results->selectionModel()->selectedRows(), first_selection);
    results->setCurrentIndex(model->index(1, 0));
    QCOMPARE(results->currentIndex(), model->index(1, 0));
    const QModelIndexList second_selection{model->index(1, 0)};
    QCOMPARE(results->selectionModel()->selectedRows(), second_selection);

    results->setFocus();
    QTRY_VERIFY(results->hasFocus());
    service.response_.partial = true;
    service.response_.results = {MakeResult("first", "same", 1U)};
    query->setText(QStringLiteral("replacement"));
    dialog.SubmitSearch();
    QCOMPARE(model->rowCount(), 0);
    verify_unselected();
    QVERIFY(results->hasFocus());
    QVERIFY(service.WaitForQueries(1U));
    QTRY_COMPARE(model->rowCount(), 1);
    QVERIFY(dialog.response_->partial);
    QCOMPARE(model->data(model->index(0, 0)).toString(),
             QStringLiteral("same"));
    verify_unselected();
    QVERIFY(results->hasFocus());

    query->setFocus();
    QTRY_VERIFY(query->hasFocus());
    goldendict::core::FullTextResponse error_only;
    error_only.errors.push_back(
        {goldendict::core::FullTextErrorCode::kMalformedIndex, "broken",
         "error"});
    finish_current(error_only);
    QCOMPARE(model->rowCount(), 0);
    verify_unselected();
    QVERIFY(query->hasFocus());

    goldendict::core::FullTextResponse empty;
    finish_current(empty);
    QCOMPARE(model->rowCount(), 0);
    verify_unselected();
    QVERIFY(query->hasFocus());

    results->setFocus();
    QTRY_VERIFY(results->hasFocus());
    query->setText(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(2U));
    QCOMPARE(model->rowCount(), 0);
    verify_unselected();
    QVERIFY(results->hasFocus());
    dialog.CancelSearch();
    QVERIFY(service.WaitForCancellation());
    service.ReleaseCancelledRequest();
    QTest::qWait(20);
    QCoreApplication::processEvents();
    QCOMPARE(model->rowCount(), 0);
    verify_unselected();
    QVERIFY(results->hasFocus());
}

void FullTextSearchDialogTest::RetainsExactAcceptedScopeForByValueActivation() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* model = ResponseModel(&dialog);
    auto* results = Results(&dialog);
    const auto ordered_result = MakeResult("ordered", "ordered", 5U);
    service.response_.results = {ordered_result};

    goldendict::core::FullTextQuery ordered_scope;
    ordered_scope.dictionary_filter_active = true;
    ordered_scope.dictionary_ids = {"second", "first", "second"};
    dialog.SetProjectedQuery(ordered_scope);
    dialog.InitializeQuery(QStringLiteral("ordered"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));
    QTRY_COMPARE(model->rowCount(), 1);
    QVERIFY(dialog.accepted_activation_scope_.has_value());
    QVERIFY(dialog.accepted_activation_scope_->dictionary_filter_active);
    QCOMPARE(dialog.accepted_activation_scope_->dictionary_ids,
             ordered_scope.dictionary_ids);

    std::vector<FullTextResultActivationIntent> activations;
    connect(&dialog, &FullTextSearchDialog::ResultActivationRequested, &dialog,
            [&activations](FullTextResultActivationIntent intent) {
                activations.push_back(std::move(intent));
            });
    goldendict::core::FullTextQuery later_scope;
    later_scope.dictionary_filter_active = true;
    later_scope.dictionary_ids = {"later"};
    dialog.SetProjectedQuery(later_scope);
    dialog.InitializeQuery(QStringLiteral("changed composer"));
    const QModelIndex ordered_index = model->index(0, 0);
    results->setCurrentIndex(ordered_index);
    dialog.ActivateResult(ordered_index);
    QCOMPARE(activations.size(), std::size_t{1});
    CompareIntent(activations.back(), ordered_result, true,
                  ordered_scope.dictionary_ids);

    activations.back().result.headword = "mutated copy";
    activations.back().dictionary_ids.clear();
    dialog.ActivateResult(ordered_index);
    QCOMPARE(activations.size(), std::size_t{2});
    CompareIntent(activations.back(), ordered_result, true,
                  ordered_scope.dictionary_ids);

    dialog.InitializeQuery(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(2U));
    QVERIFY(!dialog.response_.has_value());
    QCOMPARE(model->rowCount(), 0);
    QVERIFY(!dialog.accepted_activation_scope_.has_value());
    dialog.ActivateResult(ordered_index);
    QCOMPARE(activations.size(), std::size_t{2});
    dialog.CancelSearch();
    QVERIFY(service.WaitForCancellation());
    service.ReleaseCancelledRequest();

    const auto empty_result = MakeResult("empty", "empty", 6U);
    service.response_.results = {empty_result};
    goldendict::core::FullTextQuery empty_scope;
    empty_scope.dictionary_filter_active = true;
    dialog.SetProjectedQuery(empty_scope);
    dialog.InitializeQuery(QStringLiteral("empty"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(3U));
    QTRY_COMPARE(model->rowCount(), 1);
    const QModelIndex empty_index = model->index(0, 0);
    results->setCurrentIndex(empty_index);
    dialog.ActivateResult(empty_index);
    QCOMPARE(activations.size(), std::size_t{3});
    CompareIntent(activations.back(), empty_result, true, {});

    const auto absent_result = MakeResult("absent", "absent", 7U);
    service.response_.results = {absent_result};
    dialog.SetProjectedQuery({});
    dialog.InitializeQuery(QStringLiteral("absent"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(4U));
    QTRY_COMPARE(model->rowCount(), 1);
    const QModelIndex absent_index = model->index(0, 0);
    results->setCurrentIndex(absent_index);
    dialog.ActivateResult(absent_index);
    QCOMPARE(activations.size(), std::size_t{4});
    CompareIntent(activations.back(), absent_result, false, {});

    const auto copied = activations.back();
    dialog.accepted_activation_scope_.reset();
    model->Reset({});
    CompareIntent(copied, absent_result, false, {});
    dialog.ActivateResult(absent_index);
    QCOMPARE(activations.size(), std::size_t{4});
}

void FullTextSearchDialogTest::
    ProjectsCompleteCurrentResponsesAndReplacesRowsAtomically() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* model = ResponseModel(&dialog);
    auto* results = Results(&dialog);
    auto* articles_found = ArticlesFound(&dialog);
    auto* partial_status = PartialStatus(&dialog);
    auto* empty_status = EmptyStatus(&dialog);
    QVERIFY(model != nullptr);
    QVERIFY(results != nullptr);
    QVERIFY(partial_status != nullptr);
    QVERIFY(empty_status != nullptr);
    QCOMPARE(results->model(), model);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());

    const auto finish_current =
        [&dialog](goldendict::core::FullTextResponse response) {
            dialog.active_generation_ = ++dialog.generation_;
            dialog.FinishSearch(dialog.generation_, std::move(response));
        };

    goldendict::core::FullTextResponse empty;
    empty.errors.push_back(
        {goldendict::core::FullTextErrorCode::kMalformedIndex, "empty-error",
         "contained"});
    finish_current(empty);
    QVERIFY(dialog.response_.has_value());
    QCOMPARE(dialog.response_->errors.size(), std::size_t{1});
    QCOMPARE(dialog.response_->errors.front().code,
             goldendict::core::FullTextErrorCode::kMalformedIndex);
    QCOMPARE(dialog.response_->errors.front().dictionary_id,
             std::string("empty-error"));
    QCOMPARE(dialog.response_->errors.front().message,
             std::string("contained"));
    QVERIFY(!dialog.response_->partial);
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());

    goldendict::core::FullTextResponse conclusive_empty;
    finish_current(conclusive_empty);
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(!empty_status->isHidden());

    goldendict::core::FullTextResponse empty_partial;
    empty_partial.partial = true;
    finish_current(empty_partial);
    QVERIFY(dialog.response_->partial);
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QVERIFY(!partial_status->isHidden());
    QVERIFY(empty_status->isHidden());

    goldendict::core::FullTextResponse partial;
    partial.partial = true;
    partial.results.resize(3);
    partial.results[0].headword = u8"café";
    partial.results[1].headword = "duplicate";
    partial.results[2].headword = "duplicate";
    partial.errors.push_back(
        {goldendict::core::FullTextErrorCode::kDeadlineExceeded, "partial",
         "deadline"});
    finish_current(partial);
    QVERIFY(dialog.response_->partial);
    QCOMPARE(dialog.response_->errors.size(), std::size_t{1});
    QCOMPARE(dialog.response_->errors.front().code,
             goldendict::core::FullTextErrorCode::kDeadlineExceeded);
    QCOMPARE(dialog.response_->errors.front().dictionary_id,
             std::string("partial"));
    QCOMPARE(dialog.response_->errors.front().message, std::string("deadline"));
    QCOMPARE(dialog.response_->results.size(), std::size_t{3});
    QCOMPARE(dialog.response_->results[0].headword, std::string(u8"café"));
    QCOMPARE(dialog.response_->results[1].headword, std::string("duplicate"));
    QCOMPARE(dialog.response_->results[2].headword, std::string("duplicate"));
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 3"));
    QVERIFY(!partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
    const auto labels = dialog.findChildren<QLabel*>();
    for (const auto* label : labels) {
        QVERIFY(!label->text().contains(QStringLiteral("partial")));
        QVERIFY(!label->text().contains(QStringLiteral("deadline")));
    }
    QCOMPARE(model->data(model->index(0, 0)).toString(),
             QString::fromUtf8(u8"café"));
    QCOMPARE(model->data(model->index(1, 0)).toString(),
             QStringLiteral("duplicate"));
    QCOMPARE(model->data(model->index(2, 0)).toString(),
             QStringLiteral("duplicate"));

    goldendict::core::FullTextResponse result_with_error;
    result_with_error.results = {MakeResult("retained", "retained", 4U)};
    result_with_error.errors.push_back(
        {goldendict::core::FullTextErrorCode::kMalformedIndex, "contained",
         "private"});
    finish_current(result_with_error);
    QCOMPARE(model->rowCount(), 1);
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());

    finish_current({});
    QCOMPARE(model->rowCount(), 0);
    QVERIFY(partial_status->isHidden());
    QVERIFY(!empty_status->isHidden());

    QSignalSpy resets(model, &QAbstractItemModel::modelReset);
    QSignalSpy inserted(model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(model, &QAbstractItemModel::rowsRemoved);
    service.response_.results.resize(3);
    service.response_.results[0].headword = u8"über";
    service.response_.results[1].headword = "replacement";
    service.response_.results[2].headword = "replacement";
    dialog.InitializeQuery(QStringLiteral("replacement"));
    dialog.SubmitSearch();
    QVERIFY(!dialog.response_.has_value());
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
    QCOMPARE(resets.size(), 1);
    QVERIFY(service.WaitForQueries(1U));
    QTRY_VERIFY(dialog.response_.has_value());
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 3"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
    QCOMPARE(resets.size(), 2);
    QCOMPARE(inserted.size(), 0);
    QCOMPARE(removed.size(), 0);
    QCOMPARE(model->data(model->index(0, 0)).toString(),
             QString::fromUtf8(u8"über"));
    QCOMPARE(model->data(model->index(1, 0)).toString(),
             QStringLiteral("replacement"));
    QCOMPARE(model->data(model->index(2, 0)).toString(),
             QStringLiteral("replacement"));
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

    goldendict::core::FullTextQuery blocked_scope;
    blocked_scope.dictionary_filter_active = true;
    blocked_scope.dictionary_ids = {"blocked-scope"};
    dialog.SetProjectedQuery(blocked_scope);
    query->setText(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));
    QVERIFY(!search->isEnabled());
    goldendict::core::FullTextQuery pending_scope;
    pending_scope.dictionary_filter_active = true;
    pending_scope.dictionary_ids = {"pending-scope"};
    dialog.SetProjectedQuery(pending_scope);
    query->setText(QStringLiteral("never starts"));
    dialog.SubmitSearch();
    goldendict::core::FullTextQuery replacement_scope;
    replacement_scope.dictionary_ids = {"replacement", "duplicate",
                                        "replacement"};
    replacement_scope.dictionary_filter_active = true;
    dialog.SetProjectedQuery(replacement_scope);
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
    QCOMPARE(ResponseModel(&dialog)->rowCount(), 1);
    QVERIFY(!Results(&dialog)->currentIndex().isValid());
    QVERIFY(!Results(&dialog)->selectionModel()->hasSelection());
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 1"));
    QCOMPARE(ResponseModel(&dialog)
                 ->data(ResponseModel(&dialog)->index(0, 0))
                 .toString(),
             QStringLiteral("current"));
    QVERIFY(!dialog.active_generation_.has_value());
    QVERIFY(dialog.accepted_activation_scope_.has_value());
    QVERIFY(dialog.accepted_activation_scope_->dictionary_filter_active);
    QCOMPARE(dialog.accepted_activation_scope_->dictionary_ids,
             replacement_scope.dictionary_ids);
    QVERIFY(search->isEnabled());
}

void FullTextSearchDialogTest::CancellationIsIdempotentAndRestoresIdleState() {
    ControllableDictionaryService service;
    service.response_.partial = true;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    dialog.InitializeQuery(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));

    CancelButton(&dialog)->click();
    dialog.CancelSearch();
    QVERIFY(service.WaitForCancellation());
    QVERIFY(!dialog.active_generation_.has_value());
    QVERIFY(!dialog.pending_activation_scope_.has_value());
    QVERIFY(!dialog.accepted_activation_scope_.has_value());
    QVERIFY(!dialog.response_.has_value());
    QCOMPARE(ResponseModel(&dialog)->rowCount(), 0);
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 0"));
    QVERIFY(PartialStatus(&dialog)->isHidden());
    QVERIFY(EmptyStatus(&dialog)->isHidden());
    QVERIFY(SearchButton(&dialog)->isEnabled());
    QVERIFY(!CancelButton(&dialog)->isEnabled());
    QVERIFY(Progress(&dialog)->isHidden());

    service.ReleaseCancelledRequest();
    QTest::qWait(20);
    QCoreApplication::processEvents();
    QVERIFY(!dialog.response_.has_value());
    QCOMPARE(ResponseModel(&dialog)->rowCount(), 0);
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 0"));
    QVERIFY(PartialStatus(&dialog)->isHidden());
    QVERIFY(EmptyStatus(&dialog)->isHidden());
    QCOMPARE(dialog.generation_, 1U);
}

void FullTextSearchDialogTest::
    ServiceReplacementDetachAndDestructionSuppressLateDelivery() {
    ControllableDictionaryService first;
    ControllableDictionaryService second;
    goldendict::core::ApplicationPreferences preferences;
    {
        FullTextSearchDialog dialog(preferences, &first);
        first.response_.partial = true;
        first.response_.results.resize(1);
        first.response_.results.front().headword = "accepted";
        dialog.InitializeQuery(QStringLiteral("accepted"));
        dialog.SubmitSearch();
        QVERIFY(first.WaitForQueries(1U));
        QTRY_VERIFY(dialog.response_.has_value());
        QCOMPARE(ResponseModel(&dialog)->rowCount(), 1);
        QCOMPARE(ArticlesFound(&dialog)->text(),
                 QStringLiteral("Articles found: 1"));
        QVERIFY(!PartialStatus(&dialog)->isHidden());
        QVERIFY(EmptyStatus(&dialog)->isHidden());
        QVERIFY(dialog.accepted_activation_scope_.has_value());
        dialog.SetService(&second);
        QVERIFY(dialog.response_.has_value());
        QCOMPARE(dialog.response_->results.front().headword,
                 std::string("accepted"));
        QCOMPARE(ResponseModel(&dialog)->rowCount(), 1);
        QCOMPARE(ArticlesFound(&dialog)->text(),
                 QStringLiteral("Articles found: 1"));
        QVERIFY(!PartialStatus(&dialog)->isHidden());
        QVERIFY(EmptyStatus(&dialog)->isHidden());
        QVERIFY(dialog.accepted_activation_scope_.has_value());
        QVERIFY(!dialog.pending_activation_scope_.has_value());
        dialog.DetachController();
        dialog.DetachController();
        QVERIFY(dialog.response_.has_value());
        QCOMPARE(ResponseModel(&dialog)->rowCount(), 1);
        QVERIFY(!PartialStatus(&dialog)->isHidden());
        QVERIFY(EmptyStatus(&dialog)->isHidden());
        QVERIFY(dialog.accepted_activation_scope_.has_value());
    }
    QCOMPARE(second.Queries().size(), std::size_t{0});

    ControllableDictionaryService blocked;
    blocked.response_.partial = true;
    ControllableDictionaryService replacement;
    {
        FullTextSearchDialog dialog(preferences, &blocked);
        dialog.InitializeQuery(QStringLiteral("blocked"));
        dialog.SubmitSearch();
        QVERIFY(blocked.WaitForQueries(1U));
        blocked.ReleaseCancelledRequest();
        dialog.SetService(&replacement);
        QVERIFY(blocked.WaitForCancellation());
        QVERIFY(!dialog.active_generation_.has_value());
        QVERIFY(!dialog.pending_activation_scope_.has_value());
        QVERIFY(!dialog.accepted_activation_scope_.has_value());
        QVERIFY(!dialog.response_.has_value());
        QCOMPARE(ResponseModel(&dialog)->rowCount(), 0);
        QCOMPARE(ArticlesFound(&dialog)->text(),
                 QStringLiteral("Articles found: 0"));
        QVERIFY(PartialStatus(&dialog)->isHidden());
        QVERIFY(EmptyStatus(&dialog)->isHidden());
        QVERIFY(SearchButton(&dialog)->isEnabled());
        dialog.DetachController();
        dialog.DetachController();
    }
    QCOMPARE(replacement.Queries().size(), std::size_t{0});
}

}  // namespace goldendict::app

QTEST_MAIN(goldendict::app::FullTextSearchDialogTest)

#include "full_text_search_dialog_test.moc"
