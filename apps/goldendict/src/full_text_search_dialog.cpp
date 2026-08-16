// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_search_dialog.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QListView>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

#include "full_text_query_composer.h"
#include "full_text_response_model.h"

namespace goldendict::app {

FullTextSearchDialog::FullTextSearchDialog(
    const goldendict::core::ApplicationPreferences& preferences,
    const goldendict::core::DictionaryService* service, QWidget* parent)
    : QDialog(parent),
      controller_([this](std::uint64_t generation,
                         goldendict::core::FullTextResponse response) {
          FinishSearch(generation, std::move(response));
      }) {
    setObjectName(QStringLiteral("fullTextSearchDialog"));
    setWindowTitle(QStringLiteral("Full-text search"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);

    composer_ = new FullTextQueryComposer(preferences, this);
    composer_->setObjectName(QStringLiteral("fullTextQueryComposer"));
    query_text_ =
        composer_->findChild<QLineEdit*>(QStringLiteral("fullTextQueryText"));
    response_model_ = new FullTextResponseModel(this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(composer_);

    results_ = new QListView(this);
    results_->setObjectName(QStringLiteral("fullTextSearchResults"));
    results_->setModel(response_model_);
    layout->addWidget(results_);

    progress_ = new QProgressBar(this);
    progress_->setObjectName(QStringLiteral("fullTextSearchProgress"));
    progress_->setRange(0, 0);
    progress_->hide();
    layout->addWidget(progress_);

    search_button_ = new QPushButton(tr("Search"), this);
    search_button_->setObjectName(QStringLiteral("fullTextSearchButton"));
    search_button_->setDefault(true);
    cancel_button_ = new QPushButton(tr("Cancel"), this);
    cancel_button_->setObjectName(QStringLiteral("fullTextCancelButton"));
    cancel_button_->setEnabled(false);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(search_button_);
    buttons->addWidget(cancel_button_);
    buttons->addStretch();
    layout->addLayout(buttons);

    connect(search_button_, &QPushButton::clicked, this,
            [this]() { SubmitSearch(); });
    connect(cancel_button_, &QPushButton::clicked, this,
            [this]() { CancelSearch(); });
    controller_.SetService(service);
}

FullTextSearchDialog::~FullTextSearchDialog() {
    DetachController();
}

void FullTextSearchDialog::SetService(
    const goldendict::core::DictionaryService* service) {
    active_generation_.reset();
    RestoreIdleState();
    controller_.SetService(service);
}

void FullTextSearchDialog::DetachController() {
    active_generation_.reset();
    RestoreIdleState();
    controller_.DetachConsumer();
}

void FullTextSearchDialog::InitializeQuery(const QString& text) {
    query_text_->setText(text);
    query_text_->setFocus();
    query_text_->selectAll();
}

void FullTextSearchDialog::SetProjectedQuery(
    goldendict::core::FullTextQuery query) {
    projected_query_ = std::move(query);
}

const goldendict::core::FullTextQuery& FullTextSearchDialog::ProjectedQuery()
    const noexcept {
    return projected_query_;
}

void FullTextSearchDialog::SubmitSearch() {
    auto query = composer_->Compose();
    query.dictionary_ids = projected_query_.dictionary_ids;
    query.dictionary_filter_active = projected_query_.dictionary_filter_active;
    response_.reset();
    response_model_->Reset({});
    active_generation_ = ++generation_;
    progress_->show();
    search_button_->setEnabled(false);
    cancel_button_->setEnabled(true);
    controller_.Submit(std::move(query), *active_generation_);
}

void FullTextSearchDialog::CancelSearch() {
    active_generation_.reset();
    controller_.Cancel();
    RestoreIdleState();
}

void FullTextSearchDialog::FinishSearch(
    std::uint64_t generation, goldendict::core::FullTextResponse response) {
    if (!active_generation_.has_value() || generation != *active_generation_) {
        return;
    }
    active_generation_.reset();
    response_ = std::move(response);
    response_model_->Reset(*response_);
    RestoreIdleState();
}

void FullTextSearchDialog::RestoreIdleState() {
    progress_->hide();
    search_button_->setEnabled(true);
    cancel_button_->setEnabled(false);
}

}  // namespace goldendict::app
