// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_SOURCE_DIRECTORIES_DIALOG_H_
#define GOLDENDICT_APPS_SOURCE_DIRECTORIES_DIALOG_H_

#include <QDialog>

#include <functional>
#include <string>
#include <vector>

#include "goldendict/core/application.h"

class QListWidget;
class QLabel;
class QPushButton;
class QTreeWidget;

class SourceDirectoriesDialog final : public QDialog {
    Q_OBJECT

   public:
    using ApplyCallback = std::function<QString(
        const std::vector<std::string>&,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&,
        const std::vector<goldendict::core::MediaWikiSourceConfiguration>&,
        const std::vector<goldendict::core::WebsiteSourceConfiguration>&,
        const std::vector<goldendict::core::ForvoSourceConfiguration>&,
        const std::vector<goldendict::core::DictServerSourceConfiguration>&)>;

    SourceDirectoriesDialog(
        const std::vector<std::string>& dictionary_paths,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&
            sound_directories,
        QWidget* parent);
    SourceDirectoriesDialog(
        const std::vector<std::string>& dictionary_paths,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&
            sound_directories,
        const std::vector<goldendict::core::MediaWikiSourceConfiguration>&
            mediawiki_sources,
        const std::vector<goldendict::core::WebsiteSourceConfiguration>&
            website_sources,
        const std::vector<goldendict::core::ForvoSourceConfiguration>&
            forvo_sources,
        const std::vector<goldendict::core::DictServerSourceConfiguration>&
            dict_server_sources,
        ApplyCallback apply_callback, QWidget* parent = nullptr);

    std::vector<std::string> DictionaryPaths() const;
    std::vector<goldendict::core::SoundDirectoryConfiguration>
    SoundDirectories() const;
    std::vector<goldendict::core::MediaWikiSourceConfiguration>
    MediaWikiSources() const;
    std::vector<goldendict::core::WebsiteSourceConfiguration> WebsiteSources()
        const;
    std::vector<goldendict::core::ForvoSourceConfiguration> ForvoSources()
        const;
    std::vector<goldendict::core::DictServerSourceConfiguration>
    DictServerSources() const;

   private:
    QWidget* CreateDictionaryTab();
    QWidget* CreateSoundTab();
    QWidget* CreateOnlineTab(const QString& description,
                             const QStringList& headers, QTreeWidget** tree,
                             const QString& object_prefix);
    void AddMediaWiki();
    void AddWebsite();
    void AddForvo();
    void AddDictServer();
    void MoveOnline(QTreeWidget* tree, int offset);
    void UpdateOnlineButtons(QTreeWidget* tree);
    void Apply();
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
    QTreeWidget* mediawiki_sources_ = nullptr;
    QTreeWidget* website_sources_ = nullptr;
    QTreeWidget* forvo_sources_ = nullptr;
    QTreeWidget* dict_server_sources_ = nullptr;
    QLabel* validation_error_ = nullptr;
    ApplyCallback apply_callback_;
};

#endif  // GOLDENDICT_APPS_SOURCE_DIRECTORIES_DIALOG_H_
