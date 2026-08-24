// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_DICTIONARY_BROWSER_H_
#define GOLDENDICT_APPS_GOLDENDICT_DICTIONARY_BROWSER_H_

#include <functional>
#include <vector>

#include <QByteArray>
#include <QDialog>

#include "goldendict/core/dictionary_service.h"
#include "goldendict/core/headword_export.h"
#include "widgets_facade_binding.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QComboBox;
class QCheckBox;
class QPushButton;
class QPlainTextEdit;
class QProgressDialog;
class QTimer;

namespace goldendict::core {
class DesktopFacade;
}  // namespace goldendict::core

class DictionaryBrowser final : public QDialog {
    Q_OBJECT

   public:
    explicit DictionaryBrowser(QWidget* parent = nullptr);
    ~DictionaryBrowser() override;

    void SetFacade(goldendict::core::DesktopFacade* facade);
    void SetBindingRegistry(
        const goldendict::widgets::WidgetsFacadeBindingRegistry*
            registry) noexcept;

    bool UsesBindingRegistry(
        const goldendict::widgets::WidgetsFacadeBindingRegistry* registry)
        const noexcept {
        return registry_ == registry;
    }

    void QuiesceBindingConsumer(bool shutting_down) noexcept;

    void RunSmokeCheck(const QString& expected_dictionary,
                       const QString& prefix, const QString& expected_headword,
                       std::function<void(bool)> completion);
    void RunExportSmokeCheck(const QString& path, const QString& prefix,
                             const QByteArray& expected,
                             std::function<void(bool)> completion);
    bool StartLifecycleExportForTest(const QString& path);

   signals:
    void HeadwordSelected(const QString& headword);
    void HelpRequested();

   private:
    void RefreshDictionaryInfo();
    void RefreshHeadwords();
    void StartHeadwordExport(const QString& path,
                             std::function<void(bool)> completion = {});
    QString DefaultExportFileName() const;
    QString CurrentSourcePath() const;
    QString CurrentSourceDirectory() const;

    goldendict::core::DesktopFacade* facade_ = nullptr;
    const goldendict::widgets::WidgetsFacadeBindingRegistry* registry_ =
        nullptr;
    std::vector<goldendict::core::DictionaryIdentity> catalog_;
    QComboBox* dictionaries_ = nullptr;
    QLabel* identifier_ = nullptr;
    QLabel* edition_ = nullptr;
    QLabel* source_ = nullptr;
    QPlainTextEdit* description_ = nullptr;
    QLabel* source_language_ = nullptr;
    QLabel* target_language_ = nullptr;
    QLabel* article_count_ = nullptr;
    QLabel* headword_count_ = nullptr;
    QPushButton* copy_source_ = nullptr;
    QPushButton* open_source_directory_ = nullptr;
    QComboBox* filter_mode_ = nullptr;
    QCheckBox* match_case_ = nullptr;
    QLineEdit* prefix_ = nullptr;
    QListWidget* headwords_ = nullptr;
    QLabel* result_status_ = nullptr;
    QPushButton* export_headwords_ = nullptr;
    QPushButton* help_button_ = nullptr;
    QProgressDialog* export_progress_ = nullptr;
    QTimer* export_poll_timer_ = nullptr;
    std::unique_ptr<goldendict::core::HeadwordExportOperation>
        export_operation_;
    goldendict::widgets::WidgetsFacadeBindingRegistry::Lease export_binding_;
    bool binding_acquisition_enabled_ = true;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_DICTIONARY_BROWSER_H_
