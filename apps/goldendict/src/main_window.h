// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
#define GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_

#include <functional>
#include <memory>
#include <vector>

#include <QMainWindow>
#include <QStringList>

class ArticlePage;
class ArticleSchemeHandler;
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
    void RunFavoritesSmokeCheck(std::function<void(bool)> completion);

   signals:
    void DictionaryDirectorySelected(const QString& directory);
    void LookupSubmitted(const QString& word);
    void AddFavoriteRequested(const QString& word);

   private slots:
    void ChooseDictionaryDirectory();
    void StartLookup();
    void FinishLookup();
    void FindInArticle(bool backwards = false);
    void PrintArticle();
    void SaveArticle();
    void UpdateNavigationActions();
    void ZoomArticle(double delta);

   private:
    void ShowMessage(const QString& title, const QString& message);

    goldendict::core::DesktopFacade* facade_ = nullptr;
    std::unique_ptr<goldendict::core::LookupRequest> request_;
    QLineEdit* query_ = nullptr;
    QListWidget* history_list_ = nullptr;
    QTreeWidget* favorites_tree_ = nullptr;
    QAction* add_favorite_action_ = nullptr;
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
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
