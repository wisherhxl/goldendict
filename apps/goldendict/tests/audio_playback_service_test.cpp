// SPDX-License-Identifier: GPL-3.0-or-later

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>

#include <memory>
#include <vector>

#include "audio_playback_service.h"
#include "goldendict/core/application.h"

namespace {

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
        "bookname=Audio Playback Fixture\n"
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
    QCoreApplication application(argc, argv);
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

    QByteArray played_bytes;
    QString played_media_type;
    std::vector<AudioPlaybackService::Status> statuses;
    QString status_detail;
    AudioPlaybackService service(
        nullptr,
        [&](const QByteArray& bytes, const QString& media_type) {
            played_bytes = bytes;
            played_media_type = media_type;
            return true;
        },
        [&](AudioPlaybackService::Status status, const QString& detail) {
            statuses.push_back(status);
            status_detail = detail;
        });
    const QUrl audio_url = QUrl::fromEncoded(
        QByteArrayLiteral("goldendict://resource/") +
        QUrl::toPercentEncoding(QString::fromStdString(catalog.front().id)) +
        QByteArrayLiteral("/") +
        QUrl::toPercentEncoding(QStringLiteral("audio/clip.wav")));
    if (service.Play(*facade, audio_url) !=
            AudioPlaybackService::Result::kStarted ||
        played_bytes != QByteArrayLiteral("RIFFfixtureWAVE") ||
        played_media_type != QStringLiteral("audio/wav") || statuses.empty() ||
        statuses.back() != AudioPlaybackService::Status::kLoading ||
        status_detail != QStringLiteral("audio/wav")) {
        return 1;
    }

    const QUrl article_url = QUrl::fromEncoded(
        QByteArrayLiteral("goldendict://article/") +
        QUrl::toPercentEncoding(QString::fromStdString(catalog.front().id)) +
        QByteArrayLiteral("/fixture"));
    if (service.Play(*facade, article_url) !=
            AudioPlaybackService::Result::kInvalidResource ||
        statuses.back() != AudioPlaybackService::Status::kFailed ||
        status_detail != QStringLiteral("Invalid audio resource")) {
        return 1;
    }
    if (!AudioPlaybackService::HasWavDecoder())
        return 1;
    return 0;
}
