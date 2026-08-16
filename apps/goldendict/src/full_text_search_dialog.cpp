// SPDX-License-Identifier: GPL-3.0-or-later

#include "full_text_search_dialog.h"

#include <QLineEdit>
#include <QVBoxLayout>

#include <utility>

#include "full_text_query_composer.h"

namespace goldendict::app {

FullTextSearchDialog::FullTextSearchDialog(
    const goldendict::core::ApplicationPreferences& preferences,
    const goldendict::core::DictionaryService* service, QWidget* parent)
    : QDialog(parent),
      controller_([](std::uint64_t, goldendict::core::FullTextResponse) {}) {
    setObjectName(QStringLiteral("fullTextSearchDialog"));
    setWindowTitle(QStringLiteral("Full-text search"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);

    composer_ = new FullTextQueryComposer(preferences, this);
    composer_->setObjectName(QStringLiteral("fullTextQueryComposer"));
    query_text_ =
        composer_->findChild<QLineEdit*>(QStringLiteral("fullTextQueryText"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(composer_);
    controller_.SetService(service);
}

FullTextSearchDialog::~FullTextSearchDialog() {
    DetachController();
}

void FullTextSearchDialog::SetService(
    const goldendict::core::DictionaryService* service) {
    controller_.SetService(service);
}

void FullTextSearchDialog::DetachController() {
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

}  // namespace goldendict::app
