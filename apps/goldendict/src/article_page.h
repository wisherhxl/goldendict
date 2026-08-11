// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_PAGE_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_PAGE_H_

#include <QWebEnginePage>

namespace goldendict::core {
class DesktopFacade;
}

class ArticlePage final : public QWebEnginePage {
    Q_OBJECT

   public:
    explicit ArticlePage(QObject* parent = nullptr);

    void SetFacade(const goldendict::core::DesktopFacade* facade) noexcept;

   signals:
    void LookupRequested(const QString& text);

   protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType type,
                                 bool is_main_frame) override;

   private:
    const goldendict::core::DesktopFacade* facade_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_PAGE_H_
