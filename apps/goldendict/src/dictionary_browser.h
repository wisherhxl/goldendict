// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_DICTIONARY_BROWSER_H_
#define GOLDENDICT_APPS_GOLDENDICT_DICTIONARY_BROWSER_H_

#include <functional>
#include <vector>

#include <QDialog>

#include "goldendict/core/dictionary_service.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QComboBox;

namespace goldendict::core {
class DesktopFacade;
}  // namespace goldendict::core

class DictionaryBrowser final : public QDialog {
    Q_OBJECT

   public:
    explicit DictionaryBrowser(QWidget* parent = nullptr);

    void SetFacade(goldendict::core::DesktopFacade* facade);
    void RunSmokeCheck(const QString& expected_dictionary,
                       const QString& prefix, const QString& expected_headword,
                       std::function<void(bool)> completion);

   signals:
    void HeadwordSelected(const QString& headword);

   private:
    void RefreshDictionaryInfo();
    void RefreshHeadwords();

    goldendict::core::DesktopFacade* facade_ = nullptr;
    std::vector<goldendict::core::DictionaryIdentity> catalog_;
    QComboBox* dictionaries_ = nullptr;
    QLabel* identifier_ = nullptr;
    QLabel* edition_ = nullptr;
    QLabel* source_ = nullptr;
    QLineEdit* prefix_ = nullptr;
    QListWidget* headwords_ = nullptr;
    QLabel* result_status_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_DICTIONARY_BROWSER_H_
