// SPDX-License-Identifier: GPL-3.0-or-later

#include "preferences_dialog.h"

#include <utility>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
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
    const QString error = apply_callback_(candidate);
    if (!error.isEmpty()) {
        validation_error_->setText(error);
        validation_error_->show();
        return;
    }
    preferences_ = std::move(candidate);
    accept();
}
