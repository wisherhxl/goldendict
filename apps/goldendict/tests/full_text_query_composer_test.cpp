// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtTest>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
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

void VerifyModeSelector(FullTextQueryComposer& composer) {
    using goldendict::core::FullTextSearchMode;

    const auto selectors =
        composer.findChildren<QComboBox*>(QStringLiteral("fullTextQueryMode"));
    QCOMPARE(selectors.size(), 1);
    const auto* selector = selectors.constFirst();
    QCOMPARE(selector->count(), 4);

    const std::array<QString, 4> expected_texts = {
        QStringLiteral("Whole words"), QStringLiteral("Plain text"),
        QStringLiteral("Wildcards"), QStringLiteral("RegExp")};
    const std::array<FullTextSearchMode, 4> expected_modes = {
        FullTextSearchMode::kWholeWords, FullTextSearchMode::kPlainText,
        FullTextSearchMode::kWildcard, FullTextSearchMode::kRegularExpression};
    for (int index = 0; index < selector->count(); ++index) {
        QCOMPARE(selector->itemText(index), expected_texts.at(index));
        QCOMPARE(selector->itemData(index).toInt(),
                 static_cast<int>(expected_modes.at(index)));
    }
    QCOMPARE(selector->findText(QStringLiteral("Wildcards")), 2);
    QCOMPARE(
        selector->findData(static_cast<int>(FullTextSearchMode::kWildcard)), 2);
    QCOMPARE(selector->findText(QStringLiteral("RegExp")), 3);
    QCOMPARE(selector->findData(
                 static_cast<int>(FullTextSearchMode::kRegularExpression)),
             3);
}

void VerifyIgnoreOptionsRow(FullTextQueryComposer& composer,
                            const QCheckBox* expected_ignore_order,
                            const QCheckBox* expected_ignore_diacritics) {
    const auto ignore_order_controls = composer.findChildren<QCheckBox*>(
        QStringLiteral("fullTextIgnoreWordOrder"), Qt::FindDirectChildrenOnly);
    QCOMPARE(ignore_order_controls.size(), 1);
    QCOMPARE(ignore_order_controls.constFirst(), expected_ignore_order);
    QCOMPARE(expected_ignore_order->parentWidget(), &composer);
    QCOMPARE(expected_ignore_order->objectName(),
             QStringLiteral("fullTextIgnoreWordOrder"));
    QCOMPARE(expected_ignore_order->text(),
             QStringLiteral("Ignore words order"));

    const auto ignore_diacritics_controls = composer.findChildren<QCheckBox*>(
        QStringLiteral("fullTextIgnoreDiacritics"), Qt::FindDirectChildrenOnly);
    QCOMPARE(ignore_diacritics_controls.size(), 1);
    QCOMPARE(ignore_diacritics_controls.constFirst(),
             expected_ignore_diacritics);
    QCOMPARE(expected_ignore_diacritics->parentWidget(), &composer);
    QCOMPARE(expected_ignore_diacritics->objectName(),
             QStringLiteral("fullTextIgnoreDiacritics"));
    QCOMPARE(expected_ignore_diacritics->text(),
             QStringLiteral("Ignore diacritics"));

    QHBoxLayout* ignore_options_layout = nullptr;
    int matching_layouts = 0;
    for (auto* candidate : composer.findChildren<QHBoxLayout*>()) {
        if (candidate->indexOf(expected_ignore_order) >= 0 &&
            candidate->indexOf(expected_ignore_diacritics) >= 0) {
            ignore_options_layout = candidate;
            ++matching_layouts;
        }
    }
    QCOMPARE(matching_layouts, 1);
    QVERIFY(ignore_options_layout != nullptr);
    QCOMPARE(ignore_options_layout->count(), 2);
    QCOMPARE(ignore_options_layout->itemAt(0)->widget(), expected_ignore_order);
    QCOMPARE(ignore_options_layout->itemAt(1)->widget(),
             expected_ignore_diacritics);

    auto* top_level_layout = qobject_cast<QVBoxLayout*>(composer.layout());
    QVERIFY(top_level_layout != nullptr);
    QCOMPARE(top_level_layout->count(), 3);
    QCOMPARE(top_level_layout->itemAt(2)->layout(), ignore_options_layout);
}

void VerifyOptionsGrid(FullTextQueryComposer& composer) {
    auto* query = Control<QLineEdit>(composer, "fullTextQueryText");
    auto* use_distance =
        Control<QCheckBox>(composer, "fullTextUseMaximumWordDistance");
    auto* distance = Control<QSpinBox>(composer, "fullTextMaximumWordDistance");
    auto* use_articles =
        Control<QCheckBox>(composer, "fullTextUseMaximumArticles");
    auto* articles =
        Control<QSpinBox>(composer, "fullTextMaximumArticlesPerDictionary");
    auto* match_case = Control<QCheckBox>(composer, "fullTextMatchCase");
    auto* mode = Control<QComboBox>(composer, "fullTextQueryMode");

    QCOMPARE(use_distance->text(), QStringLiteral("Maximum word distance"));
    QCOMPARE(use_articles->text(),
             QStringLiteral("Maximum articles per dictionary"));
    QCOMPARE(match_case->text(), QStringLiteral("Match case"));
    QCOMPARE(distance->minimum(), 0);
    QCOMPARE(distance->maximum(), 1000);
    QCOMPARE(articles->minimum(), 1);
    QCOMPARE(articles->maximum(), 100000);

    const auto grids = composer.findChildren<QGridLayout*>();
    QCOMPARE(grids.size(), 1);
    auto* grid = grids.constFirst();
    QCOMPARE(grid->count(), 6);
    QCOMPARE(grid->itemAtPosition(0, 0)->widget(), use_distance);
    QCOMPARE(grid->itemAtPosition(0, 1)->widget(), distance);
    QCOMPARE(grid->itemAtPosition(1, 0)->widget(), use_articles);
    QCOMPARE(grid->itemAtPosition(1, 1)->widget(), articles);
    QCOMPARE(grid->itemAtPosition(1, 2)->widget(), match_case);

    auto* mode_layout = grid->itemAtPosition(0, 2)->layout();
    QVERIFY(mode_layout != nullptr);
    QCOMPARE(mode_layout->count(), 2);
    auto* mode_label = qobject_cast<QLabel*>(mode_layout->itemAt(0)->widget());
    QVERIFY(mode_label != nullptr);
    QCOMPARE(mode_label->text(), QStringLiteral("Mode:"));
    QCOMPARE(mode_layout->itemAt(1)->widget(), mode);

    int matching_mode_labels = 0;
    for (const auto* label : composer.findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Mode:")) {
            ++matching_mode_labels;
            QCOMPARE(label, mode_label);
        }
    }
    QCOMPARE(matching_mode_labels, 1);

    const std::array<QWidget*, 8> direct_widgets = {
        query,    use_distance, distance, use_articles,
        articles, match_case,   mode,     mode_label};
    for (auto* widget : direct_widgets) {
        QCOMPARE(widget->parentWidget(), &composer);
    }
    const std::array<std::pair<QWidget*, QString>, 7> named_widgets = {{
        {query, QStringLiteral("fullTextQueryText")},
        {use_distance, QStringLiteral("fullTextUseMaximumWordDistance")},
        {distance, QStringLiteral("fullTextMaximumWordDistance")},
        {use_articles, QStringLiteral("fullTextUseMaximumArticles")},
        {articles, QStringLiteral("fullTextMaximumArticlesPerDictionary")},
        {match_case, QStringLiteral("fullTextMatchCase")},
        {mode, QStringLiteral("fullTextQueryMode")},
    }};
    for (const auto& [widget, name] : named_widgets) {
        const auto matches =
            composer.findChildren<QWidget*>(name, Qt::FindDirectChildrenOnly);
        QCOMPARE(matches.size(), 1);
        QCOMPARE(matches.constFirst(), widget);
        QCOMPARE(widget->objectName(), name);
    }

    auto* top_level_layout = qobject_cast<QVBoxLayout*>(composer.layout());
    QVERIFY(top_level_layout != nullptr);
    QCOMPARE(top_level_layout->count(), 3);
    QCOMPARE(top_level_layout->itemAt(0)->widget(), query);
    QCOMPARE(top_level_layout->itemAt(1)->layout(), grid);
    QCOMPARE(grid->parent(), top_level_layout);
    QCOMPARE(mode_layout->parent(), grid);
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

    VerifyModeSelector(composer);
    const auto* mode_selector =
        Control<QComboBox>(composer, "fullTextQueryMode");
    QCOMPARE(mode_selector->currentIndex(),
             mode_selector->findData(persisted_mode));
    QCOMPARE(mode_selector->currentData().toInt(), persisted_mode);
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
    auto* ignore_diacritics =
        Control<QCheckBox>(composer, "fullTextIgnoreDiacritics");

    VerifyIgnoreOptionsRow(composer, ignore_order, ignore_diacritics);
    QVERIFY(ignore_order->isChecked());
    QVERIFY(ignore_diacritics->isChecked());
    QVERIFY(ignore_order->isEnabled());
    QVERIFY(ignore_diacritics->isEnabled());

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
    QCOMPARE(Control<QCheckBox>(composer, "fullTextIgnoreDiacritics"),
             ignore_diacritics);
    VerifyIgnoreOptionsRow(composer, ignore_order, ignore_diacritics);
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
    const auto query_fields =
        composer.findChildren<QLineEdit*>(QStringLiteral("fullTextQueryText"));
    QCOMPARE(query_fields.size(), 1);
    auto* query_field = query_fields.constFirst();
    QCOMPARE(query_field->objectName(), QStringLiteral("fullTextQueryText"));
    QCOMPARE(query_field->parentWidget(), &composer);
    query_field->setText(QString::fromUtf8("stable query Δ"));
    const auto mode_selectors =
        composer.findChildren<QComboBox*>(QStringLiteral("fullTextQueryMode"));
    QCOMPARE(mode_selectors.size(), 1);
    auto* mode_selector = mode_selectors.constFirst();
    VerifyOptionsGrid(composer);
    const auto ignore_order_controls = composer.findChildren<QCheckBox*>(
        QStringLiteral("fullTextIgnoreWordOrder"));
    QCOMPARE(ignore_order_controls.size(), 1);
    auto* ignore_order =
        Control<QCheckBox>(composer, "fullTextIgnoreWordOrder");
    auto* ignore_diacritics =
        Control<QCheckBox>(composer, "fullTextIgnoreDiacritics");
    QCOMPARE(ignore_order_controls.constFirst(), ignore_order);
    QCOMPARE(ignore_order->objectName(),
             QStringLiteral("fullTextIgnoreWordOrder"));
    QCOMPARE(ignore_order->parentWidget(), &composer);
    QCOMPARE(ignore_order->text(), QStringLiteral("Ignore words order"));
    QVERIFY(ignore_order->isChecked());
    QVERIFY(ignore_order->isEnabled());
    VerifyIgnoreOptionsRow(composer, ignore_order, ignore_diacritics);
    auto* use_distance =
        Control<QCheckBox>(composer, "fullTextUseMaximumWordDistance");
    auto* distance = Control<QSpinBox>(composer, "fullTextMaximumWordDistance");
    ignore_order->setChecked(false);
    use_distance->setChecked(false);
    QCOMPARE(Control<QLineEdit>(composer, "fullTextQueryText"), query_field);
    VerifyOptionsGrid(composer);
    QCOMPARE(query_field->text(), QString::fromUtf8("stable query Δ"));
    QVERIFY(!composer.Compose().ignore_word_order);
    QVERIFY(!composer.Compose().maximum_word_distance.has_value());
    ignore_order->setChecked(true);
    use_distance->setChecked(true);

    for (const auto mode :
         {goldendict::core::FullTextSearchMode::kWildcard,
          goldendict::core::FullTextSearchMode::kRegularExpression}) {
        SelectMode(composer, mode);
        VerifyModeSelector(composer);
        QCOMPARE(mode_selector->currentIndex(),
                 mode_selector->findData(static_cast<int>(mode)));
        QCOMPARE(mode_selector->currentData().toInt(), static_cast<int>(mode));
        QCOMPARE(Control<QComboBox>(composer, "fullTextQueryMode"),
                 mode_selector);
        QCOMPARE(Control<QLineEdit>(composer, "fullTextQueryText"),
                 query_field);
        VerifyOptionsGrid(composer);
        QCOMPARE(query_field->text(), QString::fromUtf8("stable query Δ"));
        QCOMPARE(composer.Compose().text, std::string("stable query Δ"));
        QCOMPARE(Control<QCheckBox>(composer, "fullTextIgnoreWordOrder"),
                 ignore_order);
        QCOMPARE(Control<QCheckBox>(composer, "fullTextIgnoreDiacritics"),
                 ignore_diacritics);
        VerifyIgnoreOptionsRow(composer, ignore_order, ignore_diacritics);
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
        VerifyModeSelector(composer);
        QCOMPARE(Control<QComboBox>(composer, "fullTextQueryMode"),
                 mode_selector);
        QCOMPARE(Control<QLineEdit>(composer, "fullTextQueryText"),
                 query_field);
        VerifyOptionsGrid(composer);
        QCOMPARE(query_field->text(), QString::fromUtf8("stable query Δ"));
        QCOMPARE(composer.Compose().text, std::string("stable query Δ"));
        QCOMPARE(Control<QCheckBox>(composer, "fullTextIgnoreWordOrder"),
                 ignore_order);
        QCOMPARE(Control<QCheckBox>(composer, "fullTextIgnoreDiacritics"),
                 ignore_diacritics);
        VerifyIgnoreOptionsRow(composer, ignore_order, ignore_diacritics);
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
    VerifyModeSelector(composer);
    const auto query_fields =
        composer.findChildren<QLineEdit*>(QStringLiteral("fullTextQueryText"));
    QCOMPARE(query_fields.size(), 1);
    auto* query_field = query_fields.constFirst();
    auto* mode_selector = Control<QComboBox>(composer, "fullTextQueryMode");
    auto* ignore_order =
        Control<QCheckBox>(composer, "fullTextIgnoreWordOrder");
    auto* ignore_diacritics =
        Control<QCheckBox>(composer, "fullTextIgnoreDiacritics");
    VerifyOptionsGrid(composer);
    query_field->setText(QString::fromUtf8("repeatable Δ"));
    SelectMode(composer,
               goldendict::core::FullTextSearchMode::kRegularExpression);
    QCOMPARE(mode_selector->currentIndex(),
             mode_selector->findData(static_cast<int>(
                 goldendict::core::FullTextSearchMode::kRegularExpression)));
    QCOMPARE(mode_selector->currentData().toInt(),
             static_cast<int>(
                 goldendict::core::FullTextSearchMode::kRegularExpression));

    const auto first = composer.Compose();
    const auto second = composer.Compose();
    VerifyIgnoreOptionsRow(composer, ignore_order, ignore_diacritics);
    VerifyModeSelector(composer);
    QCOMPARE(first.mode,
             goldendict::core::FullTextQueryMode::kRegularExpression);
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
    QCOMPARE(Control<QLineEdit>(composer, "fullTextQueryText"), query_field);
    VerifyOptionsGrid(composer);
    QCOMPARE(query_field->text(), QString::fromUtf8("repeatable Δ"));
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
