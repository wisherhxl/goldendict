// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_query_composer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVariant>

#include <cstdint>
#include <optional>
#include <string>

namespace goldendict::app {
namespace {

constexpr std::size_t kApplicationFullTextResultLimit = 100000U;

goldendict::core::FullTextQueryMode ToQueryMode(
    goldendict::core::FullTextSearchMode mode) {
    using goldendict::core::FullTextQueryMode;
    using goldendict::core::FullTextSearchMode;

    switch (mode) {
        case FullTextSearchMode::kWholeWords:
            return FullTextQueryMode::kWholeWords;
        case FullTextSearchMode::kPlainText:
            return FullTextQueryMode::kPlainText;
        case FullTextSearchMode::kWildcard:
            return FullTextQueryMode::kWildcard;
        case FullTextSearchMode::kRegularExpression:
            return FullTextQueryMode::kRegularExpression;
    }
    return FullTextQueryMode::kWholeWords;
}

bool UsesWordControls(goldendict::core::FullTextSearchMode mode) {
    return mode == goldendict::core::FullTextSearchMode::kWholeWords ||
           mode == goldendict::core::FullTextSearchMode::kPlainText;
}

std::string ToUtf8(const QString& text) {
    const QByteArray utf8 = text.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

}  // namespace

FullTextQueryComposer::FullTextQueryComposer(
    const goldendict::core::ApplicationPreferences& preferences,
    QWidget* parent)
    : QWidget(parent) {
    query_text_ = new QLineEdit(this);
    query_text_->setObjectName(QStringLiteral("fullTextQueryText"));

    mode_ = new QComboBox(this);
    mode_->setObjectName(QStringLiteral("fullTextQueryMode"));
    mode_->addItem(tr("Whole words"),
                   QVariant::fromValue(static_cast<int>(
                       goldendict::core::FullTextSearchMode::kWholeWords)));
    mode_->addItem(tr("Plain text"),
                   QVariant::fromValue(static_cast<int>(
                       goldendict::core::FullTextSearchMode::kPlainText)));
    mode_->addItem(tr("Wildcard"),
                   QVariant::fromValue(static_cast<int>(
                       goldendict::core::FullTextSearchMode::kWildcard)));
    mode_->addItem(
        tr("Regular expression"),
        QVariant::fromValue(static_cast<int>(
            goldendict::core::FullTextSearchMode::kRegularExpression)));

    const int persisted_mode =
        static_cast<int>(preferences.full_text_search_mode);
    mode_->setCurrentIndex(mode_->findData(persisted_mode));

    match_case_ = new QCheckBox(tr("Match case"), this);
    match_case_->setObjectName(QStringLiteral("fullTextMatchCase"));
    match_case_->setChecked(preferences.full_text_match_case);

    ignore_diacritics_ = new QCheckBox(tr("Ignore diacritics"), this);
    ignore_diacritics_->setObjectName(
        QStringLiteral("fullTextIgnoreDiacritics"));
    ignore_diacritics_->setChecked(preferences.full_text_ignore_diacritics);

    ignore_word_order_ = new QCheckBox(tr("Ignore words order"), this);
    ignore_word_order_->setObjectName(
        QStringLiteral("fullTextIgnoreWordOrder"));
    ignore_word_order_->setChecked(preferences.full_text_ignore_word_order);

    use_maximum_word_distance_ =
        new QCheckBox(tr("Maximum word distance"), this);
    use_maximum_word_distance_->setObjectName(
        QStringLiteral("fullTextUseMaximumWordDistance"));
    use_maximum_word_distance_->setChecked(
        preferences.full_text_use_maximum_word_distance);

    maximum_word_distance_ = new QSpinBox(this);
    maximum_word_distance_->setObjectName(
        QStringLiteral("fullTextMaximumWordDistance"));
    maximum_word_distance_->setRange(
        0, static_cast<int>(goldendict::core::kMaximumFullTextWordDistance));
    maximum_word_distance_->setValue(
        static_cast<int>(preferences.full_text_maximum_word_distance));

    use_maximum_articles_ =
        new QCheckBox(tr("Maximum articles per dictionary"), this);
    use_maximum_articles_->setObjectName(
        QStringLiteral("fullTextUseMaximumArticles"));
    use_maximum_articles_->setChecked(
        preferences.full_text_use_maximum_articles);

    maximum_articles_per_dictionary_ = new QSpinBox(this);
    maximum_articles_per_dictionary_->setObjectName(
        QStringLiteral("fullTextMaximumArticlesPerDictionary"));
    maximum_articles_per_dictionary_->setRange(1, 100000);
    maximum_articles_per_dictionary_->setValue(static_cast<int>(
        preferences.full_text_maximum_articles_per_dictionary));

    auto* word_distance_layout = new QHBoxLayout;
    word_distance_layout->addWidget(use_maximum_word_distance_);
    word_distance_layout->addWidget(maximum_word_distance_);

    auto* article_limit_layout = new QHBoxLayout;
    article_limit_layout->addWidget(use_maximum_articles_);
    article_limit_layout->addWidget(maximum_articles_per_dictionary_);

    auto* form = new QFormLayout;
    form->addRow(query_text_);
    form->addRow(tr("Mode:"), mode_);
    form->addRow(word_distance_layout);
    form->addRow(article_limit_layout);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(match_case_);
    layout->addWidget(ignore_diacritics_);
    layout->addWidget(ignore_word_order_);

    connect(mode_, &QComboBox::currentIndexChanged, this,
            [this]() { UpdateModeDependentControls(); });
    connect(use_maximum_word_distance_, &QCheckBox::toggled, this,
            [this]() { UpdateModeDependentControls(); });
    connect(use_maximum_articles_, &QCheckBox::toggled, this,
            [this](bool checked) {
                maximum_articles_per_dictionary_->setEnabled(checked);
            });

    UpdateModeDependentControls();
    maximum_articles_per_dictionary_->setEnabled(
        use_maximum_articles_->isChecked());
}

goldendict::core::FullTextQuery FullTextQueryComposer::Compose() const {
    const auto persisted_mode =
        static_cast<goldendict::core::FullTextSearchMode>(
            mode_->currentData().toInt());
    const bool uses_word_controls = UsesWordControls(persisted_mode);

    goldendict::core::FullTextQuery query;
    query.text = ToUtf8(query_text_->text());
    query.mode = ToQueryMode(persisted_mode);
    query.match_case = match_case_->isChecked();
    query.ignore_diacritics = ignore_diacritics_->isChecked();
    query.ignore_word_order =
        uses_word_controls && ignore_word_order_->isChecked();
    query.maximum_word_distance =
        uses_word_controls && use_maximum_word_distance_->isChecked()
            ? std::optional<std::uint32_t>(
                  static_cast<std::uint32_t>(maximum_word_distance_->value()))
            : std::nullopt;
    query.result_limit = kApplicationFullTextResultLimit;
    query.maximum_articles_per_dictionary =
        use_maximum_articles_->isChecked()
            ? std::optional<std::size_t>(static_cast<std::size_t>(
                  maximum_articles_per_dictionary_->value()))
            : std::nullopt;
    return query;
}

void FullTextQueryComposer::UpdateModeDependentControls() {
    const auto persisted_mode =
        static_cast<goldendict::core::FullTextSearchMode>(
            mode_->currentData().toInt());
    const bool enabled = UsesWordControls(persisted_mode);
    ignore_word_order_->setEnabled(enabled);
    use_maximum_word_distance_->setEnabled(enabled);
    maximum_word_distance_->setEnabled(enabled &&
                                       use_maximum_word_distance_->isChecked());
}

}  // namespace goldendict::app
