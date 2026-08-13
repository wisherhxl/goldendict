// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_SOURCE_DIRECTORIES_DIALOG_H_
#define GOLDENDICT_APPS_SOURCE_DIRECTORIES_DIALOG_H_

#include <QDialog>

#include <string>
#include <vector>

#include "goldendict/core/application.h"

class QListWidget;
class QPushButton;
class QTreeWidget;

class SourceDirectoriesDialog final : public QDialog {
    Q_OBJECT

   public:
    SourceDirectoriesDialog(
        const std::vector<std::string>& dictionary_paths,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&
            sound_directories,
        QWidget* parent = nullptr);

    std::vector<std::string> DictionaryPaths() const;
    std::vector<goldendict::core::SoundDirectoryConfiguration>
    SoundDirectories() const;

   private:
    QWidget* CreateDictionaryTab();
    QWidget* CreateSoundTab();
    void AddDictionaryPath(const QString& path);
    void AddSoundDirectory(const QString& path, const QString& name);
    void MoveDictionaryPath(int offset);
    void MoveSoundDirectory(int offset);
    void UpdateDictionaryButtons();
    void UpdateSoundButtons();

    QListWidget* dictionary_paths_ = nullptr;
    QTreeWidget* sound_directories_ = nullptr;
    QPushButton* add_dictionary_ = nullptr;
    QPushButton* remove_dictionary_ = nullptr;
    QPushButton* dictionary_up_ = nullptr;
    QPushButton* dictionary_down_ = nullptr;
    QPushButton* add_sound_ = nullptr;
    QPushButton* remove_sound_ = nullptr;
    QPushButton* sound_up_ = nullptr;
    QPushButton* sound_down_ = nullptr;
};

#endif  // GOLDENDICT_APPS_SOURCE_DIRECTORIES_DIALOG_H_
