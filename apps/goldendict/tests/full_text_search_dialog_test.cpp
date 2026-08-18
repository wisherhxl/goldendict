// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAction>
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPointer>
#include <QProgressBar>
#include <QProxyStyle>
#include <QPushButton>
#include <QStyleOptionViewItem>
#include <QTranslator>
#include <QVBoxLayout>
#include <QtTest>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "full_text_query_composer.h"
#include "full_text_response_model.h"
#include "full_text_search_dialog.h"

namespace goldendict::app {
namespace {

class TitleTranslator final : public QTranslator {
   public:
    QString translate(const char* context, const char* source_text, const char*,
                      int) const override {
        if (qstrcmp(context, "goldendict::app::FullTextSearchDialog") == 0 &&
            qstrcmp(source_text, "Full-text search") == 0) {
            ++matching_requests;
            return QStringLiteral("Translated full-text title");
        }
        return {};
    }

    mutable int matching_requests = 0;
};

class SearchGroupTranslator final : public QTranslator {
   public:
    QString translate(const char* context, const char* source_text, const char*,
                      int) const override {
        if (qstrcmp(context, "goldendict::app::FullTextSearchDialog") == 0 &&
            qstrcmp(source_text, "Search") == 0) {
            ++matching_requests;
            return QStringLiteral("Translated Search group");
        }
        return {};
    }

    mutable int matching_requests = 0;
};

class ScopedTranslatorInstallation final {
   public:
    explicit ScopedTranslatorInstallation(QTranslator* translator)
        : translator_(translator),
          installed_(QCoreApplication::installTranslator(translator_)) {}

    ~ScopedTranslatorInstallation() {
        if (installed_)
            QCoreApplication::removeTranslator(translator_);
    }

    bool IsInstalled() const { return installed_; }

   private:
    QTranslator* translator_;
    bool installed_;
};

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

void VerifySearchButtonDefaultPolicy(FullTextSearchDialog* dialog) {
    const auto matches = dialog->findChildren<QPushButton*>(
        QStringLiteral("fullTextSearchButton"), Qt::FindDirectChildrenOnly);
    QCOMPARE(matches.size(), 1);
    auto* search = SearchButton(dialog);
    QVERIFY(search != nullptr);
    QCOMPARE(search, matches.front());
    QCOMPARE(search->parent(), dialog);
    QCOMPARE(search->objectName(), QStringLiteral("fullTextSearchButton"));
    QCOMPARE(search->text(), QStringLiteral("Search"));
    QVERIFY(search->isDefault());
    QVERIFY(!search->autoDefault());
}

void VerifyExactForwardTabChain(FullTextSearchDialog* dialog) {
    const std::vector<QString> object_names = {
        QStringLiteral("fullTextQueryText"),
        QStringLiteral("fullTextSearchResults"),
        QStringLiteral("fullTextUseMaximumWordDistance"),
        QStringLiteral("fullTextMaximumWordDistance"),
        QStringLiteral("fullTextQueryMode"),
        QStringLiteral("fullTextUseMaximumArticles"),
        QStringLiteral("fullTextMaximumArticlesPerDictionary"),
        QStringLiteral("fullTextMatchCase"),
        QStringLiteral("fullTextSearchButton"),
        QStringLiteral("fullTextCancelButton")};
    std::vector<QWidget*> controls;
    controls.reserve(object_names.size());
    for (const auto& object_name : object_names) {
        const auto matches = dialog->findChildren<QWidget*>(
            object_name, Qt::FindChildrenRecursively);
        QCOMPARE(matches.size(), 1);
        controls.push_back(matches.front());
    }

    const qsizetype focus_chain_bound =
        dialog->window()->findChildren<QWidget*>().size() + 1;
    for (std::size_t index = 0; index + 1U < controls.size(); ++index) {
        QWidget* current = controls[index];
        std::vector<QWidget*> visited = {current};
        for (qsizetype step = 0; step < focus_chain_bound; ++step) {
            current = current->nextInFocusChain();
            QVERIFY(current != nullptr);
            QVERIFY2(
                std::find(visited.cbegin(), visited.cend(), current) ==
                    visited.cend(),
                "focus chain contains a cycle before the next named control");
            visited.push_back(current);

            const auto named =
                std::find(controls.cbegin(), controls.cend(), current);
            if (named == controls.cend())
                continue;

            QCOMPARE(current, controls[index + 1U]);
            break;
        }
        QVERIFY2(current == controls[index + 1U],
                 "next named control is missing from the bounded focus chain");
    }
}

QGroupBox* SearchGroupBox(FullTextSearchDialog* dialog) {
    const auto groups =
        dialog->findChildren<QGroupBox*>(QString(), Qt::FindDirectChildrenOnly);
    return groups.size() == 1 ? groups.front() : nullptr;
}

void VerifySearchGroupBox(
    FullTextSearchDialog* dialog, QGroupBox* expected_group = nullptr,
    FullTextQueryComposer* expected_composer = nullptr,
    const QString& expected_title = QStringLiteral("Search")) {
    const auto groups =
        dialog->findChildren<QGroupBox*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(groups.size(), 1);
    auto* group = groups.front();
    if (expected_group != nullptr)
        QCOMPARE(group, expected_group);
    QCOMPARE(group->title(), expected_title);

    const auto composers = dialog->findChildren<FullTextQueryComposer*>();
    QCOMPARE(composers.size(), 1);
    auto* composer = composers.front();
    if (expected_composer != nullptr)
        QCOMPARE(composer, expected_composer);
    QCOMPARE(composer->parent(), group);

    const auto direct_composers = group->findChildren<FullTextQueryComposer*>(
        QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(direct_composers.size(), 1);
    QCOMPARE(direct_composers.front(), composer);

    auto* group_layout = qobject_cast<QVBoxLayout*>(group->layout());
    QVERIFY(group_layout != nullptr);
    QCOMPARE(group_layout->count(), 1);
    QCOMPARE(group_layout->itemAt(0)->widget(), composer);

    auto* enclosing_layout = qobject_cast<QVBoxLayout*>(dialog->layout());
    QVERIFY(enclosing_layout != nullptr);
    QCOMPARE(enclosing_layout->indexOf(group), 0);
    QCOMPARE(
        enclosing_layout->itemAt(1)->widget(),
        dialog->findChild<QListView*>(QStringLiteral("fullTextSearchResults")));
}

QPushButton* CancelButton(FullTextSearchDialog* dialog) {
    return dialog->findChild<QPushButton*>(
        QStringLiteral("fullTextCancelButton"));
}

QPushButton* HelpButton(FullTextSearchDialog* dialog) {
    return dialog->findChild<QPushButton*>(
        QStringLiteral("fullTextHelpButton"));
}

QAction* HelpAction(FullTextSearchDialog* dialog) {
    return dialog->findChild<QAction*>(QStringLiteral("fullTextHelpAction"));
}

QHBoxLayout* ButtonRow(FullTextSearchDialog* dialog) {
    auto* search = SearchButton(dialog);
    auto* cancel = CancelButton(dialog);
    auto* help = HelpButton(dialog);
    for (auto* row : dialog->findChildren<QHBoxLayout*>()) {
        if (row->indexOf(search) >= 0 && row->indexOf(cancel) >= 0 &&
            row->indexOf(help) >= 0) {
            return row;
        }
    }
    return nullptr;
}

void VerifyButtonRow(FullTextSearchDialog* dialog,
                     QHBoxLayout* expected_row = nullptr) {
    const auto searches = dialog->findChildren<QPushButton*>(
        QStringLiteral("fullTextSearchButton"), Qt::FindDirectChildrenOnly);
    const auto cancels = dialog->findChildren<QPushButton*>(
        QStringLiteral("fullTextCancelButton"), Qt::FindDirectChildrenOnly);
    const auto helps = dialog->findChildren<QPushButton*>(
        QStringLiteral("fullTextHelpButton"), Qt::FindDirectChildrenOnly);
    QCOMPARE(searches.size(), 1);
    QCOMPARE(cancels.size(), 1);
    QCOMPARE(helps.size(), 1);

    auto* search = searches.front();
    auto* cancel = cancels.front();
    auto* help = helps.front();
    QList<QHBoxLayout*> matching_rows;
    for (auto* row : dialog->findChildren<QHBoxLayout*>()) {
        if (row->indexOf(search) >= 0 && row->indexOf(cancel) >= 0 &&
            row->indexOf(help) >= 0) {
            matching_rows.push_back(row);
        }
    }
    QCOMPARE(matching_rows.size(), 1);

    auto* row = matching_rows.front();
    if (expected_row != nullptr)
        QCOMPARE(row, expected_row);
    QCOMPARE(row->count(), 7);
    QCOMPARE(row->itemAt(1)->widget(), search);
    QCOMPARE(row->itemAt(3)->widget(), cancel);
    QCOMPARE(row->itemAt(5)->widget(), help);

    for (const int index : {0, 2, 4, 6}) {
        auto* spacer = row->itemAt(index)->spacerItem();
        QVERIFY(spacer != nullptr);
        QCOMPARE(spacer->sizePolicy().horizontalPolicy(),
                 QSizePolicy::Expanding);
    }

    QCOMPARE(search->parent(), dialog);
    QCOMPARE(cancel->parent(), dialog);
    QCOMPARE(help->parent(), dialog);
    QCOMPARE(search->text(), QStringLiteral("Search"));
    QCOMPARE(cancel->text(), QStringLiteral("Cancel"));
    QCOMPARE(help->text(), QStringLiteral("Help"));

    auto* enclosing_layout = qobject_cast<QVBoxLayout*>(dialog->layout());
    QVERIFY(enclosing_layout != nullptr);
    int matching_layouts = 0;
    for (int index = 0; index < enclosing_layout->count(); ++index) {
        if (enclosing_layout->itemAt(index)->layout() == row)
            ++matching_layouts;
    }
    QCOMPARE(matching_layouts, 1);
}

QProgressBar* Progress(FullTextSearchDialog* dialog) {
    return dialog->findChild<QProgressBar*>(
        QStringLiteral("fullTextSearchProgress"));
}

QListView* Results(FullTextSearchDialog* dialog);
QLabel* PartialStatus(FullTextSearchDialog* dialog);
QLabel* EmptyStatus(FullTextSearchDialog* dialog);
QLabel* FailureStatus(FullTextSearchDialog* dialog);
QLabel* MixedResultStatus(FullTextSearchDialog* dialog);
QLabel* PartialEmptyStatus(FullTextSearchDialog* dialog);
QLabel* ErrorCountStatus(FullTextSearchDialog* dialog);

QHBoxLayout* ResultCountProgressRow(FullTextSearchDialog* dialog) {
    auto* label = dialog->findChild<QLabel*>(
        QStringLiteral("fullTextArticlesFoundLabel"));
    auto* progress = Progress(dialog);
    for (auto* row : dialog->findChildren<QHBoxLayout*>()) {
        if (row->indexOf(label) >= 0 && row->indexOf(progress) >= 0)
            return row;
    }
    return nullptr;
}

void VerifyResultCountProgressRow(FullTextSearchDialog* dialog,
                                  QHBoxLayout* expected_row = nullptr) {
    const auto labels = dialog->findChildren<QLabel*>(
        QStringLiteral("fullTextArticlesFoundLabel"),
        Qt::FindDirectChildrenOnly);
    const auto progress_bars = dialog->findChildren<QProgressBar*>(
        QStringLiteral("fullTextSearchProgress"), Qt::FindDirectChildrenOnly);
    QCOMPARE(labels.size(), 1);
    QCOMPARE(progress_bars.size(), 1);

    auto* label = labels.front();
    auto* progress = progress_bars.front();
    QList<QHBoxLayout*> matching_rows;
    for (auto* row : dialog->findChildren<QHBoxLayout*>()) {
        if (row->indexOf(label) >= 0 && row->indexOf(progress) >= 0)
            matching_rows.push_back(row);
    }
    QCOMPARE(matching_rows.size(), 1);

    auto* row = matching_rows.front();
    if (expected_row != nullptr)
        QCOMPARE(row, expected_row);
    QCOMPARE(row->count(), 2);
    QCOMPARE(row->itemAt(0)->widget(), label);
    QCOMPARE(row->itemAt(1)->widget(), progress);
    QCOMPARE(label->parent(), dialog);
    QCOMPARE(progress->parent(), dialog);

    auto* enclosing_layout = qobject_cast<QVBoxLayout*>(dialog->layout());
    QVERIFY(enclosing_layout != nullptr);
    int row_index = -1;
    for (int index = 0; index < enclosing_layout->count(); ++index) {
        if (enclosing_layout->itemAt(index)->layout() == row) {
            row_index = index;
            break;
        }
    }
    QVERIFY(row_index >= 0);
    QCOMPARE(enclosing_layout->itemAt(row_index - 1)->widget(),
             Results(dialog));

    const std::vector<QLabel*> statuses = {
        PartialStatus(dialog),      EmptyStatus(dialog),
        FailureStatus(dialog),      MixedResultStatus(dialog),
        PartialEmptyStatus(dialog), ErrorCountStatus(dialog)};
    for (std::size_t index = 0; index < statuses.size(); ++index) {
        QCOMPARE(
            enclosing_layout->itemAt(row_index + 1 + static_cast<int>(index))
                ->widget(),
            statuses[index]);
    }
}

void VerifyProgressAlignmentContract(FullTextSearchDialog* dialog) {
    const auto matches = dialog->findChildren<QProgressBar*>(
        QStringLiteral("fullTextSearchProgress"), Qt::FindDirectChildrenOnly);
    QCOMPARE(matches.size(), 1);
    auto* progress = Progress(dialog);
    QVERIFY(progress != nullptr);
    QCOMPARE(progress, matches.front());
    QCOMPARE(progress->parent(), dialog);
    QCOMPARE(progress->objectName(), QStringLiteral("fullTextSearchProgress"));
    QCOMPARE(progress->alignment(), Qt::AlignCenter);
    QCOMPARE(progress->minimum(), 0);
    QCOMPARE(progress->maximum(), 0);
    VerifyResultCountProgressRow(dialog);
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

void VerifyResultCountMinimumHeight(FullTextSearchDialog* dialog) {
    const auto matches = dialog->findChildren<QLabel*>(
        QStringLiteral("fullTextArticlesFoundLabel"),
        Qt::FindDirectChildrenOnly);
    QCOMPARE(matches.size(), 1);
    auto* label = ArticlesFound(dialog);
    QVERIFY(label != nullptr);
    QCOMPARE(label, matches.front());
    QCOMPARE(label->parent(), dialog);
    QCOMPARE(label->objectName(), QStringLiteral("fullTextArticlesFoundLabel"));
    QCOMPARE(label->minimumHeight(), 21);
    QCOMPARE(label->maximumHeight(), QWIDGETSIZE_MAX);
    VerifyResultCountProgressRow(dialog);
}

QLabel* PartialStatus(FullTextSearchDialog* dialog) {
    return dialog->findChild<QLabel*>(
        QStringLiteral("fullTextPartialResponseStatus"));
}

QLabel* EmptyStatus(FullTextSearchDialog* dialog) {
    return dialog->findChild<QLabel*>(
        QStringLiteral("fullTextEmptyResponseStatus"));
}

QLabel* FailureStatus(FullTextSearchDialog* dialog) {
    return dialog->findChild<QLabel*>(
        QStringLiteral("fullTextFailureResponseStatus"));
}

QLabel* MixedResultStatus(FullTextSearchDialog* dialog) {
    return dialog->findChild<QLabel*>(
        QStringLiteral("fullTextMixedResultResponseStatus"));
}

QLabel* PartialEmptyStatus(FullTextSearchDialog* dialog) {
    return dialog->findChild<QLabel*>(
        QStringLiteral("fullTextPartialEmptyResponseStatus"));
}

QLabel* ErrorCountStatus(FullTextSearchDialog* dialog) {
    return dialog->findChild<QLabel*>(
        QStringLiteral("fullTextErrorCountResponseStatus"));
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
                   const std::vector<std::string>& expected_ids,
                   const std::string& expected_query_text,
                   bool expected_ignore_diacritics) {
    CompareResult(actual.result, expected_result);
    QCOMPARE(actual.dictionary_filter_active, expected_filter_active);
    QCOMPARE(actual.dictionary_ids, expected_ids);
    QCOMPARE(actual.query_text, expected_query_text);
    QCOMPARE(actual.ignore_diacritics, expected_ignore_diacritics);
}

}  // namespace

class FullTextSearchDialogTest final : public QObject {
    Q_OBJECT

   private slots:
    void ResolvesWindowTitleThroughPrivateTranslationContext();
    void ResolvesSearchGroupTitleThroughPrivateTranslationContext();
    void PreservesExactForwardTabChainAcrossRequestTransitions();
    void SubmitsExactComposedAndProjectedQueryAndRetainsResponse();
    void PaintsEachResultWithIndependentDirectionAndElision();
    void KeepsSelectionAndFocusDeterministicAcrossAcceptedResponses();
    void RetainsExactAcceptedScopeAndQueryContextForByValueActivation();
    void ProjectsCompleteCurrentResponsesAndReplacesRowsAtomically();
    void ShowsOnlyGenericTerminalFailureForErrorOnlyResponses();
    void ShowsOnlyGenericMixedResultStatusForResponsesWithResultsAndErrors();
    void ShowsOnlyGenericPartialEmptyStatusForPartialResponsesWithoutResults();
    void ShowsAuthoritativeAcceptedErrorCountWithoutDetails();
    void NotifiesExactlyOnceForEachAcceptedResponseShape();
    void ActivatesExactCurrentResultOnceFromMouseAndKeyboard();
    void SuppressesDuplicateInvalidStaleAndCancelledActivation();
    void ReplacesRunningAndPendingGenerationsAndSuppressesStaleCompletion();
    void ActiveCancellationRestoresIdleStateWithoutDismissal();
    void EmitsHelpIntentWithoutMutatingIdleOrActiveState();
    void EnforcesPinnedMinimumAcrossResizeAndRestore();
    void RestoresAndCapturesGeometryOnlyForIdleDismissal();
    void IdleCancelDismissesThroughDialogLifecycle();
    void ServiceReplacementDetachAndDestructionSuppressLateDelivery();
};

void FullTextSearchDialogTest::
    ResolvesWindowTitleThroughPrivateTranslationContext() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog default_dialog(preferences, &service);
    QCOMPARE(default_dialog.windowTitle(), QStringLiteral("Full-text search"));

    TitleTranslator translator;
    const ScopedTranslatorInstallation installation(&translator);
    QVERIFY(installation.IsInstalled());
    FullTextSearchDialog translated_dialog(preferences, &service);
    QCOMPARE(translated_dialog.windowTitle(),
             QStringLiteral("Translated full-text title"));
    QCOMPARE(translator.matching_requests, 1);
}

void FullTextSearchDialogTest::
    ResolvesSearchGroupTitleThroughPrivateTranslationContext() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog default_dialog(preferences, &service);
    VerifySearchGroupBox(&default_dialog);

    {
        SearchGroupTranslator translator;
        const ScopedTranslatorInstallation installation(&translator);
        QVERIFY(installation.IsInstalled());
        FullTextSearchDialog translated_dialog(preferences, &service);
        auto* translated_group = SearchGroupBox(&translated_dialog);
        QVERIFY(translated_group != nullptr);
        VerifySearchGroupBox(&translated_dialog, translated_group, nullptr,
                             QStringLiteral("Translated Search group"));
        auto* translated_search_button = SearchButton(&translated_dialog);
        QVERIFY(translated_search_button != nullptr);
        QCOMPARE(translated_search_button->text(),
                 QStringLiteral("Translated Search group"));
        QCOMPARE(translator.matching_requests, 2);
    }

    FullTextSearchDialog restored_dialog(preferences, &service);
    VerifySearchGroupBox(&restored_dialog);
}

void FullTextSearchDialogTest::
    PreservesExactForwardTabChainAcrossRequestTransitions() {
    ControllableDictionaryService service;
    service.response_.results = {MakeResult("dictionary", "result", 1U)};
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* search_group = SearchGroupBox(&dialog);
    QVERIFY(search_group != nullptr);
    auto* composer = dialog.composer_;
    VerifySearchGroupBox(&dialog, search_group, composer);
    auto* button_row = ButtonRow(&dialog);
    QVERIFY(button_row != nullptr);
    VerifyButtonRow(&dialog, button_row);
    auto* result_count_progress_row = ResultCountProgressRow(&dialog);
    QVERIFY(result_count_progress_row != nullptr);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    VerifyResultCountMinimumHeight(&dialog);
    VerifyExactForwardTabChain(&dialog);
    VerifySearchButtonDefaultPolicy(&dialog);
    dialog.InitializeQuery(QStringLiteral("complete"));
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyButtonRow(&dialog, button_row);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyResultCountMinimumHeight(&dialog);
    dialog.show();
    QTRY_VERIFY(dialog.query_text_->hasFocus());
    VerifyExactForwardTabChain(&dialog);

    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyButtonRow(&dialog, button_row);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    VerifyResultCountMinimumHeight(&dialog);
    QVERIFY(!SearchButton(&dialog)->isEnabled());
    VerifySearchButtonDefaultPolicy(&dialog);
    VerifyExactForwardTabChain(&dialog);
    QTRY_VERIFY(dialog.response_.has_value());
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyButtonRow(&dialog, button_row);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    VerifyResultCountMinimumHeight(&dialog);
    QVERIFY(SearchButton(&dialog)->isEnabled());
    VerifySearchButtonDefaultPolicy(&dialog);
    VerifyExactForwardTabChain(&dialog);

    dialog.InitializeQuery(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(2U));
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyButtonRow(&dialog, button_row);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    VerifyResultCountMinimumHeight(&dialog);
    QVERIFY(!SearchButton(&dialog)->isEnabled());
    VerifyExactForwardTabChain(&dialog);
    dialog.CancelSearch();
    QVERIFY(service.WaitForCancellation());
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyButtonRow(&dialog, button_row);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    VerifyResultCountMinimumHeight(&dialog);
    QVERIFY(SearchButton(&dialog)->isEnabled());
    VerifySearchButtonDefaultPolicy(&dialog);
    VerifyExactForwardTabChain(&dialog);
    service.ReleaseCancelledRequest();
}

void FullTextSearchDialogTest::
    EmitsHelpIntentWithoutMutatingIdleOrActiveState() {
    ControllableDictionaryService idle_service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog idle_dialog(preferences, &idle_service);
    auto* idle_help_button = HelpButton(&idle_dialog);
    auto* idle_help_action = HelpAction(&idle_dialog);
    QVERIFY(idle_help_button != nullptr);
    QCOMPARE(idle_help_button->parent(), &idle_dialog);
    QCOMPARE(idle_help_button->objectName(),
             QStringLiteral("fullTextHelpButton"));
    QCOMPARE(idle_help_button->text(), QStringLiteral("Help"));
    QVERIFY(idle_help_action != nullptr);
    QCOMPARE(idle_help_action->parent(), &idle_dialog);
    QCOMPARE(idle_help_action->objectName(),
             QStringLiteral("fullTextHelpAction"));
    QCOMPARE(idle_help_action->shortcut(), QKeySequence(Qt::Key_F1));
    QCOMPARE(idle_help_action->shortcutContext(),
             Qt::WidgetWithChildrenShortcut);
    QVERIFY(idle_dialog.actions().contains(idle_help_action));

    idle_dialog.InitializeQuery(QStringLiteral("idle-query"));
    goldendict::core::FullTextQuery projected_query;
    projected_query.dictionary_filter_active = true;
    projected_query.dictionary_ids = {"idle-dictionary"};
    idle_dialog.SetProjectedQuery(projected_query);
    goldendict::core::FullTextResponse accepted_response;
    accepted_response.results = {
        MakeResult("idle-dictionary", "idle-result", 17U)};
    idle_dialog.pending_activation_scope_ =
        FullTextSearchDialog::ActivationScope{true, {"idle-dictionary"}};
    idle_dialog.pending_activation_context_ =
        FullTextSearchDialog::ActivationContext{"accepted-query", true};
    idle_dialog.active_generation_ = ++idle_dialog.generation_;
    std::size_t completion_count = 0U;
    idle_dialog.completion_notifier_ = [&completion_count]() {
        ++completion_count;
    };
    idle_dialog.FinishSearch(idle_dialog.generation_, accepted_response);
    const QModelIndex selected_index = ResponseModel(&idle_dialog)->index(0, 0);
    Results(&idle_dialog)->setCurrentIndex(selected_index);
    Results(&idle_dialog)
        ->selectionModel()
        ->select(selected_index, QItemSelectionModel::ClearAndSelect |
                                     QItemSelectionModel::Rows);
    idle_dialog.show();
    QCoreApplication::processEvents();

    QSignalSpy idle_help_requests(&idle_dialog,
                                  &FullTextSearchDialog::HelpRequested);
    QSignalSpy idle_geometry_captures(&idle_dialog,
                                      &FullTextSearchDialog::GeometryCaptured);
    const QByteArray idle_geometry = idle_dialog.saveGeometry();
    const auto idle_projected_query = idle_dialog.ProjectedQuery();
    const std::uint64_t idle_generation = idle_dialog.generation_;

    idle_help_button->click();
    QCOMPARE(idle_help_requests.count(), 1);
    Results(&idle_dialog)->setFocus();
    QTest::keyClick(Results(&idle_dialog), Qt::Key_F1);
    QCOMPARE(idle_help_requests.count(), 2);
    idle_help_button->click();
    QCOMPARE(idle_help_requests.count(), 3);
    Results(&idle_dialog)->setFocus();
    QTest::keyClick(Results(&idle_dialog), Qt::Key_F1);
    QCOMPARE(idle_help_requests.count(), 4);

    QVERIFY(idle_dialog.isVisible());
    QVERIFY(!idle_dialog.active_generation_.has_value());
    QCOMPARE(idle_dialog.generation_, idle_generation);
    QCOMPARE(idle_service.Queries().size(), std::size_t{0});
    QCOMPARE(idle_dialog.ProjectedQuery().dictionary_filter_active,
             idle_projected_query.dictionary_filter_active);
    QCOMPARE(idle_dialog.ProjectedQuery().dictionary_ids,
             idle_projected_query.dictionary_ids);
    QCOMPARE(idle_dialog.query_text_->text(), QStringLiteral("idle-query"));
    QVERIFY(idle_dialog.response_.has_value());
    QCOMPARE(idle_dialog.response_->results.size(), std::size_t{1});
    QCOMPARE(idle_dialog.response_->results.front().headword,
             std::string("idle-result"));
    QCOMPARE(ResponseModel(&idle_dialog)->rowCount(), 1);
    QCOMPARE(Results(&idle_dialog)->currentIndex(), selected_index);
    QVERIFY(
        Results(&idle_dialog)->selectionModel()->isSelected(selected_index));
    QVERIFY(idle_dialog.accepted_activation_scope_.has_value());
    QVERIFY(idle_dialog.accepted_activation_scope_->dictionary_filter_active);
    QCOMPARE(idle_dialog.accepted_activation_scope_->dictionary_ids,
             std::vector<std::string>{"idle-dictionary"});
    QVERIFY(idle_dialog.accepted_activation_context_.has_value());
    QCOMPARE(idle_dialog.accepted_activation_context_->query_text,
             std::string("accepted-query"));
    QVERIFY(idle_dialog.accepted_activation_context_->ignore_diacritics);
    QCOMPARE(completion_count, std::size_t{1});
    QCOMPARE(idle_geometry_captures.count(), 0);
    QCOMPARE(idle_dialog.saveGeometry(), idle_geometry);
    idle_dialog.hide();

    ControllableDictionaryService active_service;
    FullTextSearchDialog active_dialog(preferences, &active_service);
    active_dialog.InitializeQuery(QStringLiteral("blocked"));
    active_dialog.show();
    active_dialog.activateWindow();
    QCoreApplication::processEvents();
    active_dialog.SubmitSearch();
    QVERIFY(active_service.WaitForQueries(1U));
    QSignalSpy active_help_requests(&active_dialog,
                                    &FullTextSearchDialog::HelpRequested);
    QSignalSpy active_geometry_captures(
        &active_dialog, &FullTextSearchDialog::GeometryCaptured);
    const QByteArray active_geometry = active_dialog.saveGeometry();
    const auto active_generation = active_dialog.active_generation_;

    HelpButton(&active_dialog)->click();
    QCOMPARE(active_help_requests.count(), 1);
    Results(&active_dialog)->setFocus();
    QTest::keyClick(Results(&active_dialog), Qt::Key_F1);
    QCOMPARE(active_help_requests.count(), 2);
    HelpButton(&active_dialog)->click();
    QCOMPARE(active_help_requests.count(), 3);
    Results(&active_dialog)->setFocus();
    QTest::keyClick(Results(&active_dialog), Qt::Key_F1);
    QCOMPARE(active_help_requests.count(), 4);

    QVERIFY(active_dialog.isVisible());
    QCOMPARE(active_dialog.active_generation_, active_generation);
    QVERIFY(active_dialog.pending_activation_scope_.has_value());
    QVERIFY(!active_dialog.pending_activation_scope_->dictionary_filter_active);
    QVERIFY(active_dialog.pending_activation_scope_->dictionary_ids.empty());
    QVERIFY(active_dialog.pending_activation_context_.has_value());
    QCOMPARE(active_dialog.pending_activation_context_->query_text,
             std::string("blocked"));
    QVERIFY(!active_dialog.accepted_activation_scope_.has_value());
    QVERIFY(!active_dialog.accepted_activation_context_.has_value());
    QVERIFY(!active_dialog.response_.has_value());
    QCOMPARE(ResponseModel(&active_dialog)->rowCount(), 0);
    QVERIFY(!Results(&active_dialog)->currentIndex().isValid());
    QVERIFY(!Results(&active_dialog)->selectionModel()->hasSelection());
    const auto active_queries = active_service.Queries();
    QCOMPARE(active_queries.size(), std::size_t{1});
    QCOMPARE(active_queries.front().text, std::string("blocked"));
    QCOMPARE(active_dialog.query_text_->text(), QStringLiteral("blocked"));
    QVERIFY(!SearchButton(&active_dialog)->isEnabled());
    QVERIFY(CancelButton(&active_dialog)->isEnabled());
    QVERIFY(!Progress(&active_dialog)->isHidden());
    QCOMPARE(active_geometry_captures.count(), 0);
    QCOMPARE(active_dialog.saveGeometry(), active_geometry);

    active_dialog.CancelSearch();
    QVERIFY(active_service.WaitForCancellation());
    active_service.ReleaseCancelledRequest();
}

void FullTextSearchDialogTest::
    NotifiesExactlyOnceForEachAcceptedResponseShape() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    std::size_t notifications = 0U;
    dialog.completion_notifier_ = [&notifications]() {
        ++notifications;
    };
    QCOMPARE(notifications, std::size_t{0});

    goldendict::core::FullTextResponse nonempty;
    nonempty.results = {MakeResult("success", "success", 1U)};
    goldendict::core::FullTextResponse partial_empty;
    partial_empty.partial = true;
    goldendict::core::FullTextResponse partial_nonempty = partial_empty;
    partial_nonempty.results = {MakeResult("partial", "partial", 2U)};
    goldendict::core::FullTextResponse error_only;
    error_only.errors = {{goldendict::core::FullTextErrorCode::kInternal,
                          "error-only", "private detail"}};
    goldendict::core::FullTextResponse mixed = error_only;
    mixed.results = {MakeResult("mixed", "mixed", 3U)};

    std::vector<goldendict::core::FullTextResponse> responses = {
        nonempty, {}, partial_empty, partial_nonempty, error_only, mixed};
    for (std::size_t i = 0U; i < responses.size(); ++i) {
        dialog.active_generation_ = ++dialog.generation_;
        const std::uint64_t accepted_generation = dialog.generation_;
        dialog.FinishSearch(accepted_generation, responses[i]);
        QCOMPARE(notifications, i + 1U);

        dialog.FinishSearch(accepted_generation, responses[i]);
        dialog.FinishSearch(accepted_generation - 1U, responses[i]);
        QCOMPARE(notifications, i + 1U);
    }
}

void FullTextSearchDialogTest::
    ShowsAuthoritativeAcceptedErrorCountWithoutDetails() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* error_count_status = ErrorCountStatus(&dialog);
    QVERIFY(error_count_status != nullptr);
    QCOMPARE(error_count_status->parent(), &dialog);
    QVERIFY(error_count_status->isHidden());
    QVERIFY(error_count_status->text().isEmpty());

    const auto finish_current =
        [&dialog](goldendict::core::FullTextResponse response) {
            dialog.active_generation_ = ++dialog.generation_;
            dialog.FinishSearch(dialog.generation_, std::move(response));
        };
    const std::vector<goldendict::core::FullTextErrorCode> error_codes = {
        goldendict::core::FullTextErrorCode::kInvalidQuery,
        goldendict::core::FullTextErrorCode::kDictionaryUnavailable,
        goldendict::core::FullTextErrorCode::kUnsupported,
        goldendict::core::FullTextErrorCode::kMalformedIndex,
        goldendict::core::FullTextErrorCode::kCancelled,
        goldendict::core::FullTextErrorCode::kDeadlineExceeded,
        goldendict::core::FullTextErrorCode::kResourceLimit,
        goldendict::core::FullTextErrorCode::kInternal,
    };

    finish_current({});
    QVERIFY(error_count_status->isHidden());
    QVERIFY(error_count_status->text().isEmpty());

    for (const auto code : error_codes) {
        goldendict::core::FullTextResponse response;
        response.errors.push_back(
            {code, "private-dictionary", "private backend detail"});
        finish_current(std::move(response));
        QVERIFY(!error_count_status->isHidden());
        QCOMPARE(error_count_status->text(), QStringLiteral("Errors: 1"));
        QVERIFY(!FailureStatus(&dialog)->isHidden());
    }

    goldendict::core::FullTextResponse multiple;
    multiple.errors = {
        {goldendict::core::FullTextErrorCode::kInternal, "duplicate-private-id",
         "first private detail"},
        {goldendict::core::FullTextErrorCode::kInternal, "duplicate-private-id",
         "second private detail"},
        {goldendict::core::FullTextErrorCode::kMalformedIndex,
         "third-private-id", "third private detail"},
    };
    finish_current(multiple);
    QVERIFY(!error_count_status->isHidden());
    QCOMPARE(error_count_status->text(), QStringLiteral("Errors: 3"));
    QVERIFY(!FailureStatus(&dialog)->isHidden());

    multiple.results = {MakeResult("retained", "retained", 1U)};
    finish_current(multiple);
    QCOMPARE(error_count_status->text(), QStringLiteral("Errors: 3"));
    QVERIFY(!MixedResultStatus(&dialog)->isHidden());
    QVERIFY(FailureStatus(&dialog)->isHidden());

    multiple.partial = true;
    finish_current(multiple);
    QCOMPARE(error_count_status->text(), QStringLiteral("Errors: 3"));
    QVERIFY(!MixedResultStatus(&dialog)->isHidden());
    QVERIFY(!PartialStatus(&dialog)->isHidden());
    QVERIFY(PartialEmptyStatus(&dialog)->isHidden());

    multiple.results.clear();
    finish_current(multiple);
    QCOMPARE(error_count_status->text(), QStringLiteral("Errors: 3"));
    QVERIFY(MixedResultStatus(&dialog)->isHidden());
    QVERIFY(!PartialStatus(&dialog)->isHidden());
    QVERIFY(!PartialEmptyStatus(&dialog)->isHidden());
    QVERIFY(FailureStatus(&dialog)->isHidden());

    for (const auto* label : dialog.findChildren<QLabel*>()) {
        QVERIFY(!label->text().contains(QStringLiteral("private-id")));
        QVERIFY(!label->text().contains(QStringLiteral("private detail")));
        QVERIFY(!label->text().contains(QStringLiteral("Internal")));
        QVERIFY(!label->text().contains(QStringLiteral("MalformedIndex")));
    }

    finish_current({});
    QVERIFY(error_count_status->isHidden());
    QVERIFY(error_count_status->text().isEmpty());

    finish_current(multiple);
    const std::uint64_t stale_generation = dialog.generation_;
    dialog.active_generation_ = ++dialog.generation_;
    dialog.FinishSearch(stale_generation, {});
    QCOMPARE(error_count_status->text(), QStringLiteral("Errors: 3"));

    finish_current({});
    dialog.active_generation_ = ++dialog.generation_;
    dialog.FinishSearch(dialog.generation_ - 1U, multiple);
    QVERIFY(error_count_status->isHidden());

    service.response_ = multiple;
    dialog.InitializeQuery(QStringLiteral("replacement"));
    dialog.SubmitSearch();
    QVERIFY(error_count_status->isHidden());
    QVERIFY(error_count_status->text().isEmpty());
    QVERIFY(service.WaitForQueries(1U));
    QTRY_COMPARE(error_count_status->text(), QStringLiteral("Errors: 3"));
    QVERIFY(!error_count_status->isHidden());

    dialog.SetService(nullptr);
    QCOMPARE(error_count_status->text(), QStringLiteral("Errors: 3"));
    dialog.DetachController();
    QCOMPARE(error_count_status->text(), QStringLiteral("Errors: 3"));
}

void FullTextSearchDialogTest::
    ShowsOnlyGenericPartialEmptyStatusForPartialResponsesWithoutResults() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* partial_empty_status = PartialEmptyStatus(&dialog);
    QVERIFY(partial_empty_status != nullptr);
    QCOMPARE(partial_empty_status->parent(), &dialog);
    QCOMPARE(partial_empty_status->text(),
             QStringLiteral("No matches in searched dictionaries"));
    QVERIFY(partial_empty_status->isHidden());

    const auto finish_current =
        [&dialog](goldendict::core::FullTextResponse response) {
            dialog.active_generation_ = ++dialog.generation_;
            dialog.FinishSearch(dialog.generation_, std::move(response));
        };
    const std::vector<goldendict::core::FullTextErrorCode> error_codes = {
        goldendict::core::FullTextErrorCode::kInvalidQuery,
        goldendict::core::FullTextErrorCode::kDictionaryUnavailable,
        goldendict::core::FullTextErrorCode::kUnsupported,
        goldendict::core::FullTextErrorCode::kMalformedIndex,
        goldendict::core::FullTextErrorCode::kCancelled,
        goldendict::core::FullTextErrorCode::kDeadlineExceeded,
        goldendict::core::FullTextErrorCode::kResourceLimit,
        goldendict::core::FullTextErrorCode::kInternal,
    };

    goldendict::core::FullTextResponse partial_empty;
    partial_empty.partial = true;
    finish_current(partial_empty);
    QVERIFY(!partial_empty_status->isHidden());
    QVERIFY(!PartialStatus(&dialog)->isHidden());
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 0"));
    QVERIFY(EmptyStatus(&dialog)->isHidden());
    QVERIFY(FailureStatus(&dialog)->isHidden());
    QVERIFY(MixedResultStatus(&dialog)->isHidden());

    for (const auto code : error_codes) {
        partial_empty.errors = {
            {code, "private-dictionary", "private backend detail"}};
        finish_current(partial_empty);
        QVERIFY(!partial_empty_status->isHidden());
        QVERIFY(!PartialStatus(&dialog)->isHidden());
        QVERIFY(FailureStatus(&dialog)->isHidden());
        QVERIFY(MixedResultStatus(&dialog)->isHidden());
    }

    partial_empty.errors.push_back(
        {goldendict::core::FullTextErrorCode::kMalformedIndex,
         "second-private-id", "second private detail"});
    finish_current(partial_empty);
    QVERIFY(!partial_empty_status->isHidden());
    for (const auto* label : dialog.findChildren<QLabel*>()) {
        QVERIFY(!label->text().contains(QStringLiteral("private-dictionary")));
        QVERIFY(!label->text().contains(QStringLiteral("private-id")));
        QVERIFY(!label->text().contains(QStringLiteral("private detail")));
        QVERIFY(!label->text().contains(QStringLiteral("MalformedIndex")));
    }

    goldendict::core::FullTextResponse partial_result = partial_empty;
    partial_result.results = {MakeResult("retained", "retained", 1U)};
    finish_current(partial_result);
    QVERIFY(partial_empty_status->isHidden());
    QVERIFY(!PartialStatus(&dialog)->isHidden());
    QVERIFY(!MixedResultStatus(&dialog)->isHidden());

    finish_current({});
    QVERIFY(partial_empty_status->isHidden());
    QVERIFY(!EmptyStatus(&dialog)->isHidden());

    goldendict::core::FullTextResponse error_only;
    error_only.errors.push_back({goldendict::core::FullTextErrorCode::kInternal,
                                 "private", "private detail"});
    finish_current(error_only);
    QVERIFY(partial_empty_status->isHidden());
    QVERIFY(!FailureStatus(&dialog)->isHidden());

    goldendict::core::FullTextResponse result_only;
    result_only.results = {MakeResult("result", "result", 2U)};
    finish_current(result_only);
    QVERIFY(partial_empty_status->isHidden());

    result_only.errors = error_only.errors;
    finish_current(result_only);
    QVERIFY(partial_empty_status->isHidden());
    QVERIFY(!MixedResultStatus(&dialog)->isHidden());

    finish_current(partial_empty);
    QVERIFY(!partial_empty_status->isHidden());
    const std::uint64_t stale_generation = dialog.generation_;
    dialog.active_generation_ = ++dialog.generation_;
    dialog.FinishSearch(stale_generation, result_only);
    QVERIFY(!partial_empty_status->isHidden());

    finish_current(result_only);
    dialog.active_generation_ = ++dialog.generation_;
    dialog.FinishSearch(dialog.generation_ - 1U, partial_empty);
    QVERIFY(partial_empty_status->isHidden());

    service.response_ = partial_empty;
    dialog.InitializeQuery(QStringLiteral("replacement"));
    dialog.SubmitSearch();
    QVERIFY(partial_empty_status->isHidden());
    QVERIFY(service.WaitForQueries(1U));
    QTRY_VERIFY(!partial_empty_status->isHidden());
    QVERIFY(!PartialStatus(&dialog)->isHidden());

    dialog.SetService(nullptr);
    QVERIFY(!partial_empty_status->isHidden());
    dialog.DetachController();
    QVERIFY(!partial_empty_status->isHidden());
}

void FullTextSearchDialogTest::
    ShowsOnlyGenericMixedResultStatusForResponsesWithResultsAndErrors() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* mixed_result_status = MixedResultStatus(&dialog);
    VerifyProgressAlignmentContract(&dialog);
    QVERIFY(mixed_result_status != nullptr);
    QCOMPARE(mixed_result_status->parent(), &dialog);
    QCOMPARE(mixed_result_status->text(),
             QStringLiteral("Some dictionaries could not be searched"));
    QVERIFY(mixed_result_status->isHidden());

    const auto finish_current =
        [&dialog](goldendict::core::FullTextResponse response) {
            dialog.active_generation_ = ++dialog.generation_;
            dialog.FinishSearch(dialog.generation_, std::move(response));
        };
    const std::vector<goldendict::core::FullTextErrorCode> error_codes = {
        goldendict::core::FullTextErrorCode::kInvalidQuery,
        goldendict::core::FullTextErrorCode::kDictionaryUnavailable,
        goldendict::core::FullTextErrorCode::kUnsupported,
        goldendict::core::FullTextErrorCode::kMalformedIndex,
        goldendict::core::FullTextErrorCode::kCancelled,
        goldendict::core::FullTextErrorCode::kDeadlineExceeded,
        goldendict::core::FullTextErrorCode::kResourceLimit,
        goldendict::core::FullTextErrorCode::kInternal,
    };
    for (const auto code : error_codes) {
        goldendict::core::FullTextResponse mixed;
        mixed.results = {MakeResult("retained", "retained", 1U)};
        mixed.errors.push_back(
            {code, "private-dictionary", "private backend detail"});
        finish_current(std::move(mixed));
        QVERIFY(!mixed_result_status->isHidden());
        QCOMPARE(mixed_result_status->text(),
                 QStringLiteral("Some dictionaries could not be searched"));
        QCOMPARE(ArticlesFound(&dialog)->text(),
                 QStringLiteral("Articles found: 1"));
        QVERIFY(PartialStatus(&dialog)->isHidden());
        QVERIFY(EmptyStatus(&dialog)->isHidden());
        QVERIFY(FailureStatus(&dialog)->isHidden());
    }

    goldendict::core::FullTextResponse multiple;
    multiple.results = {MakeResult("first", "first", 2U),
                        MakeResult("second", "second", 3U)};
    multiple.errors = {
        {goldendict::core::FullTextErrorCode::kMalformedIndex,
         "first-private-id", "first private detail"},
        {goldendict::core::FullTextErrorCode::kInternal, "second-private-id",
         "second private detail"},
    };
    finish_current(multiple);
    QVERIFY(!mixed_result_status->isHidden());
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 2"));
    for (const auto* label : dialog.findChildren<QLabel*>()) {
        QVERIFY(!label->text().contains(QStringLiteral("private-id")));
        QVERIFY(!label->text().contains(QStringLiteral("private detail")));
        QVERIFY(!label->text().contains(QStringLiteral("MalformedIndex")));
        QVERIFY(!label->text().contains(QStringLiteral("Internal")));
    }

    multiple.partial = true;
    finish_current(multiple);
    QVERIFY(!mixed_result_status->isHidden());
    QVERIFY(!PartialStatus(&dialog)->isHidden());
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 2"));
    QVERIFY(EmptyStatus(&dialog)->isHidden());
    QVERIFY(FailureStatus(&dialog)->isHidden());

    goldendict::core::FullTextResponse result_only;
    result_only.results = {MakeResult("result", "result", 4U)};
    finish_current(result_only);
    QVERIFY(mixed_result_status->isHidden());
    finish_current({});
    QVERIFY(mixed_result_status->isHidden());

    goldendict::core::FullTextResponse error_only;
    error_only.errors.push_back({goldendict::core::FullTextErrorCode::kInternal,
                                 "error-only", "error-only detail"});
    finish_current(error_only);
    QVERIFY(mixed_result_status->isHidden());
    QVERIFY(!FailureStatus(&dialog)->isHidden());

    error_only.partial = true;
    finish_current(error_only);
    QVERIFY(mixed_result_status->isHidden());
    QVERIFY(!PartialStatus(&dialog)->isHidden());
    QVERIFY(FailureStatus(&dialog)->isHidden());

    finish_current(multiple);
    QVERIFY(!mixed_result_status->isHidden());
    const std::uint64_t stale_generation = dialog.generation_;
    dialog.active_generation_ = ++dialog.generation_;
    dialog.FinishSearch(stale_generation, {});
    QVERIFY(!mixed_result_status->isHidden());
    QVERIFY(!PartialStatus(&dialog)->isHidden());

    finish_current(result_only);
    QVERIFY(mixed_result_status->isHidden());
    dialog.active_generation_ = ++dialog.generation_;
    dialog.FinishSearch(dialog.generation_ - 1U, multiple);
    QVERIFY(mixed_result_status->isHidden());

    service.response_ = multiple;
    dialog.InitializeQuery(QStringLiteral("replacement"));
    dialog.SubmitSearch();
    QVERIFY(mixed_result_status->isHidden());
    QVERIFY(service.WaitForQueries(1U));
    QTRY_VERIFY(!mixed_result_status->isHidden());
    QVERIFY(!PartialStatus(&dialog)->isHidden());
}

void FullTextSearchDialogTest::
    ShowsOnlyGenericTerminalFailureForErrorOnlyResponses() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* failure_status = FailureStatus(&dialog);
    QVERIFY(failure_status != nullptr);
    QCOMPARE(failure_status->parent(), &dialog);
    QCOMPARE(failure_status->text(), QStringLiteral("Full-text search failed"));
    QVERIFY(failure_status->isHidden());

    const auto finish_current =
        [&dialog](goldendict::core::FullTextResponse response) {
            dialog.active_generation_ = ++dialog.generation_;
            dialog.FinishSearch(dialog.generation_, std::move(response));
        };
    const std::vector<goldendict::core::FullTextErrorCode> error_codes = {
        goldendict::core::FullTextErrorCode::kInvalidQuery,
        goldendict::core::FullTextErrorCode::kDictionaryUnavailable,
        goldendict::core::FullTextErrorCode::kUnsupported,
        goldendict::core::FullTextErrorCode::kMalformedIndex,
        goldendict::core::FullTextErrorCode::kCancelled,
        goldendict::core::FullTextErrorCode::kDeadlineExceeded,
        goldendict::core::FullTextErrorCode::kResourceLimit,
        goldendict::core::FullTextErrorCode::kInternal,
    };
    for (const auto code : error_codes) {
        goldendict::core::FullTextResponse error_only;
        error_only.errors.push_back(
            {code, "private-dictionary", "private backend detail"});
        finish_current(std::move(error_only));
        QVERIFY(!failure_status->isHidden());
        QCOMPARE(failure_status->text(),
                 QStringLiteral("Full-text search failed"));
    }

    goldendict::core::FullTextResponse multiple_errors;
    multiple_errors.errors = {
        {goldendict::core::FullTextErrorCode::kMalformedIndex,
         "first-private-id", "first private detail"},
        {goldendict::core::FullTextErrorCode::kInternal, "second-private-id",
         "second private detail"},
    };
    finish_current(std::move(multiple_errors));
    QVERIFY(!failure_status->isHidden());
    for (const auto* label : dialog.findChildren<QLabel*>()) {
        QVERIFY(!label->text().contains(QStringLiteral("private-id")));
        QVERIFY(!label->text().contains(QStringLiteral("private detail")));
        QVERIFY(!label->text().contains(QStringLiteral("MalformedIndex")));
        QVERIFY(!label->text().contains(QStringLiteral("Internal")));
    }

    finish_current({});
    QVERIFY(failure_status->isHidden());

    goldendict::core::FullTextResponse nonempty;
    nonempty.results = {MakeResult("retained", "retained", 1U)};
    finish_current(nonempty);
    QVERIFY(failure_status->isHidden());

    nonempty.errors.push_back({goldendict::core::FullTextErrorCode::kInternal,
                               "contained", "contained detail"});
    finish_current(nonempty);
    QVERIFY(failure_status->isHidden());

    goldendict::core::FullTextResponse partial_empty;
    partial_empty.partial = true;
    partial_empty.errors.push_back(
        {goldendict::core::FullTextErrorCode::kDeadlineExceeded, "partial",
         "partial detail"});
    finish_current(partial_empty);
    QVERIFY(failure_status->isHidden());

    partial_empty.results = {MakeResult("partial", "partial", 2U)};
    finish_current(partial_empty);
    QVERIFY(failure_status->isHidden());

    goldendict::core::FullTextResponse error_only;
    error_only.errors.push_back({goldendict::core::FullTextErrorCode::kInternal,
                                 "replacement", "replacement detail"});
    finish_current(error_only);
    QVERIFY(!failure_status->isHidden());
    service.response_ = error_only;
    dialog.InitializeQuery(QStringLiteral("replacement"));
    dialog.SubmitSearch();
    QVERIFY(failure_status->isHidden());
    QVERIFY(service.WaitForQueries(1U));
    QTRY_VERIFY(!failure_status->isHidden());
}

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
    QVERIFY(FailureStatus(&dialog)->isHidden());
    QVERIFY(PartialStatus(&dialog)->isHidden());
    QVERIFY(EmptyStatus(&dialog)->isHidden());
    QVERIFY(MixedResultStatus(&dialog)->isHidden());
    QVERIFY(PartialEmptyStatus(&dialog)->isHidden());
    QVERIFY(FailureStatus(&dialog)->isHidden());
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
    CompareIntent(activations.back(), expected, false, {}, "needle", false);

    results->setCurrentIndex(index);
    results->setFocus();
    QTest::keyClick(results, Qt::Key_Return);
    QCOMPARE(activations.size(), std::size_t{2});
    CompareIntent(activations.back(), expected, false, {}, "needle", false);

    QTest::keyClick(results, Qt::Key_Enter, Qt::KeypadModifier);
    QCOMPARE(activations.size(), std::size_t{3});
    CompareIntent(activations.back(), expected, false, {}, "needle", false);
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
    dialog.accepted_activation_context_ =
        FullTextSearchDialog::ActivationContext{"first-query", false};

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
    dialog.accepted_activation_context_.reset();
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
    dialog.accepted_activation_context_ =
        FullTextSearchDialog::ActivationContext{"replacement-query", true};
    dialog.ActivateResult(replacement_index);
    QCOMPARE(activations.size(), std::size_t{2});
    const auto copied = activations.back();
    model->Reset({});
    model->Reset(replacement);
    model->Reset({});
    CompareIntent(copied, replacement.results.front(), true,
                  {"replacement-scope"}, "replacement-query", true);
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
    auto* failure_status = FailureStatus(&dialog);
    auto* mixed_result_status = MixedResultStatus(&dialog);
    VerifySearchButtonDefaultPolicy(&dialog);
    QCOMPARE(search, dialog.search_button_);
    QVERIFY(search != nullptr);
    QVERIFY(cancel != nullptr);
    QVERIFY(progress != nullptr);
    QVERIFY(response_model != nullptr);
    QVERIFY(results != nullptr);
    QVERIFY(articles_found != nullptr);
    QVERIFY(partial_status != nullptr);
    QVERIFY(empty_status != nullptr);
    QVERIFY(failure_status != nullptr);
    QVERIFY(mixed_result_status != nullptr);
    QCOMPARE(response_model, dialog.response_model_);
    QCOMPARE(results, dialog.results_);
    QCOMPARE(response_model->parent(), &dialog);
    QCOMPARE(results->parent(), &dialog);
    QCOMPARE(articles_found->parent(), &dialog);
    QCOMPARE(partial_status->parent(), &dialog);
    QCOMPARE(empty_status->parent(), &dialog);
    QCOMPARE(failure_status->parent(), &dialog);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QCOMPARE(partial_status->text(),
             QStringLiteral("Results may be incomplete."));
    QCOMPARE(empty_status->text(), QStringLiteral("No matches"));
    QCOMPARE(failure_status->text(), QStringLiteral("Full-text search failed"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
    QVERIFY(failure_status->isHidden());
    QVERIFY(mixed_result_status->isHidden());
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
    QVERIFY(cancel->isEnabled());
    QVERIFY(progress->isHidden());
    VerifySearchButtonDefaultPolicy(&dialog);

    search->click();
    VerifyProgressAlignmentContract(&dialog);
    QVERIFY(!search->isEnabled());
    QVERIFY(cancel->isEnabled());
    QVERIFY(!progress->isHidden());
    VerifySearchButtonDefaultPolicy(&dialog);
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
    VerifyProgressAlignmentContract(&dialog);
    QVERIFY(search->isEnabled());
    QVERIFY(cancel->isEnabled());
    QVERIFY(progress->isHidden());
    VerifySearchButtonDefaultPolicy(&dialog);
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
    auto* button_row = ButtonRow(&dialog);
    QVERIFY(button_row != nullptr);
    VerifyButtonRow(&dialog, button_row);
    auto* result_count_progress_row = ResultCountProgressRow(&dialog);
    QVERIFY(result_count_progress_row != nullptr);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    VerifySearchButtonDefaultPolicy(&dialog);
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
    VerifyProgressAlignmentContract(&dialog);
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

void FullTextSearchDialogTest::
    RetainsExactAcceptedScopeAndQueryContextForByValueActivation() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    preferences.full_text_ignore_diacritics = true;
    FullTextSearchDialog dialog(preferences, &service);
    auto* button_row = ButtonRow(&dialog);
    QVERIFY(button_row != nullptr);
    VerifyButtonRow(&dialog, button_row);
    auto* model = ResponseModel(&dialog);
    auto* results = Results(&dialog);
    const auto ordered_result = MakeResult("ordered", "ordered", 5U);
    service.response_.results = {ordered_result};

    goldendict::core::FullTextQuery ordered_scope;
    ordered_scope.dictionary_filter_active = true;
    ordered_scope.dictionary_ids = {"second", "first", "second"};
    dialog.SetProjectedQuery(ordered_scope);
    dialog.InitializeQuery(QString::fromUtf8(u8"ordered café"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));
    VerifyButtonRow(&dialog, button_row);
    VerifyProgressAlignmentContract(&dialog);
    VerifySearchButtonDefaultPolicy(&dialog);
    QTRY_COMPARE(model->rowCount(), 1);
    QVERIFY(dialog.accepted_activation_scope_.has_value());
    QVERIFY(dialog.accepted_activation_scope_->dictionary_filter_active);
    QCOMPARE(dialog.accepted_activation_scope_->dictionary_ids,
             ordered_scope.dictionary_ids);
    QVERIFY(dialog.accepted_activation_context_.has_value());
    QCOMPARE(dialog.accepted_activation_context_->query_text,
             std::string(u8"ordered café"));
    QVERIFY(dialog.accepted_activation_context_->ignore_diacritics);

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
                  ordered_scope.dictionary_ids, u8"ordered café", true);

    activations.back().result.headword = "mutated copy";
    activations.back().dictionary_ids.clear();
    activations.back().query_text = "mutated query copy";
    activations.back().ignore_diacritics = false;
    dialog.ActivateResult(ordered_index);
    QCOMPARE(activations.size(), std::size_t{2});
    CompareIntent(activations.back(), ordered_result, true,
                  ordered_scope.dictionary_ids, u8"ordered café", true);

    dialog.InitializeQuery(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    VerifyButtonRow(&dialog, button_row);
    QVERIFY(service.WaitForQueries(2U));
    QVERIFY(!dialog.response_.has_value());
    QCOMPARE(model->rowCount(), 0);
    QVERIFY(!dialog.accepted_activation_scope_.has_value());
    QVERIFY(!dialog.accepted_activation_context_.has_value());
    dialog.ActivateResult(ordered_index);
    QCOMPARE(activations.size(), std::size_t{2});
    dialog.CancelSearch();
    QVERIFY(service.WaitForCancellation());
    service.ReleaseCancelledRequest();

    const auto empty_result = MakeResult("empty", "empty", 6U);
    service.response_.results = {empty_result};
    auto* ignore_diacritics = dialog.findChild<QCheckBox*>(
        QStringLiteral("fullTextIgnoreDiacritics"));
    QVERIFY(ignore_diacritics != nullptr);
    ignore_diacritics->setChecked(false);
    goldendict::core::FullTextQuery empty_scope;
    empty_scope.dictionary_filter_active = true;
    dialog.SetProjectedQuery(empty_scope);
    dialog.InitializeQuery(QStringLiteral("empty"));
    dialog.SubmitSearch();
    VerifyButtonRow(&dialog, button_row);
    QVERIFY(service.WaitForQueries(3U));
    QTRY_COMPARE(model->rowCount(), 1);
    const QModelIndex empty_index = model->index(0, 0);
    results->setCurrentIndex(empty_index);
    dialog.ActivateResult(empty_index);
    QCOMPARE(activations.size(), std::size_t{3});
    CompareIntent(activations.back(), empty_result, true, {}, "empty", false);

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
    CompareIntent(activations.back(), absent_result, false, {}, "absent",
                  false);

    const auto copied = activations.back();
    dialog.accepted_activation_scope_.reset();
    dialog.accepted_activation_context_.reset();
    model->Reset({});
    CompareIntent(copied, absent_result, false, {}, "absent", false);
    dialog.ActivateResult(absent_index);
    QCOMPARE(activations.size(), std::size_t{4});
}

void FullTextSearchDialogTest::
    ProjectsCompleteCurrentResponsesAndReplacesRowsAtomically() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* button_row = ButtonRow(&dialog);
    QVERIFY(button_row != nullptr);
    VerifyButtonRow(&dialog, button_row);
    auto* model = ResponseModel(&dialog);
    auto* results = Results(&dialog);
    auto* articles_found = ArticlesFound(&dialog);
    auto* partial_status = PartialStatus(&dialog);
    auto* empty_status = EmptyStatus(&dialog);
    auto* failure_status = FailureStatus(&dialog);
    auto* mixed_result_status = MixedResultStatus(&dialog);
    QVERIFY(model != nullptr);
    QVERIFY(results != nullptr);
    QVERIFY(partial_status != nullptr);
    QVERIFY(empty_status != nullptr);
    QVERIFY(failure_status != nullptr);
    QVERIFY(mixed_result_status != nullptr);
    QCOMPARE(results->model(), model);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
    QVERIFY(failure_status->isHidden());
    QVERIFY(mixed_result_status->isHidden());

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
    QVERIFY(!failure_status->isHidden());
    QVERIFY(mixed_result_status->isHidden());

    goldendict::core::FullTextResponse conclusive_empty;
    finish_current(conclusive_empty);
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(!empty_status->isHidden());
    QVERIFY(failure_status->isHidden());
    QVERIFY(mixed_result_status->isHidden());

    goldendict::core::FullTextResponse empty_partial;
    empty_partial.partial = true;
    finish_current(empty_partial);
    QVERIFY(dialog.response_->partial);
    QCOMPARE(model->rowCount(), 0);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 0"));
    QVERIFY(!partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
    QVERIFY(failure_status->isHidden());
    QVERIFY(mixed_result_status->isHidden());

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
    QVERIFY(failure_status->isHidden());
    QVERIFY(!mixed_result_status->isHidden());
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
    QVERIFY(failure_status->isHidden());
    QVERIFY(!mixed_result_status->isHidden());

    finish_current({});
    QCOMPARE(model->rowCount(), 0);
    QVERIFY(partial_status->isHidden());
    QVERIFY(!empty_status->isHidden());
    QVERIFY(failure_status->isHidden());
    QVERIFY(mixed_result_status->isHidden());

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
    QVERIFY(failure_status->isHidden());
    QVERIFY(mixed_result_status->isHidden());
    QCOMPARE(resets.size(), 1);
    QVERIFY(service.WaitForQueries(1U));
    QTRY_VERIFY(dialog.response_.has_value());
    VerifyButtonRow(&dialog, button_row);
    VerifyProgressAlignmentContract(&dialog);
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(articles_found->text(), QStringLiteral("Articles found: 3"));
    QVERIFY(partial_status->isHidden());
    QVERIFY(empty_status->isHidden());
    QVERIFY(failure_status->isHidden());
    QVERIFY(mixed_result_status->isHidden());
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
    auto* search_group = SearchGroupBox(&dialog);
    QVERIFY(search_group != nullptr);
    auto* composer = dialog.composer_;
    VerifySearchGroupBox(&dialog, search_group, composer);
    auto* button_row = ButtonRow(&dialog);
    QVERIFY(button_row != nullptr);
    VerifyButtonRow(&dialog, button_row);
    auto* result_count_progress_row = ResultCountProgressRow(&dialog);
    QVERIFY(result_count_progress_row != nullptr);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    std::size_t notifications = 0U;
    dialog.completion_notifier_ = [&notifications]() {
        ++notifications;
    };
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
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyButtonRow(&dialog, button_row);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    QCOMPARE(notifications, std::size_t{0});
    QVERIFY(!search->isEnabled());
    goldendict::core::FullTextQuery pending_scope;
    pending_scope.dictionary_filter_active = true;
    pending_scope.dictionary_ids = {"pending-scope"};
    dialog.SetProjectedQuery(pending_scope);
    query->setText(QStringLiteral("never starts"));
    dialog.SubmitSearch();
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyButtonRow(&dialog, button_row);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    QCOMPARE(notifications, std::size_t{0});
    goldendict::core::FullTextQuery replacement_scope;
    replacement_scope.dictionary_ids = {"replacement", "duplicate",
                                        "replacement"};
    replacement_scope.dictionary_filter_active = true;
    dialog.SetProjectedQuery(replacement_scope);
    query->setText(QStringLiteral("replacement"));
    dialog.SubmitSearch();
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyButtonRow(&dialog, button_row);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    QCOMPARE(dialog.generation_, 3U);
    QCOMPARE(dialog.active_generation_, std::optional<std::uint64_t>(3U));
    VerifyResultCountMinimumHeight(&dialog);
    QVERIFY(service.WaitForCancellation());
    service.ReleaseCancelledRequest();
    QVERIFY(service.WaitForQueries(2U));
    QTRY_VERIFY(dialog.response_.has_value());
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyButtonRow(&dialog, button_row);
    VerifyResultCountProgressRow(&dialog, result_count_progress_row);
    VerifyProgressAlignmentContract(&dialog);
    VerifyResultCountMinimumHeight(&dialog);
    QCOMPARE(notifications, std::size_t{1});

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
    QVERIFY(dialog.accepted_activation_context_.has_value());
    QCOMPARE(dialog.accepted_activation_context_->query_text,
             std::string("replacement"));
    QVERIFY(!dialog.accepted_activation_context_->ignore_diacritics);
    QVERIFY(search->isEnabled());
}

void FullTextSearchDialogTest::
    ActiveCancellationRestoresIdleStateWithoutDismissal() {
    ControllableDictionaryService service;
    service.response_.partial = true;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);
    auto* search_group = SearchGroupBox(&dialog);
    QVERIFY(search_group != nullptr);
    auto* composer = dialog.composer_;
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyResultCountMinimumHeight(&dialog);
    VerifySearchButtonDefaultPolicy(&dialog);
    std::size_t notifications = 0U;
    dialog.completion_notifier_ = [&notifications]() {
        ++notifications;
    };
    QCOMPARE(notifications, std::size_t{0});
    dialog.InitializeQuery(QStringLiteral("blocked"));
    dialog.SubmitSearch();
    QVERIFY(service.WaitForQueries(1U));
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyResultCountMinimumHeight(&dialog);
    VerifySearchButtonDefaultPolicy(&dialog);
    QCOMPARE(notifications, std::size_t{0});

    CancelButton(&dialog)->click();
    QVERIFY(service.WaitForCancellation());
    VerifySearchGroupBox(&dialog, search_group, composer);
    VerifyProgressAlignmentContract(&dialog);
    VerifyResultCountMinimumHeight(&dialog);
    QVERIFY(!dialog.active_generation_.has_value());
    QVERIFY(!dialog.pending_activation_scope_.has_value());
    QVERIFY(!dialog.accepted_activation_scope_.has_value());
    QVERIFY(!dialog.pending_activation_context_.has_value());
    QVERIFY(!dialog.accepted_activation_context_.has_value());
    QVERIFY(!dialog.response_.has_value());
    QCOMPARE(ResponseModel(&dialog)->rowCount(), 0);
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 0"));
    QVERIFY(PartialStatus(&dialog)->isHidden());
    QVERIFY(EmptyStatus(&dialog)->isHidden());
    QVERIFY(FailureStatus(&dialog)->isHidden());
    QVERIFY(MixedResultStatus(&dialog)->isHidden());
    QVERIFY(SearchButton(&dialog)->isEnabled());
    QVERIFY(CancelButton(&dialog)->isEnabled());
    QVERIFY(Progress(&dialog)->isHidden());
    VerifySearchButtonDefaultPolicy(&dialog);

    service.ReleaseCancelledRequest();
    QTest::qWait(20);
    QCoreApplication::processEvents();
    QVERIFY(!dialog.response_.has_value());
    QCOMPARE(ResponseModel(&dialog)->rowCount(), 0);
    QCOMPARE(ArticlesFound(&dialog)->text(),
             QStringLiteral("Articles found: 0"));
    QVERIFY(PartialStatus(&dialog)->isHidden());
    QVERIFY(EmptyStatus(&dialog)->isHidden());
    QVERIFY(MixedResultStatus(&dialog)->isHidden());
    QCOMPARE(dialog.generation_, 1U);
    QCOMPARE(notifications, std::size_t{0});
    VerifySearchButtonDefaultPolicy(&dialog);
}

void FullTextSearchDialogTest::IdleCancelDismissesThroughDialogLifecycle() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;

    auto dismiss_idle_dialog = [&]() {
        QPointer<FullTextSearchDialog> dialog =
            new FullTextSearchDialog(preferences, &service);
        dialog->show();
        QVERIFY(dialog->isVisible());
        QVERIFY(CancelButton(dialog)->isEnabled());
        CancelButton(dialog)->click();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        QVERIFY(dialog.isNull());
    };
    dismiss_idle_dialog();
    dismiss_idle_dialog();

    QPointer<FullTextSearchDialog> dialog =
        new FullTextSearchDialog(preferences, &service);
    dialog->show();
    dialog->InitializeQuery(QStringLiteral("blocked"));
    SearchButton(dialog)->click();
    QVERIFY(service.WaitForQueries(1U));
    CancelButton(dialog)->click();
    QVERIFY(service.WaitForCancellation());
    QVERIFY(!dialog.isNull());
    QVERIFY(dialog->isVisible());
    QVERIFY(SearchButton(dialog)->isEnabled());
    QVERIFY(CancelButton(dialog)->isEnabled());
    QVERIFY(Progress(dialog)->isHidden());

    service.ReleaseCancelledRequest();
    QTest::qWait(20);
    QCoreApplication::processEvents();
    QVERIFY(!dialog.isNull());
    CancelButton(dialog)->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QVERIFY(dialog.isNull());
}

void FullTextSearchDialogTest::EnforcesPinnedMinimumAcrossResizeAndRestore() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;

    FullTextSearchDialog dialog(preferences, &service);
    QCOMPARE(dialog.minimumWidth(), 430);
    QCOMPARE(dialog.minimumHeight(), 450);
    QCOMPARE(dialog.size(), QSize(492, 593));

    FullTextSearchDialog absent_dialog(preferences, &service, {});
    QCOMPARE(absent_dialog.size(), QSize(492, 593));

    FullTextSearchDialog rejected_dialog(preferences, &service,
                                         "not-qt-geometry");
    QCOMPARE(rejected_dialog.size(), QSize(492, 593));

    dialog.resize(1, 1);
    QCOMPARE(dialog.width(), 430);
    QCOMPARE(dialog.height(), 450);

    dialog.resize(617, 701);
    QCOMPARE(dialog.size(), QSize(617, 701));

    QDialog larger_source;
    larger_source.resize(641, 733);
    larger_source.show();
    QCoreApplication::processEvents();
    const QByteArray larger_geometry = larger_source.saveGeometry();
    const std::string larger_bytes(
        larger_geometry.constData(),
        static_cast<std::size_t>(larger_geometry.size()));

    FullTextSearchDialog larger_restored_dialog(preferences, &service,
                                                larger_bytes);
    larger_restored_dialog.show();
    QCoreApplication::processEvents();
    QCOMPARE(larger_restored_dialog.size(), QSize(641, 733));

    QDialog undersized_source;
    undersized_source.resize(311, 277);
    undersized_source.show();
    QCoreApplication::processEvents();
    const QByteArray undersized_geometry = undersized_source.saveGeometry();
    const std::string undersized_bytes(
        undersized_geometry.constData(),
        static_cast<std::size_t>(undersized_geometry.size()));

    FullTextSearchDialog restored_dialog(preferences, &service,
                                         undersized_bytes);
    restored_dialog.show();
    QCoreApplication::processEvents();
    QCOMPARE(restored_dialog.minimumWidth(), 430);
    QCOMPARE(restored_dialog.minimumHeight(), 450);
    QVERIFY(restored_dialog.width() >= 430);
    QVERIFY(restored_dialog.height() >= 450);
}

void FullTextSearchDialogTest::
    RestoresAndCapturesGeometryOnlyForIdleDismissal() {
    ControllableDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;

    FullTextSearchDialog default_dialog(preferences, &service);
    default_dialog.show();
    QCoreApplication::processEvents();
    const QRect default_geometry = default_dialog.geometry();

    FullTextSearchDialog absent_dialog(preferences, &service);
    absent_dialog.show();
    QCoreApplication::processEvents();
    QCOMPARE(absent_dialog.geometry(), default_geometry);

    FullTextSearchDialog invalid_dialog(preferences, &service,
                                        "not-qt-geometry");
    invalid_dialog.show();
    QCoreApplication::processEvents();
    QCOMPARE(invalid_dialog.geometry(), default_geometry);

    default_dialog.resize(default_geometry.width() + 73,
                          default_geometry.height() + 41);
    default_dialog.move(default_geometry.topLeft() + QPoint(29, 31));
    const QByteArray saved = default_dialog.saveGeometry();
    const std::string saved_bytes(saved.constData(),
                                  static_cast<std::size_t>(saved.size()));

    QPointer<FullTextSearchDialog> restored_dialog =
        new FullTextSearchDialog(preferences, &service, saved_bytes);
    restored_dialog->show();
    QCoreApplication::processEvents();
    QCOMPARE(restored_dialog->saveGeometry(), saved);

    std::vector<std::string> captures;
    connect(restored_dialog, &FullTextSearchDialog::GeometryCaptured,
            restored_dialog, [&captures](std::string geometry) {
                captures.push_back(std::move(geometry));
            });
    const QByteArray idle_cancel_geometry = restored_dialog->saveGeometry();
    CancelButton(restored_dialog)->click();
    QCOMPARE(captures.size(), std::size_t{1});
    QCOMPARE(
        captures.front(),
        std::string(idle_cancel_geometry.constData(),
                    static_cast<std::size_t>(idle_cancel_geometry.size())));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(restored_dialog.isNull());

    auto* close_dialog = new FullTextSearchDialog(preferences, &service);
    std::vector<std::string> close_captures;
    connect(close_dialog, &FullTextSearchDialog::GeometryCaptured, close_dialog,
            [&close_captures](std::string geometry) {
                close_captures.push_back(std::move(geometry));
            });
    close_dialog->show();
    QCoreApplication::processEvents();
    const QByteArray close_geometry = close_dialog->saveGeometry();
    close_dialog->close();
    QCOMPARE(close_captures.size(), std::size_t{1});
    QCOMPARE(close_captures.front(),
             std::string(close_geometry.constData(),
                         static_cast<std::size_t>(close_geometry.size())));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    FullTextSearchDialog active_dialog(preferences, &service);
    std::size_t active_captures = 0U;
    connect(&active_dialog, &FullTextSearchDialog::GeometryCaptured,
            &active_dialog,
            [&active_captures](const std::string&) { ++active_captures; });
    active_dialog.show();
    active_dialog.InitializeQuery(QStringLiteral("blocked"));
    SearchButton(&active_dialog)->click();
    QVERIFY(service.WaitForQueries(1U));
    CancelButton(&active_dialog)->click();
    QVERIFY(service.WaitForCancellation());
    QCOMPARE(active_captures, std::size_t{0});
    QVERIFY(active_dialog.isVisible());
    service.ReleaseCancelledRequest();
}

void FullTextSearchDialogTest::
    ServiceReplacementDetachAndDestructionSuppressLateDelivery() {
    ControllableDictionaryService first;
    ControllableDictionaryService second;
    goldendict::core::ApplicationPreferences preferences;
    std::size_t notifications = 0U;
    {
        FullTextSearchDialog dialog(preferences, &first);
        auto* search_group = SearchGroupBox(&dialog);
        QVERIFY(search_group != nullptr);
        auto* composer = dialog.composer_;
        VerifySearchGroupBox(&dialog, search_group, composer);
        auto* button_row = ButtonRow(&dialog);
        QVERIFY(button_row != nullptr);
        VerifyButtonRow(&dialog, button_row);
        auto* result_count_progress_row = ResultCountProgressRow(&dialog);
        QVERIFY(result_count_progress_row != nullptr);
        VerifyResultCountProgressRow(&dialog, result_count_progress_row);
        VerifyProgressAlignmentContract(&dialog);
        VerifyResultCountMinimumHeight(&dialog);
        dialog.completion_notifier_ = [&notifications]() {
            ++notifications;
        };
        first.response_.partial = true;
        first.response_.results.resize(1);
        first.response_.results.front().headword = "accepted";
        first.response_.errors.push_back(
            {goldendict::core::FullTextErrorCode::kInternal, "private-id",
             "private detail"});
        dialog.InitializeQuery(QStringLiteral("accepted"));
        dialog.SubmitSearch();
        QVERIFY(first.WaitForQueries(1U));
        QTRY_VERIFY(dialog.response_.has_value());
        VerifySearchGroupBox(&dialog, search_group, composer);
        VerifyButtonRow(&dialog, button_row);
        VerifyResultCountProgressRow(&dialog, result_count_progress_row);
        VerifyProgressAlignmentContract(&dialog);
        QCOMPARE(notifications, std::size_t{1});
        QCOMPARE(ResponseModel(&dialog)->rowCount(), 1);
        QCOMPARE(ArticlesFound(&dialog)->text(),
                 QStringLiteral("Articles found: 1"));
        QVERIFY(!PartialStatus(&dialog)->isHidden());
        QVERIFY(EmptyStatus(&dialog)->isHidden());
        QVERIFY(FailureStatus(&dialog)->isHidden());
        QVERIFY(!MixedResultStatus(&dialog)->isHidden());
        QVERIFY(dialog.accepted_activation_scope_.has_value());
        QVERIFY(dialog.accepted_activation_context_.has_value());
        QCOMPARE(dialog.accepted_activation_context_->query_text,
                 std::string("accepted"));
        dialog.SetService(&second);
        VerifySearchGroupBox(&dialog, search_group, composer);
        VerifyButtonRow(&dialog, button_row);
        VerifyResultCountProgressRow(&dialog, result_count_progress_row);
        VerifyProgressAlignmentContract(&dialog);
        VerifyResultCountMinimumHeight(&dialog);
        QCOMPARE(notifications, std::size_t{1});
        QVERIFY(dialog.response_.has_value());
        QCOMPARE(dialog.response_->results.front().headword,
                 std::string("accepted"));
        QCOMPARE(ResponseModel(&dialog)->rowCount(), 1);
        QCOMPARE(ArticlesFound(&dialog)->text(),
                 QStringLiteral("Articles found: 1"));
        QVERIFY(!PartialStatus(&dialog)->isHidden());
        QVERIFY(EmptyStatus(&dialog)->isHidden());
        QVERIFY(FailureStatus(&dialog)->isHidden());
        QVERIFY(!MixedResultStatus(&dialog)->isHidden());
        QVERIFY(dialog.accepted_activation_scope_.has_value());
        QVERIFY(!dialog.pending_activation_scope_.has_value());
        QVERIFY(dialog.accepted_activation_context_.has_value());
        QVERIFY(!dialog.pending_activation_context_.has_value());
        dialog.DetachController();
        dialog.DetachController();
        VerifySearchGroupBox(&dialog, search_group, composer);
        VerifyButtonRow(&dialog, button_row);
        VerifyResultCountProgressRow(&dialog, result_count_progress_row);
        VerifyProgressAlignmentContract(&dialog);
        VerifyResultCountMinimumHeight(&dialog);
        QCOMPARE(notifications, std::size_t{1});
        QVERIFY(dialog.response_.has_value());
        QCOMPARE(ResponseModel(&dialog)->rowCount(), 1);
        QVERIFY(!PartialStatus(&dialog)->isHidden());
        QVERIFY(EmptyStatus(&dialog)->isHidden());
        QVERIFY(FailureStatus(&dialog)->isHidden());
        QVERIFY(!MixedResultStatus(&dialog)->isHidden());
        QVERIFY(dialog.accepted_activation_scope_.has_value());
        QVERIFY(dialog.accepted_activation_context_.has_value());
    }
    QCOMPARE(notifications, std::size_t{1});
    QCOMPARE(second.Queries().size(), std::size_t{0});

    ControllableDictionaryService blocked;
    blocked.response_.partial = true;
    ControllableDictionaryService replacement;
    {
        FullTextSearchDialog dialog(preferences, &blocked);
        auto* search_group = SearchGroupBox(&dialog);
        QVERIFY(search_group != nullptr);
        auto* composer = dialog.composer_;
        VerifySearchGroupBox(&dialog, search_group, composer);
        auto* button_row = ButtonRow(&dialog);
        QVERIFY(button_row != nullptr);
        VerifyButtonRow(&dialog, button_row);
        auto* result_count_progress_row = ResultCountProgressRow(&dialog);
        QVERIFY(result_count_progress_row != nullptr);
        VerifyResultCountProgressRow(&dialog, result_count_progress_row);
        VerifyProgressAlignmentContract(&dialog);
        VerifyResultCountMinimumHeight(&dialog);
        dialog.completion_notifier_ = [&notifications]() {
            ++notifications;
        };
        dialog.InitializeQuery(QStringLiteral("blocked"));
        dialog.SubmitSearch();
        QVERIFY(blocked.WaitForQueries(1U));
        VerifySearchGroupBox(&dialog, search_group, composer);
        VerifyButtonRow(&dialog, button_row);
        VerifyResultCountProgressRow(&dialog, result_count_progress_row);
        VerifyProgressAlignmentContract(&dialog);
        QCOMPARE(notifications, std::size_t{1});
        blocked.ReleaseCancelledRequest();
        dialog.SetService(&replacement);
        QVERIFY(blocked.WaitForCancellation());
        VerifySearchGroupBox(&dialog, search_group, composer);
        VerifyButtonRow(&dialog, button_row);
        VerifyResultCountProgressRow(&dialog, result_count_progress_row);
        VerifyProgressAlignmentContract(&dialog);
        VerifyResultCountMinimumHeight(&dialog);
        QVERIFY(!dialog.active_generation_.has_value());
        QVERIFY(!dialog.pending_activation_scope_.has_value());
        QVERIFY(!dialog.accepted_activation_scope_.has_value());
        QVERIFY(!dialog.pending_activation_context_.has_value());
        QVERIFY(!dialog.accepted_activation_context_.has_value());
        QVERIFY(!dialog.response_.has_value());
        QCOMPARE(ResponseModel(&dialog)->rowCount(), 0);
        QCOMPARE(ArticlesFound(&dialog)->text(),
                 QStringLiteral("Articles found: 0"));
        QVERIFY(PartialStatus(&dialog)->isHidden());
        QVERIFY(EmptyStatus(&dialog)->isHidden());
        QVERIFY(FailureStatus(&dialog)->isHidden());
        QVERIFY(MixedResultStatus(&dialog)->isHidden());
        QVERIFY(PartialEmptyStatus(&dialog)->isHidden());
        QVERIFY(SearchButton(&dialog)->isEnabled());
        dialog.DetachController();
        dialog.DetachController();
        VerifySearchGroupBox(&dialog, search_group, composer);
        VerifyButtonRow(&dialog, button_row);
        VerifyResultCountProgressRow(&dialog, result_count_progress_row);
        VerifyProgressAlignmentContract(&dialog);
        VerifyResultCountMinimumHeight(&dialog);
        QCOMPARE(notifications, std::size_t{1});
    }
    QCOMPARE(notifications, std::size_t{1});
    QCOMPARE(replacement.Queries().size(), std::size_t{0});
}

}  // namespace goldendict::app

QTEST_MAIN(goldendict::app::FullTextSearchDialogTest)

#include "full_text_search_dialog_test.moc"
