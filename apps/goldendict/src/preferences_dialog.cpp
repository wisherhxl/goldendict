// SPDX-License-Identifier: GPL-3.0-or-later

#include "preferences_dialog.h"

#include <utility>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(
    const goldendict::core::ApplicationPreferences& preferences,
    ApplyCallback apply_callback, QWidget* parent)
    : QDialog(parent),
      preferences_(preferences),
      apply_callback_(std::move(apply_callback)) {
    setObjectName(QStringLiteral("preferencesDialog"));
    setWindowTitle(QStringLiteral("Preferences"));
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("preferencesTabs"));
    auto* general_page = new QWidget(tabs);
    general_page->setObjectName(QStringLiteral("preferencesGeneralPage"));
    auto* general_layout = new QVBoxLayout(general_page);
    auto* tab_group = new QGroupBox(QStringLiteral("Tabs"), general_page);
    tab_group->setObjectName(QStringLiteral("preferencesTabGroup"));
    auto* tab_layout = new QVBoxLayout(tab_group);
    open_in_background_ = new QCheckBox(
        QStringLiteral("Open new tabs in the background"), tab_group);
    open_in_background_->setObjectName(
        QStringLiteral("newTabsOpenInBackground"));
    open_in_background_->setChecked(preferences.open_new_tabs_in_background);
    open_after_current_ = new QCheckBox(
        QStringLiteral("Open new tabs after the current one"), tab_group);
    open_after_current_->setObjectName(
        QStringLiteral("newTabsOpenAfterCurrentOne"));
    open_after_current_->setChecked(preferences.open_new_tabs_after_current);
    tab_layout->addWidget(open_in_background_);
    tab_layout->addWidget(open_after_current_);
    general_layout->addWidget(tab_group);

    auto* history_group =
        new QGroupBox(QStringLiteral("History"), general_page);
    history_group->setObjectName(QStringLiteral("preferencesHistoryGroup"));
    auto* history_layout = new QVBoxLayout(history_group);
    store_history_ =
        new QCheckBox(QStringLiteral("Store &history"), history_group);
    store_history_->setObjectName(QStringLiteral("storeHistory"));
    store_history_->setToolTip(QStringLiteral(
        "Turn this option on to store history of the translated words"));
    store_history_->setChecked(preferences.store_history);
    history_layout->addWidget(store_history_);
    auto* maximum_layout = new QHBoxLayout;
    auto* maximum_label =
        new QLabel(QStringLiteral("Maximum history size:"), history_group);
    maximum_label->setObjectName(QStringLiteral("historySizeLabel"));
    maximum_label->setToolTip(QStringLiteral(
        "Specify the maximum number of entries to keep in history."));
    maximum_history_entries_ = new QSpinBox(history_group);
    maximum_history_entries_->setObjectName(
        QStringLiteral("historyMaxSizeField"));
    maximum_history_entries_->setAccelerated(true);
    maximum_history_entries_->setRange(0, 99999);
    maximum_history_entries_->setValue(
        static_cast<int>(preferences.maximum_history_entries));
    maximum_label->setBuddy(maximum_history_entries_);
    maximum_layout->addWidget(maximum_label);
    maximum_layout->addWidget(maximum_history_entries_);
    maximum_layout->addStretch();
    history_layout->addLayout(maximum_layout);
    general_layout->addWidget(history_group);

    auto* favorites_group =
        new QGroupBox(QStringLiteral("Favorites"), general_page);
    favorites_group->setObjectName(QStringLiteral("favoritesBox"));
    auto* favorites_layout = new QVBoxLayout(favorites_group);
    confirm_favorites_deletion_ = new QCheckBox(
        QStringLiteral("Confirmation for items deletion"), favorites_group);
    confirm_favorites_deletion_->setObjectName(
        QStringLiteral("confirmFavoritesDeletion"));
    confirm_favorites_deletion_->setToolTip(QStringLiteral(
        "Turn this option on to confirm every operation of items deletion"));
    confirm_favorites_deletion_->setChecked(
        preferences.confirm_favorites_deletion);
    favorites_layout->addWidget(confirm_favorites_deletion_);
    general_layout->addWidget(favorites_group);

    auto* articles_group =
        new QGroupBox(QStringLiteral("Articles"), general_page);
    articles_group->setObjectName(QStringLiteral("preferencesArticlesGroup"));
    auto* articles_layout = new QHBoxLayout(articles_group);
    collapse_large_articles_ = new QCheckBox(
        QStringLiteral("Collapse articles more than"), articles_group);
    collapse_large_articles_->setObjectName(
        QStringLiteral("collapseBigArticles"));
    collapse_large_articles_->setToolTip(QStringLiteral(
        "Select this option to automatic collapse big articles"));
    collapse_large_articles_->setChecked(preferences.collapse_large_articles);
    article_size_limit_ = new QSpinBox(articles_group);
    article_size_limit_->setObjectName(QStringLiteral("articleSizeLimit"));
    article_size_limit_->setToolTip(
        QStringLiteral("Articles longer than this size will be collapsed"));
    article_size_limit_->setRange(1, 100000);
    article_size_limit_->setSingleStep(50);
    article_size_limit_->setValue(
        static_cast<int>(preferences.article_size_limit));
    article_size_limit_->setEnabled(preferences.collapse_large_articles);
    connect(collapse_large_articles_, &QCheckBox::toggled, article_size_limit_,
            &QSpinBox::setEnabled);
    auto* symbols = new QLabel(QStringLiteral("symbols"), articles_group);
    symbols->setObjectName(QStringLiteral("articleSizeLimitLabel"));
    articles_layout->addWidget(collapse_large_articles_);
    articles_layout->addWidget(article_size_limit_);
    articles_layout->addWidget(symbols);
    articles_layout->addStretch();
    general_layout->addWidget(articles_group);

    auto* input_phrase_group =
        new QGroupBox(QStringLiteral("Input phrase length"), general_page);
    input_phrase_group->setObjectName(
        QStringLiteral("preferencesInputPhraseLengthGroup"));
    auto* input_phrase_layout = new QHBoxLayout(input_phrase_group);
    limit_input_phrase_length_ = new QCheckBox(
        QStringLiteral("Ignore input phrases longer than"), input_phrase_group);
    limit_input_phrase_length_->setObjectName(
        QStringLiteral("limitInputPhraseLength"));
    limit_input_phrase_length_->setToolTip(QStringLiteral(
        "Turn this option on to ignore unreasonably long input text"));
    limit_input_phrase_length_->setChecked(
        preferences.limit_input_phrase_length);
    input_phrase_length_limit_ = new QSpinBox(input_phrase_group);
    input_phrase_length_limit_->setObjectName(
        QStringLiteral("inputPhraseLengthLimit"));
    input_phrase_length_limit_->setToolTip(
        QStringLiteral("Input phrases longer than this size will be ignored"));
    input_phrase_length_limit_->setRange(1, 1000000);
    input_phrase_length_limit_->setSingleStep(10);
    input_phrase_length_limit_->setValue(
        static_cast<int>(preferences.input_phrase_length_limit));
    input_phrase_length_limit_->setEnabled(
        preferences.limit_input_phrase_length);
    connect(limit_input_phrase_length_, &QCheckBox::toggled,
            input_phrase_length_limit_, &QSpinBox::setEnabled);
    auto* input_phrase_symbols =
        new QLabel(QStringLiteral("symbols"), input_phrase_group);
    input_phrase_symbols->setObjectName(
        QStringLiteral("inputPhraseLengthLimitLabel"));
    input_phrase_layout->addWidget(limit_input_phrase_length_);
    input_phrase_layout->addWidget(input_phrase_length_limit_);
    input_phrase_layout->addWidget(input_phrase_symbols);
    input_phrase_layout->addStretch();
    general_layout->addWidget(input_phrase_group);
    general_layout->addStretch();
    tabs->addTab(general_page, QStringLiteral("General"));
    layout->addWidget(tabs);

    validation_error_ = new QLabel(this);
    validation_error_->setObjectName(
        QStringLiteral("preferencesValidationError"));
    validation_error_->setWordWrap(true);
    validation_error_->hide();
    layout->addWidget(validation_error_);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName(QStringLiteral("preferencesButtonBox"));
    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked, this,
            &PreferencesDialog::Apply);
    connect(buttons, &QDialogButtonBox::rejected, this,
            &PreferencesDialog::reject);
    layout->addWidget(buttons);
}

void PreferencesDialog::Apply() {
    auto candidate = preferences_;
    candidate.open_new_tabs_in_background = open_in_background_->isChecked();
    candidate.open_new_tabs_after_current = open_after_current_->isChecked();
    candidate.store_history = store_history_->isChecked();
    candidate.maximum_history_entries =
        static_cast<std::uint32_t>(maximum_history_entries_->value());
    candidate.confirm_favorites_deletion =
        confirm_favorites_deletion_->isChecked();
    candidate.collapse_large_articles = collapse_large_articles_->isChecked();
    candidate.article_size_limit =
        static_cast<std::uint32_t>(article_size_limit_->value());
    candidate.limit_input_phrase_length =
        limit_input_phrase_length_->isChecked();
    candidate.input_phrase_length_limit =
        static_cast<std::uint32_t>(input_phrase_length_limit_->value());
    const QString error = apply_callback_(candidate);
    if (!error.isEmpty()) {
        validation_error_->setText(error);
        validation_error_->show();
        return;
    }
    preferences_ = std::move(candidate);
    accept();
}
