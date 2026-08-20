// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_ARTICLE_SCHEME_HANDLER_H_
#define GOLDENDICT_APPS_GOLDENDICT_ARTICLE_SCHEME_HANDLER_H_

#include <QWebEngineUrlSchemeHandler>

namespace goldendict::core {
class DesktopFacade;
}

namespace goldendict::widgets {
class WidgetsFacadeBindingRegistry;
}

class ArticleSchemeHandler final : public QWebEngineUrlSchemeHandler {
    Q_OBJECT

   public:
    explicit ArticleSchemeHandler(QObject* parent = nullptr);

    void SetFacade(const goldendict::core::DesktopFacade* facade) noexcept;
    void SetBindingRegistry(
        const goldendict::widgets::WidgetsFacadeBindingRegistry*
            registry) noexcept;

    bool UsesBindingRegistry(
        const goldendict::widgets::WidgetsFacadeBindingRegistry* registry)
        const noexcept {
        return registry_ == registry;
    }

    void requestStarted(QWebEngineUrlRequestJob* request) override;

   private:
    const goldendict::core::DesktopFacade* facade_ = nullptr;
    const goldendict::widgets::WidgetsFacadeBindingRegistry* registry_ =
        nullptr;
};

#endif  // GOLDENDICT_APPS_GOLDENDICT_ARTICLE_SCHEME_HANDLER_H_
