// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlScheme>

#include <memory>
#include <string>
#include <unordered_map>

#include "article_content_origin.h"
#include "article_scheme_handler.h"
#include "goldendict/core/application.h"
#include "widgets_facade_binding.h"

namespace {

constexpr auto kSchemeName = "goldendict";

void RegisterArticleScheme() {
    QWebEngineUrlScheme scheme(QByteArrayLiteral("goldendict"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(0);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                    QWebEngineUrlScheme::LocalScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
}

bool WriteFile(const QString& path, const QByteArray& contents) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) &&
           file.write(contents) == contents.size();
}

bool WriteStardictFixture(const QString& directory) {
    QByteArray index("fixture", 7);
    index.append('\0');
    index.append(QByteArray::fromHex("0000000000000007"));
    const QByteArray info =
        "StarDict's dict ifo file\n"
        "version=2.4.2\n"
        "bookname=Article Scheme Fixture\n"
        "wordcount=1\n"
        "idxfilesize=" +
        QByteArray::number(index.size()) +
        "\n"
        "sametypesequence=m\n";
    QDir root(directory);
    return root.mkpath(QStringLiteral("res")) &&
           WriteFile(root.filePath(QStringLiteral("fixture.ifo")), info) &&
           WriteFile(root.filePath(QStringLiteral("fixture.idx")), index) &&
           WriteFile(root.filePath(QStringLiteral("fixture.dict")),
                     QByteArrayLiteral("fixture")) &&
           WriteFile(root.filePath(QStringLiteral("res/payload.svg")),
                     QByteArrayLiteral(
                         "<svg xmlns='http://www.w3.org/2000/svg'>"
                         "<text x='0' y='20'>Article scheme resource ready"
                         "</text></svg>"));
}

std::shared_ptr<goldendict::core::DesktopFacade> CreateFixtureFacade(
    const QString& directory) {
    goldendict::core::CoreConfiguration configuration;
    configuration.index_directory =
        QDir(directory).filePath(QStringLiteral("index")).toStdString();
    configuration.dictionary_paths = {directory.toStdString()};
    return std::shared_ptr<goldendict::core::DesktopFacade>(
        goldendict::core::CreateDesktopFacade(configuration));
}

goldendict::widgets::WidgetsFacadeBindingDescriptor CreateBinding(
    const std::shared_ptr<goldendict::core::DesktopFacade>& facade) {
    goldendict::widgets::WidgetsFacadeBindingDescriptor binding;
    binding.facade_owner = facade;
    binding.facade = facade.get();
    binding.service = &facade->GetDictionaryService();
    binding.catalog = binding.service->GetCatalog();
    for (std::size_t index = 0; index < binding.catalog.size(); ++index) {
        binding.catalog_index.emplace(binding.catalog[index].id, index);
    }
    binding.consumers = goldendict::widgets::kCompleteFacadeBindingConsumers;
    return binding;
}

}  // namespace

int main(int argc, char* argv[]) {
    RegisterArticleScheme();
    QApplication application(argc, argv);

    QTemporaryDir fixture;
    if (!fixture.isValid())
        return 1;
    if (!WriteStardictFixture(fixture.path()))
        return 1;

    std::shared_ptr<goldendict::core::DesktopFacade> facade;
    try {
        facade = CreateFixtureFacade(fixture.path());
    } catch (...) {
        return 1;
    }
    const auto catalog = facade->GetDictionaryService().GetCatalog();
    if (catalog.size() != 1U)
        return 1;

    QUrl resource_url;
    resource_url.setScheme(QString::fromLatin1(kSchemeName));
    resource_url.setHost(QStringLiteral("resource"));
    resource_url.setPath(QStringLiteral("/") +
                         QString::fromStdString(catalog.front().id) +
                         QStringLiteral("/payload.svg"));
    const auto resolved =
        facade->ResolveArticleUrl(resource_url.toEncoded().toStdString());
    if (!resolved.has_value() ||
        resolved->kind != goldendict::core::ArticleUrlKind::kResource ||
        facade->GetDictionaryService()
            .GetResource(resolved->resource)
            .empty()) {
        return 1;
    }

    goldendict::widgets::WidgetsFacadeBindingRegistry registry;
    const auto prepared = registry.Prepare(CreateBinding(facade));
    if (!prepared.has_value() || !registry.Publish(*prepared))
        return 1;

    ArticleSchemeHandler handler;
    handler.SetBindingRegistry(&registry);
    auto* profile = QWebEngineProfile::defaultProfile();
    profile->installUrlSchemeHandler(QByteArrayLiteral("goldendict"), &handler);
    QWebEnginePage page(profile);

    QTimer::singleShot(10000, &application,
                       [&application]() { application.exit(2); });
    QObject::connect(
        &page, &QWebEnginePage::loadFinished, &application,
        [&application, &page](bool loaded) {
            if (!loaded) {
                application.exit(1);
                return;
            }
            page.runJavaScript(
                QStringLiteral("(() => { const image = "
                               "document.getElementById('resource'); return "
                               "image && image.complete && "
                               "image.naturalWidth > 0; })()"),
                [&application](const QVariant& result) {
                    application.exit(result.toBool() ? 0 : 1);
                });
        });
    page.setHtml(QStringLiteral("<!doctype html><html><body>"
                                "<img id=\"resource\" src=\"%1\">"
                                "</body></html>")
                     .arg(resource_url.toString(QUrl::FullyEncoded)),
                 goldendict::app::ArticleContentBaseUrl());
    const int result = application.exec();

    profile->removeUrlSchemeHandler(&handler);
    registry.ClearPublished();
    registry.ReclaimRetired();
    registry.Shutdown();
    return result;
}
