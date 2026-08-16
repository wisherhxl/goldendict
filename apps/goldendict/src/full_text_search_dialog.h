// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APP_FULL_TEXT_SEARCH_DIALOG_H_
#define GOLDENDICT_APP_FULL_TEXT_SEARCH_DIALOG_H_

#include <QDialog>

#include "full_text_request_controller.h"
#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

class QLineEdit;

namespace goldendict::app {

class FullTextQueryComposer;

class FullTextSearchDialog final : public QDialog {
    Q_OBJECT

   public:
    explicit FullTextSearchDialog(
        const goldendict::core::ApplicationPreferences& preferences,
        const goldendict::core::DictionaryService* service,
        QWidget* parent = nullptr);
    ~FullTextSearchDialog() override;

    void SetService(const goldendict::core::DictionaryService* service);
    void DetachController();
    void InitializeQuery(const QString& text);
    void SetProjectedQuery(goldendict::core::FullTextQuery query);
    const goldendict::core::FullTextQuery& ProjectedQuery() const noexcept;

   private:
    FullTextQueryComposer* composer_ = nullptr;
    QLineEdit* query_text_ = nullptr;
    FullTextRequestController controller_;
    goldendict::core::FullTextQuery projected_query_;
};

}  // namespace goldendict::app

#endif  // GOLDENDICT_APP_FULL_TEXT_SEARCH_DIALOG_H_
