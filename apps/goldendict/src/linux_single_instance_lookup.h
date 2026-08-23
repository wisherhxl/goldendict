// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_LINUX_SINGLE_INSTANCE_LOOKUP_H_
#define GOLDENDICT_LINUX_SINGLE_INSTANCE_LOOKUP_H_

#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

class QLocalServer;
class QLockFile;

#if defined(Q_OS_LINUX)
namespace goldendict::app {

enum class SingleInstanceStartResult {
    kPrimary,
    kForwarded,
    kForwardFailed,
    kSecondaryNoOp,
};

enum class SingleInstanceMessageKind {
    kActivation,
    kLookup,
};

class SingleInstanceMessage final {
   public:
    static SingleInstanceMessage Activation();
    static SingleInstanceMessage Lookup(QString normalized_lookup);

    SingleInstanceMessageKind Kind() const noexcept;
    const QString& LookupText() const noexcept;

   private:
    explicit SingleInstanceMessage(SingleInstanceMessageKind kind,
                                   QString lookup = {});

    SingleInstanceMessageKind kind_;
    QString lookup_;
};

class LinuxSingleInstanceLookup final : public QObject {
    Q_OBJECT

   public:
    static constexpr int kForwardTimeoutMilliseconds = 10000;
    static constexpr qsizetype kMaximumPendingMessages = 32;

    explicit LinuxSingleInstanceLookup(QString endpoint,
                                       QObject* parent = nullptr);
    ~LinuxSingleInstanceLookup() override;

    LinuxSingleInstanceLookup(const LinuxSingleInstanceLookup&) = delete;
    LinuxSingleInstanceLookup& operator=(const LinuxSingleInstanceLookup&) =
        delete;

    SingleInstanceStartResult Start(
        const std::optional<SingleInstanceMessage>& message);
    void PublishConsumer(std::function<void(SingleInstanceMessage)> consumer);
    bool IsPrimary() const noexcept;

   private:
    bool Forward(const SingleInstanceMessage& message);
    void ReceiveConnections();
    void ReceiveMessage(QObject* connection);

    QString endpoint_;
    std::unique_ptr<QLockFile> lock_;
    std::unique_ptr<QLocalServer> server_;
    QList<SingleInstanceMessage> pending_;
    std::function<void(SingleInstanceMessage)> consumer_;
    bool primary_ = false;
};

QString LinuxSingleInstanceEndpoint();

}  // namespace goldendict::app
#endif  // defined(Q_OS_LINUX)

#endif  // GOLDENDICT_LINUX_SINGLE_INSTANCE_LOOKUP_H_
