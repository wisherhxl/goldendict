// SPDX-License-Identifier: GPL-3.0-or-later

#include "article_scheme_handler.h"

#include <QBuffer>
#include <QByteArray>
#include <QUrl>
#include <QWebEngineUrlRequestJob>

#include "goldendict/core/desktop_facade.h"
#include "widgets_facade_binding.h"

ArticleSchemeHandler::ArticleSchemeHandler(QObject* parent)
    : QWebEngineUrlSchemeHandler(parent) {}

void ArticleSchemeHandler::SetFacade(
    const goldendict::core::DesktopFacade* facade) noexcept {
    facade_ = facade;
}

void ArticleSchemeHandler::SetBindingRegistry(
    const goldendict::widgets::WidgetsFacadeBindingRegistry*
        registry) noexcept {
    registry_ = registry;
}

void ArticleSchemeHandler::requestStarted(QWebEngineUrlRequestJob* request) {
    auto lease =
        registry_ == nullptr
            ? goldendict::widgets::WidgetsFacadeBindingRegistry::Lease{}
            : registry_->Acquire();
    const auto* facade = lease ? lease->facade : facade_;
    if (facade == nullptr) {
        request->fail(QWebEngineUrlRequestJob::RequestFailed);
        return;
    }
    const auto resolved = facade->ResolveArticleUrl(
        request->requestUrl().toString().toStdString());
    if (!resolved.has_value() ||
        resolved->kind != goldendict::core::ArticleUrlKind::kResource) {
        request->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    const auto bytes =
        facade->GetDictionaryService().GetResource(resolved->resource);
    if (bytes.empty()) {
        request->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    auto* buffer = new QBuffer(request);
    buffer->setData(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<qsizetype>(bytes.size()));
    buffer->open(QIODevice::ReadOnly);
    request->reply(QByteArray::fromStdString(resolved->resource.media_type),
                   buffer);
}
