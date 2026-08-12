// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
#define GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_

#include <functional>
#include <memory>
#include <vector>

#include <QList>
#include <QMainWindow>
#include <QStringList>

class ArticlePage;
class ArticleSchemeHandler;
class DictionaryBrowser;
class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTimer;
class QTreeWidget;
class QWebEngineView;

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

class MainWindow final : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void SetFacade(goldendict::core::DesktopFacade* facade);
    void SetHistoryWords(const QStringList& words);
    void SetFavoriteItems(const std::vector<FavoriteViewItem>& items);
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

   signals:
    void DictionaryDirectorySelected(const QString& directory);
    void LookupSubmitted(const QString& word);
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
    void ImportHistoryRequested(const QString& path);

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

   private:
    void ShowMessage(const QString& title, const QString& message);
    void RefreshHistoryList();
    bool ExportHistoryToFile(const QString& path);
    QList<int> SelectedFavoriteFolderPath() const;

    goldendict::core::DesktopFacade* facade_ = nullptr;
    std::unique_ptr<goldendict::core::LookupRequest> request_;
    QLineEdit* query_ = nullptr;
    QLineEdit* history_filter_ = nullptr;
    QListWidget* history_list_ = nullptr;
    QStringList history_words_;
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
    QPushButton* lookup_button_ = nullptr;
    QLabel* status_ = nullptr;
    QWebEngineView* article_view_ = nullptr;
    ArticlePage* article_page_ = nullptr;
    ArticleSchemeHandler* scheme_handler_ = nullptr;
    QLineEdit* article_search_ = nullptr;
    QLabel* article_search_status_ = nullptr;
    QAction* back_action_ = nullptr;
    QAction* forward_action_ = nullptr;
    QTimer* completion_timer_ = nullptr;
    DictionaryBrowser* dictionary_browser_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
