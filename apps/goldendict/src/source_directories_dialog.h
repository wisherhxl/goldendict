// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_SOURCE_DIRECTORIES_DIALOG_H_
#define GOLDENDICT_APPS_SOURCE_DIRECTORIES_DIALOG_H_

#include <QDialog>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "goldendict/core/application.h"

class QListWidget;
class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

class SourceDirectoriesDialog final : public QDialog {
    Q_OBJECT

   public:
    using ForvoCredentialMap = std::map<std::string, std::string>;
    using ApplyCallback = std::function<QString(
        const std::vector<std::string>&,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&,
        const std::vector<goldendict::core::MediaWikiSourceConfiguration>&,
        const std::vector<goldendict::core::WebsiteSourceConfiguration>&,
        const std::vector<goldendict::core::ForvoSourceConfiguration>&,
        const ForvoCredentialMap&,
        const std::vector<goldendict::core::DictServerSourceConfiguration>&,
        const std::vector<
            goldendict::core::ExternalProgramSourceConfiguration>&)>;

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
        const ForvoCredentialMap& forvo_credentials,
        const std::vector<goldendict::core::DictServerSourceConfiguration>&
            dict_server_sources,
        const std::vector<goldendict::core::ExternalProgramSourceConfiguration>&
            external_program_sources,
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
    ForvoCredentialMap ForvoCredentials() const;
    std::vector<goldendict::core::DictServerSourceConfiguration>
    DictServerSources() const;
    std::vector<goldendict::core::ExternalProgramSourceConfiguration>
    ExternalProgramSources();

#if defined(Q_OS_LINUX)
   signals:
    void HelpRequested();
#endif

   private:
    QWidget* CreateDictionaryTab();
    QWidget* CreateSoundTab();
    QWidget* CreateExternalProgramTab();
    QWidget* CreateOnlineTab(const QString& description,
                             const QStringList& headers, QTreeWidget** tree,
                             const QString& object_prefix);
    void AddMediaWiki();
    void AddWebsite();
    void AddForvo();
    void AddDictServer();
    void MoveOnline(QTreeWidget* tree, int offset);
    void UpdateOnlineButtons(QTreeWidget* tree);
    void AddExternalProgram(const QString& executable);
    void MoveExternalProgram(int offset);
    void UpdateExternalProgramSelection();
    void UpdateExternalProgramButtons();
    void StoreExternalProgramDetails();
    void AddExternalArgument(const QString& argument);
    void MoveExternalArgument(int offset);
    void UpdateExternalArgumentButtons();
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
    QLineEdit* forvo_credential_ = nullptr;
    QTreeWidget* dict_server_sources_ = nullptr;
    QTreeWidget* external_program_sources_ = nullptr;
    QComboBox* external_program_result_kind_ = nullptr;
    QLineEdit* external_program_executable_ = nullptr;
    QLineEdit* external_program_working_directory_ = nullptr;
    QListWidget* external_program_arguments_ = nullptr;
    QPushButton* external_program_add_ = nullptr;
    QPushButton* external_program_remove_ = nullptr;
    QPushButton* external_program_up_ = nullptr;
    QPushButton* external_program_down_ = nullptr;
    QPushButton* external_argument_add_ = nullptr;
    QPushButton* external_argument_remove_ = nullptr;
    QPushButton* external_argument_up_ = nullptr;
    QPushButton* external_argument_down_ = nullptr;
    bool loading_external_program_ = false;
    bool loading_forvo_credential_ = false;
    QTreeWidgetItem* external_program_current_item_ = nullptr;
    ForvoCredentialMap forvo_credentials_;
    QLabel* validation_error_ = nullptr;
    ApplyCallback apply_callback_;
};

#endif  // GOLDENDICT_APPS_SOURCE_DIRECTORIES_DIALOG_H_
