// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_INSPECTOR_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_INSPECTOR_H_

#include <QPointer>
#include <QWebEnginePage>
#include <QWidget>

class QWebEngineProfile;
class QWebEngineView;

// Private presentation adapter; ArticleView owns this independent window.
class ArticleInspector final : public QWidget {
   public:
    explicit ArticleInspector(QWebEnginePage* inspected_page);
    ~ArticleInspector() override;

    QWebEnginePage* inspectedPage() const;
    void Inspect(bool context_target);

   private:
    QPointer<QWebEnginePage> inspected_page_;
    QWebEngineProfile* profile_ = nullptr;
    QWebEngineView* view_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_INSPECTOR_H_
