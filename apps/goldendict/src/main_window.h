// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
#define GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_

#include <functional>
#include <memory>

#include <QMainWindow>

class ArticlePage;
class ArticleSchemeHandler;
class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QWebEngineView;

namespace goldendict::core {
class DesktopFacade;
class LookupRequest;
}  // namespace goldendict::core

class MainWindow final : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void SetFacade(goldendict::core::DesktopFacade* facade);
    void RunWebEngineSmokeCheck(std::function<void(bool)> completion);

   signals:
    void DictionaryDirectorySelected(const QString& directory);

   private slots:
    void ChooseDictionaryDirectory();
    void StartLookup();
    void FinishLookup();

   private:
    void ShowMessage(const QString& title, const QString& message);

    goldendict::core::DesktopFacade* facade_ = nullptr;
    std::unique_ptr<goldendict::core::LookupRequest> request_;
    QLineEdit* query_ = nullptr;
    QPushButton* lookup_button_ = nullptr;
    QLabel* status_ = nullptr;
    QWebEngineView* article_view_ = nullptr;
    ArticlePage* article_page_ = nullptr;
    ArticleSchemeHandler* scheme_handler_ = nullptr;
    QTimer* completion_timer_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_MAIN_WINDOW_H_
