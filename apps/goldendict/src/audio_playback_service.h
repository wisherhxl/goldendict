// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef GOLDENDICT_APPS_GOLDENDICT_AUDIO_PLAYBACK_SERVICE_H_
#define GOLDENDICT_APPS_GOLDENDICT_AUDIO_PLAYBACK_SERVICE_H_
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>
#include <memory>

namespace goldendict::core {
class DesktopFacade;
}
class QAudioOutput;
class QBuffer;
class QMediaPlayer;

class AudioPlaybackService final : public QObject {
   public:
    enum class Result { kStarted, kInvalidResource, kEmptyResource, kFailed };
    enum class Status { kLoading, kPlaying, kFailed };
    using PlaybackSink = std::function<bool(const QByteArray&, const QString&)>;
    using StatusSink = std::function<void(Status, const QString&)>;
    explicit AudioPlaybackService(QObject* parent = nullptr,
                                  PlaybackSink playback_sink = {},
                                  StatusSink status_sink = {});
    ~AudioPlaybackService() override;
    Result Play(const goldendict::core::DesktopFacade& facade, const QUrl& url);
    static bool HasWavDecoder();

   private:
    bool PlayWithQt(const QByteArray& bytes, const QString& media_type);
    void Report(Status status, const QString& detail) const;
    PlaybackSink playback_sink_;
    StatusSink status_sink_;
    std::unique_ptr<QBuffer> buffer_;
    std::unique_ptr<QAudioOutput> audio_output_;
    std::unique_ptr<QMediaPlayer> player_;
};
#endif
