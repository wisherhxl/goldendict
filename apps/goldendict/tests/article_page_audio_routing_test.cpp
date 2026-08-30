// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QWebEngineUrlScheme>

#include <memory>

#include "article_content_origin.h"
#include "article_page.h"
#include "goldendict/core/application.h"

class ArticlePageAudioRoutingTestAccess {
   public:
    static bool Request(ArticlePage& page, const QUrl& url) {
        return page.acceptNavigationRequest(
            url, QWebEnginePage::NavigationTypeLinkClicked, true);
    }
};

namespace {

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

bool WriteFixture(const QString& directory) {
    QByteArray index("fixture", 7);
    index.append('\0');
    index.append(QByteArray::fromHex("0000000000000007"));
    const QByteArray info =
        "StarDict's dict ifo file\n"
        "version=2.4.2\n"
        "bookname=Article Audio Fixture\n"
        "wordcount=1\n"
        "idxfilesize=" +
        QByteArray::number(index.size()) +
        "\n"
        "sametypesequence=m\n";
    QDir root(directory);
    return root.mkpath(QStringLiteral("res/audio")) &&
           WriteFile(root.filePath(QStringLiteral("fixture.ifo")), info) &&
           WriteFile(root.filePath(QStringLiteral("fixture.idx")), index) &&
           WriteFile(root.filePath(QStringLiteral("fixture.dict")),
                     QByteArrayLiteral("fixture")) &&
           WriteFile(root.filePath(QStringLiteral("res/audio/clip.wav")),
                     QByteArrayLiteral("RIFFfixtureWAVE"));
}

}  // namespace

int main(int argc, char* argv[]) {
    RegisterArticleScheme();
    QApplication application(argc, argv);

    QTemporaryDir fixture;
    if (!fixture.isValid() || !WriteFixture(fixture.path()))
        return 1;

    goldendict::core::CoreConfiguration configuration;
    configuration.index_directory =
        QDir(fixture.path()).filePath(QStringLiteral("index")).toStdString();
    configuration.dictionary_paths = {fixture.path().toStdString()};
    std::unique_ptr<goldendict::core::DesktopFacade> facade;
    try {
        facade = goldendict::core::CreateDesktopFacade(configuration);
    } catch (...) {
        return 1;
    }
    const auto catalog = facade->GetDictionaryService().GetCatalog();
    if (catalog.size() != 1U)
        return 1;

    const QUrl audio_url = QUrl::fromEncoded(
        QByteArrayLiteral("goldendict://resource/") +
        QUrl::toPercentEncoding(QString::fromStdString(catalog.front().id)) +
        QByteArrayLiteral("/") +
        QUrl::toPercentEncoding(QStringLiteral("audio/clip.wav")));
    const auto resolved =
        facade->ResolveArticleUrl(audio_url.toEncoded().toStdString());
    if (!resolved.has_value() ||
        resolved->kind != goldendict::core::ArticleUrlKind::kResource ||
        resolved->resource.media_type != "audio/wav") {
        return 1;
    }

    ArticlePage page;
    page.SetFacade(facade.get());
    QSignalSpy audio_requested(&page, &ArticlePage::AudioResourceRequested);
    if (ArticlePageAudioRoutingTestAccess::Request(page, audio_url) ||
        audio_requested.size() != 1 ||
        audio_requested.at(0).at(0).toUrl() != audio_url) {
        return 1;
    }
    return 0;
}
