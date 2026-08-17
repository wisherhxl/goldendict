// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QtTest>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "full_text_dictionary_projection.h"
#include "full_text_query_composer.h"

namespace goldendict::app {
namespace {

template <typename Widget>
Widget* Control(FullTextQueryComposer& composer, const char* name) {
    auto* control = composer.findChild<Widget*>(QString::fromLatin1(name));
    Q_ASSERT(control != nullptr);
    return control;
}

void SelectMode(FullTextQueryComposer& composer,
                goldendict::core::FullTextSearchMode mode) {
    auto* selector = Control<QComboBox>(composer, "fullTextQueryMode");
    const int index = selector->findData(static_cast<int>(mode));
    QVERIFY(index >= 0);
    selector->setCurrentIndex(index);
}

goldendict::core::DictionaryIdentity Identity(const std::string& id,
                                              bool supports_full_text_search) {
    goldendict::core::DictionaryIdentity identity;
    identity.id = id;
    identity.supports_full_text_search = supports_full_text_search;
    return identity;
}

}  // namespace

class FullTextQueryComposerTest final : public QObject {
    Q_OBJECT

   private slots:
    void MapsAllModes_data();
    void MapsAllModes();
    void MapsTextBooleansAndFixedDefaults();
    void MapsWordDistanceBoundsAndUncheckedState();
    void MapsArticleLimitBoundsAndUncheckedState();
    void RetainsValuesAcrossModeTransitions();
    void RepeatedCompositionIsDeterministicAndNonMutating();
    void ProjectsAllDictionariesInSupportedCatalogOrder();
    void ProjectsConfiguredGroupOrderMutingAndResolution();
    void AppliesOnlyVisibleDictionaryBarParticipation();
    void RecomputesAndPreservesComposedQuery();
};

void FullTextQueryComposerTest::MapsAllModes_data() {
    QTest::addColumn<int>("persisted_mode");
    QTest::addColumn<int>("query_mode");

    using goldendict::core::FullTextQueryMode;
    using goldendict::core::FullTextSearchMode;
    QTest::newRow("whole-words")
        << static_cast<int>(FullTextSearchMode::kWholeWords)
        << static_cast<int>(FullTextQueryMode::kWholeWords);
    QTest::newRow("plain-text")
        << static_cast<int>(FullTextSearchMode::kPlainText)
        << static_cast<int>(FullTextQueryMode::kPlainText);
    QTest::newRow("wildcard") << static_cast<int>(FullTextSearchMode::kWildcard)
                              << static_cast<int>(FullTextQueryMode::kWildcard);
    QTest::newRow("regular-expression")
        << static_cast<int>(FullTextSearchMode::kRegularExpression)
        << static_cast<int>(FullTextQueryMode::kRegularExpression);
}

void FullTextQueryComposerTest::MapsAllModes() {
    QFETCH(int, persisted_mode);
    QFETCH(int, query_mode);

    goldendict::core::ApplicationPreferences preferences;
    preferences.full_text_search_mode =
        static_cast<goldendict::core::FullTextSearchMode>(persisted_mode);
    FullTextQueryComposer composer(preferences);

    QCOMPARE(static_cast<int>(composer.Compose().mode), query_mode);
}

void FullTextQueryComposerTest::MapsTextBooleansAndFixedDefaults() {
    goldendict::core::ApplicationPreferences preferences;
    preferences.full_text_match_case = true;
    preferences.full_text_ignore_diacritics = true;
    preferences.full_text_ignore_word_order = true;
    FullTextQueryComposer composer(preferences);
    auto* ignore_order =
        Control<QCheckBox>(composer, "fullTextIgnoreWordOrder");

    QCOMPARE(ignore_order->text(), QStringLiteral("Ignore words order"));

    const QString text = QString::fromUtf8("Straße 日本語 café 😀");
    Control<QLineEdit>(composer, "fullTextQueryText")->setText(text);
    const auto query = composer.Compose();

    const QByteArray expected = text.toUtf8();
    QCOMPARE(query.text,
             std::string(expected.constData(),
                         static_cast<std::size_t>(expected.size())));
    QVERIFY(query.match_case);
    QVERIFY(query.ignore_diacritics);
    QVERIFY(query.ignore_word_order);
    QCOMPARE(query.result_limit, 100000U);
    QVERIFY(query.dictionary_ids.empty());
    QVERIFY(!query.dictionary_filter_active);
    QCOMPARE(query.timeout, std::chrono::seconds(5));

    Control<QCheckBox>(composer, "fullTextMatchCase")->setChecked(false);
    Control<QCheckBox>(composer, "fullTextIgnoreDiacritics")->setChecked(false);
    ignore_order->setChecked(false);
    QCOMPARE(Control<QCheckBox>(composer, "fullTextIgnoreWordOrder"),
             ignore_order);
    QCOMPARE(ignore_order->text(), QStringLiteral("Ignore words order"));
    const auto cleared = composer.Compose();
    QVERIFY(!cleared.match_case);
    QVERIFY(!cleared.ignore_diacritics);
    QVERIFY(!cleared.ignore_word_order);
}

void FullTextQueryComposerTest::MapsWordDistanceBoundsAndUncheckedState() {
    goldendict::core::ApplicationPreferences preferences;
    preferences.full_text_use_maximum_word_distance = true;
    FullTextQueryComposer composer(preferences);
    auto* distance = Control<QSpinBox>(composer, "fullTextMaximumWordDistance");
    auto* enabled =
        Control<QCheckBox>(composer, "fullTextUseMaximumWordDistance");

    QCOMPARE(distance->minimum(), 0);
    QCOMPARE(distance->maximum(), 1000);
    distance->setValue(0);
    QVERIFY(composer.Compose().maximum_word_distance.has_value());
    QCOMPARE(*composer.Compose().maximum_word_distance, std::uint32_t{0});
    distance->setValue(1000);
    QCOMPARE(*composer.Compose().maximum_word_distance, std::uint32_t{1000});
    enabled->setChecked(false);
    QVERIFY(!composer.Compose().maximum_word_distance.has_value());
}

void FullTextQueryComposerTest::MapsArticleLimitBoundsAndUncheckedState() {
    goldendict::core::ApplicationPreferences preferences;
    preferences.full_text_use_maximum_articles = true;
    FullTextQueryComposer composer(preferences);
    auto* limit =
        Control<QSpinBox>(composer, "fullTextMaximumArticlesPerDictionary");
    auto* enabled = Control<QCheckBox>(composer, "fullTextUseMaximumArticles");

    QCOMPARE(limit->minimum(), 1);
    QCOMPARE(limit->maximum(), 100000);
    limit->setValue(1);
    QVERIFY(composer.Compose().maximum_articles_per_dictionary.has_value());
    QCOMPARE(*composer.Compose().maximum_articles_per_dictionary,
             std::size_t{1});
    QCOMPARE(composer.Compose().result_limit, 100000U);
    limit->setValue(100000);
    QCOMPARE(*composer.Compose().maximum_articles_per_dictionary,
             std::size_t{100000});
    QCOMPARE(composer.Compose().result_limit, 100000U);
    enabled->setChecked(false);
    QVERIFY(!composer.Compose().maximum_articles_per_dictionary.has_value());
    QCOMPARE(composer.Compose().result_limit, 100000U);
}

void FullTextQueryComposerTest::RetainsValuesAcrossModeTransitions() {
    goldendict::core::ApplicationPreferences preferences;
    preferences.full_text_search_mode =
        goldendict::core::FullTextSearchMode::kWholeWords;
    preferences.full_text_ignore_word_order = true;
    preferences.full_text_use_maximum_word_distance = true;
    preferences.full_text_maximum_word_distance = 37U;
    FullTextQueryComposer composer(preferences);
    const auto mode_selectors =
        composer.findChildren<QComboBox*>(QStringLiteral("fullTextQueryMode"));
    QCOMPARE(mode_selectors.size(), 1);
    auto* mode_selector = mode_selectors.constFirst();
    auto* form = composer.findChild<QFormLayout*>();
    QVERIFY(form != nullptr);
    auto* mode_label =
        qobject_cast<QLabel*>(form->labelForField(mode_selector));
    QVERIFY(mode_label != nullptr);
    QCOMPARE(mode_label->text(), QStringLiteral("Mode:"));
    int matching_mode_labels = 0;
    for (const auto* label : composer.findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Mode:")) {
            ++matching_mode_labels;
        }
    }
    QCOMPARE(matching_mode_labels, 1);
    const auto ignore_order_controls = composer.findChildren<QCheckBox*>(
        QStringLiteral("fullTextIgnoreWordOrder"));
    QCOMPARE(ignore_order_controls.size(), 1);
    auto* ignore_order =
        Control<QCheckBox>(composer, "fullTextIgnoreWordOrder");
    QCOMPARE(ignore_order_controls.constFirst(), ignore_order);
    QCOMPARE(ignore_order->objectName(),
             QStringLiteral("fullTextIgnoreWordOrder"));
    QCOMPARE(ignore_order->parentWidget(), &composer);
    QCOMPARE(ignore_order->text(), QStringLiteral("Ignore words order"));
    QVERIFY(ignore_order->isChecked());
    QVERIFY(ignore_order->isEnabled());
    auto* use_distance =
        Control<QCheckBox>(composer, "fullTextUseMaximumWordDistance");
    auto* distance = Control<QSpinBox>(composer, "fullTextMaximumWordDistance");

    for (const auto mode :
         {goldendict::core::FullTextSearchMode::kWildcard,
          goldendict::core::FullTextSearchMode::kRegularExpression}) {
        SelectMode(composer, mode);
        QCOMPARE(Control<QComboBox>(composer, "fullTextQueryMode"),
                 mode_selector);
        QCOMPARE(form->labelForField(mode_selector), mode_label);
        QCOMPARE(mode_label->text(), QStringLiteral("Mode:"));
        QCOMPARE(Control<QCheckBox>(composer, "fullTextIgnoreWordOrder"),
                 ignore_order);
        QCOMPARE(ignore_order->text(), QStringLiteral("Ignore words order"));
        QVERIFY(!ignore_order->isEnabled());
        QVERIFY(!use_distance->isEnabled());
        QVERIFY(!distance->isEnabled());
        QVERIFY(!composer.Compose().ignore_word_order);
        QVERIFY(!composer.Compose().maximum_word_distance.has_value());
        QVERIFY(ignore_order->isChecked());
        QVERIFY(use_distance->isChecked());
        QCOMPARE(distance->value(), 37);
    }

    for (const auto mode :
         {goldendict::core::FullTextSearchMode::kPlainText,
          goldendict::core::FullTextSearchMode::kWholeWords}) {
        SelectMode(composer, mode);
        QCOMPARE(Control<QComboBox>(composer, "fullTextQueryMode"),
                 mode_selector);
        QCOMPARE(form->labelForField(mode_selector), mode_label);
        QCOMPARE(mode_label->text(), QStringLiteral("Mode:"));
        QCOMPARE(Control<QCheckBox>(composer, "fullTextIgnoreWordOrder"),
                 ignore_order);
        QCOMPARE(ignore_order->text(), QStringLiteral("Ignore words order"));
        QVERIFY(ignore_order->isEnabled());
        QVERIFY(use_distance->isEnabled());
        QVERIFY(distance->isEnabled());
        QVERIFY(composer.Compose().ignore_word_order);
        QCOMPARE(*composer.Compose().maximum_word_distance, std::uint32_t{37});
    }
}

void FullTextQueryComposerTest::
    RepeatedCompositionIsDeterministicAndNonMutating() {
    goldendict::core::ApplicationPreferences preferences;
    preferences.full_text_search_mode =
        goldendict::core::FullTextSearchMode::kPlainText;
    preferences.full_text_match_case = true;
    preferences.full_text_ignore_diacritics = true;
    preferences.full_text_ignore_word_order = true;
    preferences.full_text_use_maximum_word_distance = true;
    preferences.full_text_maximum_word_distance = 9U;
    preferences.full_text_use_maximum_articles = true;
    preferences.full_text_maximum_articles_per_dictionary = 1234U;
    const auto original = preferences;
    FullTextQueryComposer composer(preferences);
    auto* mode_selector = Control<QComboBox>(composer, "fullTextQueryMode");
    auto* form = composer.findChild<QFormLayout*>();
    QVERIFY(form != nullptr);
    auto* mode_label =
        qobject_cast<QLabel*>(form->labelForField(mode_selector));
    QVERIFY(mode_label != nullptr);
    QCOMPARE(mode_label->text(), QStringLiteral("Mode:"));
    Control<QLineEdit>(composer, "fullTextQueryText")
        ->setText(QString::fromUtf8("repeatable Δ"));

    const auto first = composer.Compose();
    const auto second = composer.Compose();
    QCOMPARE(second.text, first.text);
    QCOMPARE(second.mode, first.mode);
    QCOMPARE(second.match_case, first.match_case);
    QCOMPARE(second.ignore_diacritics, first.ignore_diacritics);
    QCOMPARE(second.ignore_word_order, first.ignore_word_order);
    QCOMPARE(second.maximum_word_distance, first.maximum_word_distance);
    QCOMPARE(second.result_limit, first.result_limit);
    QCOMPARE(second.maximum_articles_per_dictionary,
             first.maximum_articles_per_dictionary);
    QCOMPARE(second.timeout, first.timeout);
    QCOMPARE(Control<QComboBox>(composer, "fullTextQueryMode"), mode_selector);
    QCOMPARE(form->labelForField(mode_selector), mode_label);
    QCOMPARE(mode_label->text(), QStringLiteral("Mode:"));
    QVERIFY(preferences == original);
}

void FullTextQueryComposerTest::
    ProjectsAllDictionariesInSupportedCatalogOrder() {
    const std::vector<goldendict::core::DictionaryIdentity> catalog = {
        Identity("third", true), Identity("unsupported", false),
        Identity("first", true)};
    goldendict::core::FullTextQuery query;
    query.dictionary_ids = {"stale"};

    const auto projected =
        ProjectFullTextDictionaries(query, catalog, nullptr, false, {});

    QCOMPARE(projected.dictionary_ids,
             (std::vector<std::string>{"third", "first"}));
    QVERIFY(projected.dictionary_filter_active);
}

void FullTextQueryComposerTest::
    ProjectsConfiguredGroupOrderMutingAndResolution() {
    const std::vector<goldendict::core::DictionaryIdentity> catalog = {
        Identity("first", true), Identity("second", true),
        Identity("unsupported", false)};
    const goldendict::core::DictionaryGroupConfiguration group{
        7U,
        "Group",
        "",
        {"second", "missing", "unsupported", "first"},
        {"second"}};

    const auto projected = ProjectFullTextDictionaries(
        goldendict::core::FullTextQuery{}, catalog, &group, false, {});

    QCOMPARE(projected.dictionary_ids, (std::vector<std::string>{"first"}));
    QVERIFY(projected.dictionary_filter_active);
}

void FullTextQueryComposerTest::AppliesOnlyVisibleDictionaryBarParticipation() {
    const std::vector<goldendict::core::DictionaryIdentity> catalog = {
        Identity("first", true), Identity("second", true)};
    const goldendict::core::DictionaryGroupConfiguration group{
        7U, "Group", "", {"second", "first"}};

    const auto visible = ProjectFullTextDictionaries(
        goldendict::core::FullTextQuery{}, catalog, &group, true, {"first"});
    QCOMPARE(visible.dictionary_ids, (std::vector<std::string>{"first"}));
    const auto empty = ProjectFullTextDictionaries(
        goldendict::core::FullTextQuery{}, catalog, &group, true, {});
    QVERIFY(empty.dictionary_ids.empty());
    QVERIFY(empty.dictionary_filter_active);
    const auto hidden = ProjectFullTextDictionaries(
        goldendict::core::FullTextQuery{}, catalog, &group, false, {});
    QCOMPARE(hidden.dictionary_ids,
             (std::vector<std::string>{"second", "first"}));
}

void FullTextQueryComposerTest::RecomputesAndPreservesComposedQuery() {
    goldendict::core::ApplicationPreferences preferences;
    preferences.full_text_search_mode =
        goldendict::core::FullTextSearchMode::kPlainText;
    preferences.full_text_match_case = true;
    preferences.full_text_ignore_diacritics = true;
    preferences.full_text_ignore_word_order = true;
    preferences.full_text_use_maximum_word_distance = true;
    preferences.full_text_maximum_word_distance = 42U;
    preferences.full_text_use_maximum_articles = true;
    preferences.full_text_maximum_articles_per_dictionary = 321U;
    const auto original_preferences = preferences;
    FullTextQueryComposer composer(preferences);
    Control<QLineEdit>(composer, "fullTextQueryText")
        ->setText(QString::fromUtf8("projection Δ"));
    const auto composed = composer.Compose();

    std::vector<goldendict::core::DictionaryIdentity> catalog = {
        Identity("first", true), Identity("second", false)};
    auto projected =
        ProjectFullTextDictionaries(composed, catalog, nullptr, false, {});
    QCOMPARE(projected.dictionary_ids, (std::vector<std::string>{"first"}));
    catalog = {Identity("second", true), Identity("first", false)};
    projected =
        ProjectFullTextDictionaries(composed, catalog, nullptr, false, {});
    QCOMPARE(projected.dictionary_ids, (std::vector<std::string>{"second"}));

    goldendict::core::DictionaryGroupConfiguration group{
        7U, "Group", "", {"second"}, {"second"}};
    projected = ProjectFullTextDictionaries(composed, catalog, &group, false,
                                            {"second"});
    QVERIFY(projected.dictionary_ids.empty());
    group.muted_dictionary_ids.clear();
    projected = ProjectFullTextDictionaries(composed, catalog, &group, false,
                                            {"second"});
    QCOMPARE(projected.dictionary_ids, (std::vector<std::string>{"second"}));

    QCOMPARE(projected.text, composed.text);
    QCOMPARE(projected.mode, composed.mode);
    QCOMPARE(projected.match_case, composed.match_case);
    QCOMPARE(projected.ignore_diacritics, composed.ignore_diacritics);
    QCOMPARE(projected.ignore_word_order, composed.ignore_word_order);
    QCOMPARE(projected.maximum_word_distance, composed.maximum_word_distance);
    QCOMPARE(projected.result_limit, composed.result_limit);
    QCOMPARE(projected.maximum_articles_per_dictionary,
             composed.maximum_articles_per_dictionary);
    QCOMPARE(projected.timeout, composed.timeout);
    QVERIFY(preferences == original_preferences);
}

}  // namespace goldendict::app

QTEST_MAIN(goldendict::app::FullTextQueryComposerTest)

#include "full_text_query_composer_test.moc"
