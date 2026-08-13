// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_GROUP_EDITOR_H_
#define GOLDENDICT_APPS_GOLDENDICT_GROUP_EDITOR_H_

#include <cstddef>
#include <vector>

#include <QDialog>

#include "goldendict/core/application.h"

class QLineEdit;
class QListWidget;
class QPushButton;
class QTabBar;
class QTreeWidget;

class GroupEditor final : public QDialog {
    Q_OBJECT

   public:
    GroupEditor(
        std::vector<goldendict::core::DictionaryGroupConfiguration> groups,
        std::vector<goldendict::core::DictionaryIdentity> catalog,
        QWidget* parent = nullptr);

    std::vector<goldendict::core::DictionaryGroupConfiguration> Groups() const;
    bool RunSmokeEdits();

   private:
    void AddGroup();
    void RemoveGroup();
    void MoveGroup(int offset);
    void AddDictionaries();
    void RemoveDictionaries();
    void MoveDictionary(int offset);
    void ChooseIcon();
    void LoadCurrentGroup();
    void StoreCurrentGroup();
    void RefreshAvailable();
    void RefreshGroupTabs();
    void UpdateActions();
    std::uint32_t NextGroupId() const;
    QString DictionaryName(const std::string& id) const;

    std::vector<goldendict::core::DictionaryGroupConfiguration> groups_;
    std::vector<goldendict::core::DictionaryIdentity> catalog_;
    int current_group_ = -1;
    QTabBar* group_tabs_ = nullptr;
    QListWidget* available_ = nullptr;
    QTreeWidget* members_ = nullptr;
    QLineEdit* name_ = nullptr;
    QLineEdit* icon_ = nullptr;
    QLineEdit* favorites_folder_ = nullptr;
    QLineEdit* shortcut_ = nullptr;
    QPushButton* remove_group_ = nullptr;
    QPushButton* group_up_ = nullptr;
    QPushButton* group_down_ = nullptr;
    QPushButton* add_dictionary_ = nullptr;
    QPushButton* remove_dictionary_ = nullptr;
    QPushButton* dictionary_up_ = nullptr;
    QPushButton* dictionary_down_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_GROUP_EDITOR_H_
