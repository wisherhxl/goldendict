// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APP_FULL_TEXT_SEARCH_DIALOG_H_
#define GOLDENDICT_APP_FULL_TEXT_SEARCH_DIALOG_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <QDialog>
#include <QModelIndex>

#include "full_text_request_controller.h"
#include "goldendict/core/application.h"
#include "goldendict/core/dictionary_service.h"

class QLineEdit;
class QLabel;
class QListView;
class QProgressBar;
class QPushButton;

namespace goldendict::app {

struct FullTextResultActivationIntent final {
    goldendict::core::FullTextResult result;
    bool dictionary_filter_active = false;
    std::vector<std::string> dictionary_ids;
};

class FullTextQueryComposer;
class FullTextResponseModel;
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

   signals:
    void ResultActivationRequested(FullTextResultActivationIntent intent);

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

   private:
    friend class FullTextSearchDialogTest;

    void SubmitSearch();
    void CancelSearch();
    void FinishSearch(std::uint64_t generation,
                      goldendict::core::FullTextResponse response);
    void ResetResults(goldendict::core::FullTextResponse response);
    void ActivateResult(const QModelIndex& index);
    void UpdateResultCount();
    void UpdatePartialStatus();
    void UpdateEmptyStatus();
    void UpdateFailureStatus();
    void UpdateMixedResultStatus();
    void UpdatePartialEmptyStatus();
    void UpdateErrorCountStatus();
    void RestoreIdleState();

    FullTextQueryComposer* composer_ = nullptr;
    QLineEdit* query_text_ = nullptr;
    QListView* results_ = nullptr;
    QLabel* result_count_ = nullptr;
    QLabel* partial_status_ = nullptr;
    QLabel* empty_status_ = nullptr;
    QLabel* failure_status_ = nullptr;
    QLabel* mixed_result_status_ = nullptr;
    QLabel* partial_empty_status_ = nullptr;
    QLabel* error_count_status_ = nullptr;
    QPushButton* search_button_ = nullptr;
    QPushButton* cancel_button_ = nullptr;
    QProgressBar* progress_ = nullptr;
    FullTextResponseModel* response_model_ = nullptr;
    FullTextRequestController controller_;
    goldendict::core::FullTextQuery projected_query_;
    std::optional<goldendict::core::FullTextResponse> response_;

    struct ActivationScope final {
        bool dictionary_filter_active = false;
        std::vector<std::string> dictionary_ids;
    };

    std::optional<ActivationScope> pending_activation_scope_;
    std::optional<ActivationScope> accepted_activation_scope_;
    std::optional<std::uint64_t> active_generation_;
    std::uint64_t generation_ = 0U;
};

}  // namespace goldendict::app

#endif  // GOLDENDICT_APP_FULL_TEXT_SEARCH_DIALOG_H_
