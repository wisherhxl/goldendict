// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_playback_service.h"
#include <QAudioOutput>
#include <QBuffer>
#include <QMediaFormat>
#include <QMediaPlayer>
#include <utility>
#include "goldendict/core/desktop_facade.h"

AudioPlaybackService::AudioPlaybackService(QObject* parent,
                                           PlaybackSink playback_sink,
                                           StatusSink status_sink)
    : QObject(parent),
      playback_sink_(std::move(playback_sink)),
      status_sink_(std::move(status_sink)) {
    if (!playback_sink_) {
        playback_sink_ = [this](const QByteArray& bytes, const QString& type) {
            return PlayWithQt(bytes, type);
        };
    }
}

AudioPlaybackService::~AudioPlaybackService() = default;

bool AudioPlaybackService::HasWavDecoder() {
    return QMediaFormat().supportedFileFormats(QMediaFormat::Decode)
        .contains(QMediaFormat::Wave);
}

void AudioPlaybackService::Report(Status status, const QString& detail) const {
    if (status_sink_)
        status_sink_(status, detail);
}

AudioPlaybackService::Result AudioPlaybackService::Play(
    const goldendict::core::DesktopFacade& facade, const QUrl& url) {
    const auto resolved =
        facade.ResolveArticleUrl(url.toEncoded().toStdString());
    if (!resolved ||
        resolved->kind != goldendict::core::ArticleUrlKind::kResource ||
        resolved->resource.media_type.rfind("audio/", 0) != 0) {
        Report(Status::kFailed, QStringLiteral("Invalid audio resource"));
        return Result::kInvalidResource;
    }
    const auto bytes =
        facade.GetDictionaryService().GetResource(resolved->resource);
    if (bytes.empty()) {
        Report(Status::kFailed, QStringLiteral("Audio resource is empty"));
        return Result::kEmptyResource;
    }
    const QByteArray data(reinterpret_cast<const char*>(bytes.data()),
                          static_cast<qsizetype>(bytes.size()));
    const QString media_type =
        QString::fromStdString(resolved->resource.media_type);
    Report(Status::kLoading, media_type);
    if (playback_sink_(data, media_type))
        return Result::kStarted;
    Report(Status::kFailed, QStringLiteral("Playback could not be started"));
    return Result::kFailed;
}

bool AudioPlaybackService::PlayWithQt(const QByteArray& bytes,
                                      const QString& media_type) {
    static_cast<void>(media_type);
    if (!player_) {
        audio_output_ = std::make_unique<QAudioOutput>();
        player_ = std::make_unique<QMediaPlayer>();
        player_->setAudioOutput(audio_output_.get());
        connect(player_.get(), &QMediaPlayer::playbackStateChanged, this,
                [this](QMediaPlayer::PlaybackState state) {
                    if (state == QMediaPlayer::PlayingState)
                        Report(Status::kPlaying, QString());
                });
        connect(player_.get(), &QMediaPlayer::errorOccurred, this,
                [this](QMediaPlayer::Error, const QString& error) {
                    Report(Status::kFailed,
                           error.isEmpty()
                               ? QStringLiteral("Media backend error")
                               : error);
                });
    }
    player_->stop();
    buffer_ = std::make_unique<QBuffer>();
    buffer_->setData(bytes);
    if (!buffer_->open(QIODevice::ReadOnly))
        return false;
    player_->setSourceDevice(buffer_.get());
    player_->play();
    return true;
}
