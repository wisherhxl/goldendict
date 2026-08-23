// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_LINUX_SINGLE_INSTANCE_LOOKUP_H_
#define GOLDENDICT_LINUX_SINGLE_INSTANCE_LOOKUP_H_

#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

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

class LinuxSingleInstanceLookup final : public QObject {
    Q_OBJECT

   public:
    static constexpr int kForwardTimeoutMilliseconds = 10000;
    static constexpr qsizetype kMaximumPendingLookups = 32;

    explicit LinuxSingleInstanceLookup(QString endpoint,
                                       QObject* parent = nullptr);
    ~LinuxSingleInstanceLookup() override;

    LinuxSingleInstanceLookup(const LinuxSingleInstanceLookup&) = delete;
    LinuxSingleInstanceLookup& operator=(const LinuxSingleInstanceLookup&) =
        delete;

    SingleInstanceStartResult Start(const QString& normalized_lookup);
    void PublishConsumer(std::function<void(QString)> consumer);
    bool IsPrimary() const noexcept;

   private:
    bool Forward(const QString& normalized_lookup);
    void ReceiveConnections();
    void ReceiveMessage(QObject* connection);

    QString endpoint_;
    std::unique_ptr<QLockFile> lock_;
    std::unique_ptr<QLocalServer> server_;
    QList<QString> pending_;
    std::function<void(QString)> consumer_;
    bool primary_ = false;
};

QString LinuxSingleInstanceEndpoint();

}  // namespace goldendict::app
#endif  // defined(Q_OS_LINUX)

#endif  // GOLDENDICT_LINUX_SINGLE_INSTANCE_LOOKUP_H_
