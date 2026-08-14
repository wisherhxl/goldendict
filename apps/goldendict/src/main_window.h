// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
#define GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <QList>
#include <QMainWindow>
#include <QStringList>

#include "goldendict/core/application.h"
#include "goldendict/core/desktop_facade.h"

class ArticlePage;
class ArticleView;
class ArticleSchemeHandler;
enum class ArticleLinkDisposition;
class DictionaryBrowser;
class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QPoint;
class QComboBox;
class QEvent;
class QTabWidget;
class QTimer;
class QToolBar;
class QToolButton;
class QUrl;
class QWebEngineView;
class QPrinter;
class QShortcut;
class FavoritesTreeWidget;
class SuggestionWorker;
class SourceDirectoriesDialog;

namespace goldendict::core {
class DesktopFacade;
class LookupRequest;
}  // namespace goldendict::core

struct FavoriteViewItem {
    QString text;
    bool folder = false;
    bool expanded = false;
    QList<int> path;
    std::vector<FavoriteViewItem> children;
};

struct HistoryViewItem {
    QString word;
    std::uint32_t group_id = 0U;
};

class MainWindow final : public QMainWindow {
    Q_OBJECT

   public:
    using SourceApplyCallback = std::function<QString(
        const std::vector<std::string>&,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&,
        const std::vector<goldendict::core::MediaWikiSourceConfiguration>&,
        const std::vector<goldendict::core::WebsiteSourceConfiguration>&,
        const std::vector<goldendict::core::ForvoSourceConfiguration>&,
        const std::vector<goldendict::core::DictServerSourceConfiguration>&,
        const std::vector<
            goldendict::core::ExternalProgramSourceConfiguration>&)>;
    using HistoryExportCallback = std::function<QString(const QString&)>;
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void SetFacade(goldendict::core::DesktopFacade* facade);
    void SetPreferences(
        const goldendict::core::ApplicationPreferences& preferences);
    bool RestoreMainWindowGeometry(const std::string& geometry);
    std::string CaptureMainWindowGeometry() const;
    bool RestoreMainWindowState(const std::string& state);
    std::string CaptureMainWindowState() const;
    void SetHistoryWords(const QStringList& words);
    void SetHistoryItems(const std::vector<HistoryViewItem>& items);
    void SetFavoriteItems(const std::vector<FavoriteViewItem>& items,
                          const QList<int>& current_path = {});
    void SetDictionaryGroups(
        const std::vector<goldendict::core::DictionaryGroupConfiguration>&
            groups);
    void SetSourceDirectories(
        const std::vector<std::string>& dictionary_paths,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&
            sound_directories);
    void SetOnlineSources(
        const std::vector<goldendict::core::MediaWikiSourceConfiguration>&
            mediawiki_sources,
        const std::vector<goldendict::core::WebsiteSourceConfiguration>&
            website_sources,
        const std::vector<goldendict::core::ForvoSourceConfiguration>&
            forvo_sources,
        const std::vector<goldendict::core::DictServerSourceConfiguration>&
            dict_server_sources,
        const std::vector<goldendict::core::ExternalProgramSourceConfiguration>&
            external_program_sources,
        SourceApplyCallback apply_callback);
    void SetHistoryExportCallback(HistoryExportCallback callback);
    const std::vector<goldendict::core::DictionaryGroupConfiguration>&
    DictionaryGroups() const noexcept;
    void RunWebEngineSmokeCheck(std::function<void(bool)> completion);
    void RunWebEngineInteractionCheck(std::function<void(bool)> completion);
    void RunArticleContextMenuCheck(std::function<void(bool)> completion);
    void RunSystemPrintCheck(std::function<void(bool)> completion);
    void RunHistorySmokeCheck(std::function<void(bool)> completion);
    void RunHistoryManagementSmokeCheck(std::function<void(bool)> completion);
    void RunHistoryExportSmokeCheck(const QString& path,
                                    std::function<void(bool)> completion);
    void RunHistoryImportSmokeCheck(const QString& path,
                                    std::function<void(bool)> completion);
    void RunFavoritesSmokeCheck(std::function<void(bool)> completion);
    void RunFavoritesCrossFolderMoveSmokeCheck(
        std::function<void(bool)> completion);
    void RunFavoritesTransferSmokeCheck(const QString& path,
                                        std::function<void(bool)> completion);
    void RunDictionaryBrowserSmokeCheck(std::function<void(bool)> completion);
    void RunDictionaryBrowserExportSmokeCheck(
        const QString& path, std::function<void(bool)> completion);
    void RunDictionaryGroupsSmokeCheck(std::function<void(bool)> completion);
    void RunSourceDirectoriesSmokeCheck(std::function<void(bool)> completion);
    void RunArticleTabsSmokeCheck(std::function<void(bool)> completion);
    void RunSuggestionPaneSmokeCheck(std::function<void(bool)> completion);
    void RunDictionaryBarSmokeCheck(std::function<void(bool)> completion);
    void RunViewMenuSmokeCheck(std::function<void(bool)> completion);
    void RunHistoryMenuSmokeCheck(const QString& path,
                                  std::function<void(bool)> completion);
    void RunFileMenuSmokeCheck(const QString& path,
                               std::function<void(bool)> completion);
    void RunEditMenuSmokeCheck(std::function<void(bool)> completion);
    void RunSearchMenuSmokeCheck(std::function<void(bool)> completion);
    void RunArticleTabSessionRestartSmokeCheck(
        bool prepare, std::function<void(bool)> completion);

   signals:
    void SourceDirectoriesEdited(
        const std::vector<std::string>& dictionary_paths,
        const std::vector<goldendict::core::SoundDirectoryConfiguration>&
            sound_directories);
    void LookupSubmitted(const QString& word, std::uint32_t group_id);
    void AddFavoriteRequested(const QString& word,
                              const QList<int>& parent_path);
    void AddFavoriteFolderRequested(const QString& name,
                                    const QList<int>& parent_path);
    void RenameFavoriteRequested(const QList<int>& path, const QString& name);
    void MoveFavoriteRequested(const QList<int>& path, int offset);
    void MoveFavoriteToRootRequested(const QList<int>& path);
    void MoveFavoriteAcrossFoldersRequested(
        const QList<int>& source_path, const QList<int>& destination_path,
        int destination_index, const QList<QList<int>>& expanded_paths);
    void ImportFavoritesRequested(const QString& path);
    void ExportFavoritesRequested(const QString& path);
    void RemoveFavoriteRequested(const QList<int>& path);
    void ClearHistoryRequested();
    void ImportHistoryRequested(const QString& path, std::uint32_t group_id);
    void DictionaryGroupsEdited();
    void ArticleTabSessionMutated();

   private slots:
    void EditSourceDirectories();
    void StartLookup();
    void FinishLookup();
    void FindInArticle(bool backwards = false);
    void PrintArticle();
    void PreviewArticle();
    void SaveArticleAsPdf();
    void SaveArticle();
    void UpdateNavigationActions();
    void ZoomArticle(double delta);
    void ShowDictionaryBrowser();
    void ExportHistory();
    void ImportHistory();
    void ClearHistory();
    void CreateFavoriteFolder();
    void RenameFavorite();
    void ImportFavorites();
    void ExportFavorites();
    void EditDictionaryGroups();

   private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void StartLookupInTab(goldendict::core::TabOpenPolicy open_policy,
                          goldendict::core::TabActivationPolicy activation,
                          const QString& internal_url = {});
    void StartNavigationLookup(
        goldendict::core::ArticleTabId tab_id,
        const goldendict::core::TabNavigationState& navigation,
        bool record_history);
    void SyncArticleTabs();
    void RebuildArticleTabs();
    void ActivateArticleTab(int index);
    void CloseArticleTab(int index);
    void CloseOtherArticleTabs(int index);
    void CreateEmptyArticleTab(bool activate);
    goldendict::core::TabPlacementPolicy NewTabPlacementPolicy() const;
    void NavigateArticleTab(bool forward);
    void OpenArticleLink(goldendict::core::ArticleTabId tab_id, const QUrl& url,
                         ArticleLinkDisposition disposition);
    void LookupArticleSelection(goldendict::core::ArticleTabId tab_id,
                                const QString& text,
                                ArticleLinkDisposition disposition);
    void StartPrinterRender(ArticleView* view, QPrinter* printer);
    void UpdateFileActions();
    void ShowTabContextMenu(const QPoint& position);
    ArticleView* CreateArticleView(goldendict::core::ArticleTabId tab_id);
    ArticleView* ArticleViewForTab(goldendict::core::ArticleTabId tab_id) const;
    goldendict::core::ArticleTabId TabIdAt(int index) const;
    void ShowMessage(const QString& title, const QString& message);
    void RefreshHistoryList();
    void UpdateHistoryActions();
    void SelectGroup(std::uint32_t group_id);
    void RefreshGroupSelector();
    void RefreshDictionaryBar();
    void ApplyDictionaryParticipation();
    std::vector<std::string> ParticipatingDictionaryIds(
        std::uint32_t group_id) const;
    void ApplyDictionaryFilter(goldendict::core::LookupQuery* query) const;
    void ApplyDictionaryFilter(goldendict::core::SuggestionQuery* query) const;
    void RefreshResultsNavigation();
    void RefreshSuggestions();
    void RefreshArticleSearch();
    void StartSuggestionLookup();
    void FinishSuggestionLookup(goldendict::core::ArticleTabId tab_id,
                                std::uint64_t generation,
                                goldendict::core::SuggestionResponse response);
    void ActivateSuggestion();
    void StopSuggestionWorker();
    void NavigateToSelectedResult();
    QList<int> SelectedFavoriteFolderPath() const;
    QList<QList<int>> ExpandedFavoriteFolderPaths() const;
    void ApplyDefaultPaneLayout();
    bool HasUsableMainWindowLayout() const;

    goldendict::core::DesktopFacade* facade_ = nullptr;
    std::vector<std::string> dictionary_paths_;
    std::vector<goldendict::core::SoundDirectoryConfiguration>
        sound_directories_;
    std::vector<goldendict::core::MediaWikiSourceConfiguration>
        mediawiki_sources_;
    std::vector<goldendict::core::WebsiteSourceConfiguration> website_sources_;
    std::vector<goldendict::core::ForvoSourceConfiguration> forvo_sources_;
    std::vector<goldendict::core::DictServerSourceConfiguration>
        dict_server_sources_;
    std::vector<goldendict::core::ExternalProgramSourceConfiguration>
        external_program_sources_;
    SourceApplyCallback source_apply_callback_;
    std::function<int(SourceDirectoriesDialog&)> source_dialog_executor_;
    bool source_configuration_busy_ = false;
    goldendict::core::ApplicationPreferences preferences_;
    QPushButton* dictionary_sources_button_ = nullptr;
    QLineEdit* query_ = nullptr;
    QComboBox* group_selector_ = nullptr;
    QPushButton* edit_groups_button_ = nullptr;
    std::vector<goldendict::core::DictionaryGroupConfiguration> groups_;
    std::vector<QShortcut*> group_shortcuts_;
    QLineEdit* history_filter_ = nullptr;
    QListWidget* history_list_ = nullptr;
    std::vector<HistoryViewItem> history_items_;
    std::uint32_t selected_group_id_ = 0U;
    QPushButton* clear_history_button_ = nullptr;
    QPushButton* export_history_button_ = nullptr;
    QPushButton* import_history_button_ = nullptr;
    QAction* clear_history_action_ = nullptr;
    QAction* export_history_action_ = nullptr;
    QAction* import_history_action_ = nullptr;
    HistoryExportCallback history_export_callback_;
    std::function<QString()> history_export_path_provider_;
    std::function<QString()> history_import_path_provider_;
    bool history_command_busy_ = false;
    FavoritesTreeWidget* favorites_tree_ = nullptr;
    QListWidget* results_list_ = nullptr;
    QListWidget* suggestions_list_ = nullptr;
    QAction* add_favorite_action_ = nullptr;
    QAction* add_favorite_folder_action_ = nullptr;
    QAction* rename_favorite_action_ = nullptr;
    QAction* move_favorite_up_action_ = nullptr;
    QAction* move_favorite_down_action_ = nullptr;
    QAction* move_favorite_to_root_action_ = nullptr;
    QAction* import_favorites_action_ = nullptr;
    QAction* export_favorites_action_ = nullptr;
    QAction* remove_favorite_action_ = nullptr;
    QAction* dictionary_browser_action_ = nullptr;
    QToolButton* lookup_button_ = nullptr;
    QToolBar* dictionary_bar_ = nullptr;
    std::map<std::uint32_t, std::vector<std::string>> participating_ids_;
    std::map<std::uint32_t, std::vector<std::string>> dictionary_members_;
    std::map<std::uint32_t, std::vector<std::string>> solo_restore_ids_;
    QLabel* status_ = nullptr;
    QTabWidget* article_tabs_ = nullptr;
    ArticleView* article_view_ = nullptr;
    ArticlePage* article_page_ = nullptr;
    ArticleSchemeHandler* scheme_handler_ = nullptr;
    QLineEdit* article_search_ = nullptr;
    QLabel* article_search_status_ = nullptr;
    QAction* back_action_ = nullptr;
    QAction* forward_action_ = nullptr;
    QAction* new_tab_action_ = nullptr;
    QAction* print_preview_action_ = nullptr;
    QAction* print_action_ = nullptr;
    QAction* save_article_action_ = nullptr;
    QAction* quit_action_ = nullptr;
    QAction* dictionaries_action_ = nullptr;
    QAction* search_in_page_action_ = nullptr;
    QTimer* completion_timer_ = nullptr;
    std::map<goldendict::core::ArticleTabId,
             std::unique_ptr<goldendict::core::LookupRequest>>
        requests_;
    std::map<goldendict::core::ArticleTabId,
             std::vector<goldendict::core::DictionaryIdentity>>
        lookup_results_;

    struct ArticleSearchPresentation {
        QString query;
        QString status;
        std::uint64_t generation = 0U;
    };

    std::map<goldendict::core::ArticleTabId, ArticleSearchPresentation>
        article_search_presentations_;

    struct SuggestionPresentation {
        QString query;
        std::uint32_t group_id = 0U;
        std::uint64_t generation = 0U;
        std::vector<goldendict::core::HeadwordSuggestion> rows;
    };

    std::map<goldendict::core::ArticleTabId, SuggestionPresentation>
        suggestions_;
    std::uint64_t suggestion_generation_ = 0U;
    std::unique_ptr<SuggestionWorker> suggestion_worker_;
    DictionaryBrowser* dictionary_browser_ = nullptr;
    std::unique_ptr<QPrinter> printer_;
    bool print_in_progress_ = false;
    bool restoring_main_window_state_ = false;
    std::function<bool(QPrinter*)> print_dialog_executor_;
    std::function<void(QPrinter*, const std::function<void()>&)>
        print_preview_executor_;
    std::function<void(ArticleView*, QPrinter*)> print_dispatcher_;
    std::function<bool()> printer_available_;
    std::function<QString()> save_article_path_provider_;
    std::function<bool(const QString&, const QString&)> article_save_writer_;
    std::function<void()> quit_dispatcher_;
    bool save_in_progress_ = false;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
