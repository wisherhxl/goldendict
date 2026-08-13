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
class ArticleSchemeHandler;
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
class QToolButton;
class QTreeWidget;
class QWebEngineView;
class QShortcut;

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
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void SetFacade(goldendict::core::DesktopFacade* facade);
    void SetPreferences(
        const goldendict::core::ApplicationPreferences& preferences);
    bool RestoreMainWindowGeometry(const std::string& geometry);
    std::string CaptureMainWindowGeometry() const;
    void SetHistoryWords(const QStringList& words);
    void SetHistoryItems(const std::vector<HistoryViewItem>& items);
    void SetFavoriteItems(const std::vector<FavoriteViewItem>& items);
    void SetDictionaryGroups(
        const std::vector<goldendict::core::DictionaryGroupConfiguration>&
            groups);
    const std::vector<goldendict::core::DictionaryGroupConfiguration>&
    DictionaryGroups() const noexcept;
    void RunWebEngineSmokeCheck(std::function<void(bool)> completion);
    void RunWebEngineInteractionCheck(std::function<void(bool)> completion);
    void RunHistorySmokeCheck(std::function<void(bool)> completion);
    void RunHistoryManagementSmokeCheck(std::function<void(bool)> completion);
    void RunHistoryExportSmokeCheck(const QString& path,
                                    std::function<void(bool)> completion);
    void RunHistoryImportSmokeCheck(const QString& path,
                                    std::function<void(bool)> completion);
    void RunFavoritesSmokeCheck(std::function<void(bool)> completion);
    void RunFavoritesTransferSmokeCheck(const QString& path,
                                        std::function<void(bool)> completion);
    void RunDictionaryBrowserSmokeCheck(std::function<void(bool)> completion);
    void RunDictionaryBrowserExportSmokeCheck(
        const QString& path, std::function<void(bool)> completion);
    void RunDictionaryGroupsSmokeCheck(std::function<void(bool)> completion);
    void RunArticleTabsSmokeCheck(std::function<void(bool)> completion);
    void RunArticleTabSessionRestartSmokeCheck(
        bool prepare, std::function<void(bool)> completion);

   signals:
    void DictionaryDirectorySelected(const QString& directory);
    void LookupSubmitted(const QString& word, std::uint32_t group_id);
    void AddFavoriteRequested(const QString& word,
                              const QList<int>& parent_path);
    void AddFavoriteFolderRequested(const QString& name,
                                    const QList<int>& parent_path);
    void RenameFavoriteRequested(const QList<int>& path, const QString& name);
    void MoveFavoriteRequested(const QList<int>& path, int offset);
    void MoveFavoriteToRootRequested(const QList<int>& path);
    void ImportFavoritesRequested(const QString& path);
    void ExportFavoritesRequested(const QString& path);
    void RemoveFavoriteRequested(const QList<int>& path);
    void ClearHistoryRequested();
    void ImportHistoryRequested(const QString& path, std::uint32_t group_id);
    void DictionaryGroupsEdited();
    void ArticleTabSessionMutated();

   private slots:
    void ChooseDictionaryDirectory();
    void StartLookup();
    void FinishLookup();
    void FindInArticle(bool backwards = false);
    void PrintArticle();
    void SaveArticle();
    void UpdateNavigationActions();
    void ZoomArticle(double delta);
    void ShowDictionaryBrowser();
    void ExportHistory();
    void ImportHistory();
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
    void ShowTabContextMenu(const QPoint& position);
    QWebEngineView* CreateArticleView(goldendict::core::ArticleTabId tab_id);
    QWebEngineView* ArticleView(goldendict::core::ArticleTabId tab_id) const;
    goldendict::core::ArticleTabId TabIdAt(int index) const;
    void ShowMessage(const QString& title, const QString& message);
    void RefreshHistoryList();
    void SelectGroup(std::uint32_t group_id);
    void RefreshGroupSelector();
    bool ExportHistoryToFile(const QString& path);
    QList<int> SelectedFavoriteFolderPath() const;

    goldendict::core::DesktopFacade* facade_ = nullptr;
    goldendict::core::ApplicationPreferences preferences_;
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
    QTreeWidget* favorites_tree_ = nullptr;
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
    QLabel* status_ = nullptr;
    QTabWidget* article_tabs_ = nullptr;
    QWebEngineView* article_view_ = nullptr;
    ArticlePage* article_page_ = nullptr;
    ArticleSchemeHandler* scheme_handler_ = nullptr;
    QLineEdit* article_search_ = nullptr;
    QLabel* article_search_status_ = nullptr;
    QAction* back_action_ = nullptr;
    QAction* forward_action_ = nullptr;
    QTimer* completion_timer_ = nullptr;
    std::map<goldendict::core::ArticleTabId,
             std::unique_ptr<goldendict::core::LookupRequest>>
        requests_;
    DictionaryBrowser* dictionary_browser_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
