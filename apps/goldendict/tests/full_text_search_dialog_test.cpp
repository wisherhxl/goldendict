// SPDX-License-Identifier: GPL-3.0-or-later

#include <QLineEdit>
#include <QtTest>

#include "full_text_search_dialog.h"

namespace goldendict::app {
namespace {

class CountingDictionaryService final
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
        const goldendict::core::FullTextQuery&,
        const goldendict::core::CancellationToken*) const override {
        ++full_text_calls;
        return {};
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

    mutable int full_text_calls = 0;
};

}  // namespace

class FullTextSearchDialogTest final : public QObject {
    Q_OBJECT

   private slots:
    void HostsComposerAndProjectedStateWithoutSubmitting();
    void ServiceReplacementAndDestructionRemainIdle();
};

void FullTextSearchDialogTest::
    HostsComposerAndProjectedStateWithoutSubmitting() {
    CountingDictionaryService service;
    goldendict::core::ApplicationPreferences preferences;
    FullTextSearchDialog dialog(preferences, &service);

    QCOMPARE(dialog.windowTitle(), QStringLiteral("Full-text search"));
    QVERIFY(!dialog.isModal());
    QVERIFY(!(dialog.windowFlags() & Qt::WindowContextHelpButtonHint));
    QVERIFY(dialog.testAttribute(Qt::WA_DeleteOnClose));
    auto* query =
        dialog.findChild<QLineEdit*>(QStringLiteral("fullTextQueryText"));
    QVERIFY(query != nullptr);
    dialog.InitializeQuery(QStringLiteral("selected query"));
    QCOMPARE(query->text(), QStringLiteral("selected query"));
    QCOMPARE(query->selectedText(), QStringLiteral("selected query"));

    goldendict::core::FullTextQuery projected;
    projected.text = "selected query";
    projected.dictionary_ids = {"second", "first"};
    projected.dictionary_filter_active = true;
    dialog.SetProjectedQuery(projected);
    QCOMPARE(dialog.ProjectedQuery().text, projected.text);
    QCOMPARE(dialog.ProjectedQuery().dictionary_ids, projected.dictionary_ids);
    QVERIFY(dialog.ProjectedQuery().dictionary_filter_active);
    QCOMPARE(service.full_text_calls, 0);
}

void FullTextSearchDialogTest::ServiceReplacementAndDestructionRemainIdle() {
    CountingDictionaryService first;
    CountingDictionaryService second;
    goldendict::core::ApplicationPreferences preferences;
    {
        FullTextSearchDialog dialog(preferences, &first);
        dialog.SetService(nullptr);
        dialog.SetService(&second);
        dialog.DetachController();
    }
    QCOMPARE(first.full_text_calls, 0);
    QCOMPARE(second.full_text_calls, 0);
}

}  // namespace goldendict::app

QTEST_MAIN(goldendict::app::FullTextSearchDialogTest)

#include "full_text_search_dialog_test.moc"
