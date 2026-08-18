// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_search_dialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QGroupBox>
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
    const goldendict::core::DictionaryService* service,
    const std::string& geometry, QWidget* parent)
    : QDialog(parent),
      completion_notifier_([]() { QApplication::beep(); }),
      controller_([this](std::uint64_t generation,
                         goldendict::core::FullTextResponse response) {
          FinishSearch(generation, std::move(response));
      }) {
    setObjectName(QStringLiteral("fullTextSearchDialog"));
    setWindowTitle(tr("Full-text search"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);
    setMinimumSize(430, 450);

    auto* search_group = new QGroupBox(tr("Search"), this);
    auto* search_group_layout = new QVBoxLayout(search_group);
    composer_ = new FullTextQueryComposer(preferences, search_group);
    composer_->setObjectName(QStringLiteral("fullTextQueryComposer"));
    search_group_layout->addWidget(composer_);
    query_text_ =
        composer_->findChild<QLineEdit*>(QStringLiteral("fullTextQueryText"));
    response_model_ = new FullTextResponseModel(this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(search_group);

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
    result_count_->setMinimumHeight(21);
    UpdateResultCount();

    progress_ = new QProgressBar(this);
    progress_->setObjectName(QStringLiteral("fullTextSearchProgress"));
    progress_->setAlignment(Qt::AlignCenter);
    progress_->setRange(0, 0);
    progress_->hide();

    auto* result_count_progress = new QHBoxLayout;
    result_count_progress->addWidget(result_count_);
    result_count_progress->addWidget(progress_);
    layout->addLayout(result_count_progress);

    partial_status_ = new QLabel(tr("Results may be incomplete."), this);
    partial_status_->setObjectName(
        QStringLiteral("fullTextPartialResponseStatus"));
    partial_status_->hide();
    layout->addWidget(partial_status_);

    empty_status_ = new QLabel(tr("No matches"), this);
    empty_status_->setObjectName(QStringLiteral("fullTextEmptyResponseStatus"));
    empty_status_->hide();
    layout->addWidget(empty_status_);

    failure_status_ = new QLabel(tr("Full-text search failed"), this);
    failure_status_->setObjectName(
        QStringLiteral("fullTextFailureResponseStatus"));
    failure_status_->hide();
    layout->addWidget(failure_status_);

    mixed_result_status_ =
        new QLabel(tr("Some dictionaries could not be searched"), this);
    mixed_result_status_->setObjectName(
        QStringLiteral("fullTextMixedResultResponseStatus"));
    mixed_result_status_->hide();
    layout->addWidget(mixed_result_status_);

    partial_empty_status_ =
        new QLabel(tr("No matches in searched dictionaries"), this);
    partial_empty_status_->setObjectName(
        QStringLiteral("fullTextPartialEmptyResponseStatus"));
    partial_empty_status_->hide();
    layout->addWidget(partial_empty_status_);

    error_count_status_ = new QLabel(this);
    error_count_status_->setObjectName(
        QStringLiteral("fullTextErrorCountResponseStatus"));
    error_count_status_->hide();
    layout->addWidget(error_count_status_);

    search_button_ = new QPushButton(tr("Search"), this);
    search_button_->setObjectName(QStringLiteral("fullTextSearchButton"));
    search_button_->setDefault(true);
    search_button_->setAutoDefault(false);
    cancel_button_ = new QPushButton(tr("Cancel"), this);
    cancel_button_->setObjectName(QStringLiteral("fullTextCancelButton"));
    help_button_ = new QPushButton(tr("Help"), this);
    help_button_->setObjectName(QStringLiteral("fullTextHelpButton"));
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(search_button_);
    buttons->addStretch();
    buttons->addWidget(cancel_button_);
    buttons->addStretch();
    buttons->addWidget(help_button_);
    buttons->addStretch();
    layout->addLayout(buttons);

    QWidget::setTabOrder(query_text_, results_);
    QWidget::setTabOrder(results_,
                         composer_->findChild<QWidget*>(
                             QStringLiteral("fullTextUseMaximumWordDistance")));
    QWidget::setTabOrder(composer_->findChild<QWidget*>(
                             QStringLiteral("fullTextUseMaximumWordDistance")),
                         composer_->findChild<QWidget*>(
                             QStringLiteral("fullTextMaximumWordDistance")));
    QWidget::setTabOrder(
        composer_->findChild<QWidget*>(
            QStringLiteral("fullTextMaximumWordDistance")),
        composer_->findChild<QWidget*>(QStringLiteral("fullTextQueryMode")));
    QWidget::setTabOrder(
        composer_->findChild<QWidget*>(QStringLiteral("fullTextQueryMode")),
        composer_->findChild<QWidget*>(
            QStringLiteral("fullTextUseMaximumArticles")));
    QWidget::setTabOrder(composer_->findChild<QWidget*>(
                             QStringLiteral("fullTextUseMaximumArticles")),
                         composer_->findChild<QWidget*>(QStringLiteral(
                             "fullTextMaximumArticlesPerDictionary")));
    QWidget::setTabOrder(
        composer_->findChild<QWidget*>(
            QStringLiteral("fullTextMaximumArticlesPerDictionary")),
        composer_->findChild<QWidget*>(QStringLiteral("fullTextMatchCase")));
    QWidget::setTabOrder(
        composer_->findChild<QWidget*>(QStringLiteral("fullTextMatchCase")),
        search_button_);
    QWidget::setTabOrder(search_button_, cancel_button_);

    help_action_ = new QAction(this);
    help_action_->setObjectName(QStringLiteral("fullTextHelpAction"));
    help_action_->setShortcut(QKeySequence(Qt::Key_F1));
    help_action_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(help_action_);

    connect(search_button_, &QPushButton::clicked, this,
            [this]() { SubmitSearch(); });
    connect(cancel_button_, &QPushButton::clicked, this,
            [this]() { CancelSearch(); });
    connect(help_button_, &QPushButton::clicked, this,
            &FullTextSearchDialog::HelpRequested);
    connect(help_action_, &QAction::triggered, this,
            &FullTextSearchDialog::HelpRequested);
    connect(results_, &QListView::clicked, this,
            [this](const QModelIndex& index) { ActivateResult(index); });
    controller_.SetService(service);
    resize(492, 593);
    if (!geometry.empty()) {
        restoreGeometry(QByteArray(geometry.data(),
                                   static_cast<qsizetype>(geometry.size())));
    }
}

FullTextSearchDialog::~FullTextSearchDialog() {
    DetachController();
}

void FullTextSearchDialog::SetService(
    const goldendict::core::DictionaryService* service) {
    active_generation_.reset();
    pending_activation_scope_.reset();
    pending_activation_context_.reset();
    RestoreIdleState();
    controller_.SetService(service);
}

void FullTextSearchDialog::DetachController() {
    active_generation_.reset();
    pending_activation_scope_.reset();
    pending_activation_context_.reset();
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

void FullTextSearchDialog::closeEvent(QCloseEvent* event) {
    if (!active_generation_.has_value() && !geometry_captured_) {
        const QByteArray geometry = saveGeometry();
        geometry_captured_ = true;
        emit GeometryCaptured(std::string(
            geometry.constData(), static_cast<std::size_t>(geometry.size())));
    }
    QDialog::closeEvent(event);
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
    UpdateMixedResultStatus();
    UpdatePartialEmptyStatus();
    UpdateErrorCountStatus();
    accepted_activation_scope_.reset();
    accepted_activation_context_.reset();
    pending_activation_scope_ =
        ActivationScope{query.dictionary_filter_active, query.dictionary_ids};
    pending_activation_context_ =
        ActivationContext{query.text, query.ignore_diacritics};
    active_generation_ = ++generation_;
    progress_->show();
    search_button_->setEnabled(false);
    cancel_button_->setEnabled(true);
    controller_.Submit(std::move(query), *active_generation_);
}

void FullTextSearchDialog::CancelSearch() {
    if (!active_generation_.has_value()) {
        close();
        return;
    }
    active_generation_.reset();
    pending_activation_scope_.reset();
    pending_activation_context_.reset();
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
    accepted_activation_context_ = std::move(pending_activation_context_);
    pending_activation_context_.reset();
    response_ = std::move(response);
    ResetResults(*response_);
    UpdateResultCount();
    UpdatePartialStatus();
    UpdateEmptyStatus();
    UpdateFailureStatus();
    UpdateMixedResultStatus();
    UpdatePartialEmptyStatus();
    UpdateErrorCountStatus();
    RestoreIdleState();
    completion_notifier_();
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
    if (result != nullptr && accepted_activation_scope_.has_value() &&
        accepted_activation_context_.has_value()) {
        emit ResultActivationRequested(FullTextResultActivationIntent{
            *result, accepted_activation_scope_->dictionary_filter_active,
            accepted_activation_scope_->dictionary_ids,
            accepted_activation_context_->query_text,
            accepted_activation_context_->ignore_diacritics});
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

void FullTextSearchDialog::UpdateMixedResultStatus() {
    mixed_result_status_->setVisible(response_.has_value() &&
                                     !response_->results.empty() &&
                                     !response_->errors.empty());
}

void FullTextSearchDialog::UpdatePartialEmptyStatus() {
    partial_empty_status_->setVisible(response_.has_value() &&
                                      response_->results.empty() &&
                                      response_->partial);
}

void FullTextSearchDialog::UpdateErrorCountStatus() {
    const std::size_t error_count =
        response_.has_value() ? response_->errors.size() : 0U;
    error_count_status_->setText(
        error_count == 0U ? QString() : tr("Errors: %1").arg(error_count));
    error_count_status_->setVisible(error_count != 0U);
}

void FullTextSearchDialog::RestoreIdleState() {
    progress_->hide();
    search_button_->setEnabled(true);
    cancel_button_->setEnabled(true);
}

}  // namespace goldendict::app
