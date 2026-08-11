// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_SCHEME_HANDLER_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_SCHEME_HANDLER_H_

#include <QWebEngineUrlSchemeHandler>

namespace goldendict::core {
class DesktopFacade;
}

class ArticleSchemeHandler final : public QWebEngineUrlSchemeHandler {
    Q_OBJECT

   public:
    explicit ArticleSchemeHandler(QObject* parent = nullptr);

    void SetFacade(const goldendict::core::DesktopFacade* facade) noexcept;
    void requestStarted(QWebEngineUrlRequestJob* request) override;

   private:
    const goldendict::core::DesktopFacade* facade_ = nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_SCHEME_HANDLER_H_
