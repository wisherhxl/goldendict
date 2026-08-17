// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_search_dialog.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QProgressBar>
#include <QPushButton>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <utility>

#include "full_text_query_composer.h"
#include "full_text_response_model.h"

namespace goldendict::app {
namespace {

class FullTextResultDelegate final : public QStyledItemDelegate {
   public:
    using QStyledItemDelegate::QStyledItemDelegate;

   protected:
    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        const bool is_right_to_left = option->text.isRightToLeft();
        option->direction =
            is_right_to_left ? Qt::RightToLeft : Qt::LeftToRight;
        if (option->textElideMode != Qt::ElideNone) {
            option->textElideMode =
                is_right_to_left ? Qt::ElideLeft : Qt::ElideRight;
        }
    }
};

}  // namespace

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
    results_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_->setModel(response_model_);
    results_->setItemDelegate(new FullTextResultDelegate(results_));
    results_->installEventFilter(this);
    layout->addWidget(results_);

    result_count_ = new QLabel(this);
    result_count_->setObjectName(QStringLiteral("fullTextArticlesFoundLabel"));
    UpdateResultCount();
    layout->addWidget(result_count_);

    partial_status_ =
        new QLabel(QStringLiteral("Results may be incomplete."), this);
    partial_status_->setObjectName(
        QStringLiteral("fullTextPartialResponseStatus"));
    partial_status_->hide();
    layout->addWidget(partial_status_);

    empty_status_ = new QLabel(QStringLiteral("No matches"), this);
    empty_status_->setObjectName(QStringLiteral("fullTextEmptyResponseStatus"));
    empty_status_->hide();
    layout->addWidget(empty_status_);

    failure_status_ =
        new QLabel(QStringLiteral("Full-text search failed"), this);
    failure_status_->setObjectName(
        QStringLiteral("fullTextFailureResponseStatus"));
    failure_status_->hide();
    layout->addWidget(failure_status_);

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
    connect(results_, &QListView::clicked, this,
            [this](const QModelIndex& index) { ActivateResult(index); });
    controller_.SetService(service);
}

FullTextSearchDialog::~FullTextSearchDialog() {
    DetachController();
}

void FullTextSearchDialog::SetService(
    const goldendict::core::DictionaryService* service) {
    active_generation_.reset();
    pending_activation_scope_.reset();
    RestoreIdleState();
    controller_.SetService(service);
}

void FullTextSearchDialog::DetachController() {
    active_generation_.reset();
    pending_activation_scope_.reset();
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

bool FullTextSearchDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == results_ && event->type() == QEvent::KeyPress) {
        const auto* key_event = static_cast<QKeyEvent*>(event);
        if (key_event->key() == Qt::Key_Return ||
            key_event->key() == Qt::Key_Enter) {
            ActivateResult(results_->currentIndex());
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void FullTextSearchDialog::SubmitSearch() {
    auto query = composer_->Compose();
    query.dictionary_ids = projected_query_.dictionary_ids;
    query.dictionary_filter_active = projected_query_.dictionary_filter_active;
    response_.reset();
    ResetResults({});
    UpdateResultCount();
    UpdatePartialStatus();
    UpdateEmptyStatus();
    UpdateFailureStatus();
    accepted_activation_scope_.reset();
    pending_activation_scope_ =
        ActivationScope{query.dictionary_filter_active, query.dictionary_ids};
    active_generation_ = ++generation_;
    progress_->show();
    search_button_->setEnabled(false);
    cancel_button_->setEnabled(true);
    controller_.Submit(std::move(query), *active_generation_);
}

void FullTextSearchDialog::CancelSearch() {
    active_generation_.reset();
    pending_activation_scope_.reset();
    controller_.Cancel();
    RestoreIdleState();
}

void FullTextSearchDialog::FinishSearch(
    std::uint64_t generation, goldendict::core::FullTextResponse response) {
    if (!active_generation_.has_value() || generation != *active_generation_) {
        return;
    }
    active_generation_.reset();
    accepted_activation_scope_ = std::move(pending_activation_scope_);
    pending_activation_scope_.reset();
    response_ = std::move(response);
    ResetResults(*response_);
    UpdateResultCount();
    UpdatePartialStatus();
    UpdateEmptyStatus();
    UpdateFailureStatus();
    RestoreIdleState();
}

void FullTextSearchDialog::ResetResults(
    goldendict::core::FullTextResponse response) {
    response_model_->Reset(std::move(response));
    results_->selectionModel()->clear();
}

void FullTextSearchDialog::ActivateResult(const QModelIndex& index) {
    if (!index.isValid() || index != results_->currentIndex())
        return;
    const auto* result = response_model_->ResultAt(index);
    if (result != nullptr && accepted_activation_scope_.has_value()) {
        emit ResultActivationRequested(FullTextResultActivationIntent{
            *result, accepted_activation_scope_->dictionary_filter_active,
            accepted_activation_scope_->dictionary_ids});
    }
}

void FullTextSearchDialog::UpdateResultCount() {
    result_count_->setText(
        tr("Articles found: %1").arg(response_model_->rowCount()));
}

void FullTextSearchDialog::UpdatePartialStatus() {
    partial_status_->setVisible(response_.has_value() && response_->partial);
}

void FullTextSearchDialog::UpdateEmptyStatus() {
    empty_status_->setVisible(response_.has_value() &&
                              response_->results.empty() &&
                              !response_->partial && response_->errors.empty());
}

void FullTextSearchDialog::UpdateFailureStatus() {
    failure_status_->setVisible(
        response_.has_value() && response_->results.empty() &&
        !response_->partial && !response_->errors.empty());
}

void FullTextSearchDialog::RestoreIdleState() {
    progress_->hide();
    search_button_->setEnabled(true);
    cancel_button_->setEnabled(false);
}

}  // namespace goldendict::app
