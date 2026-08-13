// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_PAGE_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_PAGE_H_

#include <QWebEnginePage>

class QUrl;

enum class ArticleLinkDisposition {
    kCurrentTab,
    kNewForegroundTab,
    kNewBackgroundTab,
};

namespace goldendict::core {
class DesktopFacade;
}

class ArticlePage final : public QWebEnginePage {
    Q_OBJECT

   public:
    explicit ArticlePage(QObject* parent = nullptr);

    void SetFacade(const goldendict::core::DesktopFacade* facade) noexcept;
    void SetOpenNewTabsInBackground(bool enabled) noexcept;

   signals:
    void LookupRequested(const QString& text, const QString& internal_url,
                         ArticleLinkDisposition disposition);
    void ExternalUrlRequested(const QUrl& url);

   protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType type,
                                 bool is_main_frame) override;
    QWebEnginePage* createWindow(WebWindowType type) override;

   private:
    const goldendict::core::DesktopFacade* facade_ = nullptr;
    bool new_window_navigation_pending_ = false;
    bool open_new_tabs_in_background_ = true;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_PAGE_H_
