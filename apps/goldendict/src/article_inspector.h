// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_INSPECTOR_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_INSPECTOR_H_

#include <QPointer>
#include <QWebEnginePage>
#include <QWidget>

#include <memory>
#include <optional>
#include <vector>

class QWebEngineProfile;
class QWebEngineView;
class ArticleInspector;

// Application-lifetime presentation state, with no file-storage responsibility.
class ArticleInspectorState final : public QObject {
    Q_OBJECT
   public:
    void SetInitialGeometry(const QByteArray& geometry);
    QByteArray geometry() const;
    void CheckpointForExit();

   signals:
    void GeometryCaptured();

   private:
    friend class ArticleInspector;
    void Restore(ArticleInspector* inspector);
    void Forget(ArticleInspector* inspector);
    void Adjusted(ArticleInspector* inspector);
    void Closed(ArticleInspector* inspector);
    std::vector<ArticleInspector*> retained_;
    QByteArray saved_;
    std::optional<QByteArray> latest_adjusted_;
    bool exiting_ = false;
};

// Private presentation adapter; ArticleView owns this independent window.
class ArticleInspector final : public QWidget {
   public:
    explicit ArticleInspector(
        QWebEnginePage* inspected_page,
        std::shared_ptr<ArticleInspectorState> state = {});
    ~ArticleInspector() override;

    QWebEnginePage* inspectedPage() const;
    void Inspect(bool context_target);

   protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

   private:
    QPointer<QWebEnginePage> inspected_page_;
    QWebEngineProfile* profile_ = nullptr;
    QWebEngineView* view_ = nullptr;
    std::shared_ptr<ArticleInspectorState> state_;
    bool restoring_ = false;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_INSPECTOR_H_
