// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APP_FULL_TEXT_SEARCH_DIALOG_H_
#define GOLDENDICT_APP_FULL_TEXT_SEARCH_DIALOG_H_

#include <cstdint>
#include <optional>

#include <QDialog>

#include "full_text_request_controller.h"
#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

class QLineEdit;
class QProgressBar;
class QPushButton;

namespace goldendict::app {

class FullTextQueryComposer;
class FullTextSearchDialogTest;

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
    friend class FullTextSearchDialogTest;

    void SubmitSearch();
    void CancelSearch();
    void FinishSearch(std::uint64_t generation,
                      goldendict::core::FullTextResponse response);
    void RestoreIdleState();

    FullTextQueryComposer* composer_ = nullptr;
    QLineEdit* query_text_ = nullptr;
    QPushButton* search_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
    QProgressBar* progress_ = nullptr;
    FullTextRequestController controller_;
    goldendict::core::FullTextQuery projected_query_;
    std::optional<goldendict::core::FullTextResponse> response_;
    std::optional<std::uint64_t> active_generation_;
    std::uint64_t generation_ = 0U;
};

}  // namespace goldendict::app

#endif  // GOLDENDICT_APP_FULL_TEXT_SEARCH_DIALOG_H_
