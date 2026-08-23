// SPDX-License-Identifier: GPL-3.0-or-later

#include "linux_single_instance_lookup.h"

#if defined(Q_OS_LINUX)

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <utility>

#include "command_line_lookup.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::app {
namespace {

constexpr quint32 kProtocolMagic = 0x47444C49U;
constexpr quint16 kProtocolVersion = 1U;
constexpr char kAcknowledgement[] = "accepted";
constexpr qsizetype kHeaderSize =
    sizeof(quint32) + sizeof(quint16) + sizeof(quint32);
constexpr qsizetype kMaximumFrameSize =
    kHeaderSize + goldendict::core::kMaximumLookupTextBytes;

QByteArray Encode(const QString& lookup) {
    const QByteArray payload = lookup.toUtf8();
    QByteArray frame;
    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << kProtocolMagic << kProtocolVersion
           << static_cast<quint32>(payload.size());
    stream.writeRawData(payload.constData(), payload.size());
    return frame;
}

}  // namespace

LinuxSingleInstanceLookup::LinuxSingleInstanceLookup(QString endpoint,
                                                     QObject* parent)
    : QObject(parent),
      endpoint_(std::move(endpoint)),
      lock_(std::make_unique<QLockFile>(endpoint_ + QStringLiteral(".lock"))),
      server_(std::make_unique<QLocalServer>()) {}

LinuxSingleInstanceLookup::~LinuxSingleInstanceLookup() {
    if (primary_) {
        server_->close();
        QLocalServer::removeServer(endpoint_);
    }
}

SingleInstanceStartResult LinuxSingleInstanceLookup::Start(
    const QString& normalized_lookup) {
    if (lock_->tryLock(0)) {
        QLocalServer::removeServer(endpoint_);
        server_->setSocketOptions(QLocalServer::UserAccessOption);
        if (!server_->listen(endpoint_)) {
            lock_->unlock();
            return SingleInstanceStartResult::kForwardFailed;
        }
        primary_ = true;
        connect(server_.get(), &QLocalServer::newConnection, this,
                &LinuxSingleInstanceLookup::ReceiveConnections);
        return SingleInstanceStartResult::kPrimary;
    }
    if (lock_->error() != QLockFile::LockFailedError) {
        return SingleInstanceStartResult::kForwardFailed;
    }
    if (normalized_lookup.isEmpty()) {
        return SingleInstanceStartResult::kSecondaryNoOp;
    }
    return Forward(normalized_lookup)
               ? SingleInstanceStartResult::kForwarded
               : SingleInstanceStartResult::kForwardFailed;
}

void LinuxSingleInstanceLookup::PublishConsumer(
    std::function<void(QString)> consumer) {
    if (!primary_ || consumer_ || !consumer) {
        return;
    }
    consumer_ = std::move(consumer);
    const QList<QString> pending = std::exchange(pending_, {});
    for (const auto& lookup : pending) {
        consumer_(lookup);
    }
}

bool LinuxSingleInstanceLookup::IsPrimary() const noexcept {
    return primary_;
}

bool LinuxSingleInstanceLookup::Forward(const QString& normalized_lookup) {
    if (!IsNormalizedLookupText(normalized_lookup)) {
        return false;
    }
    QLocalSocket socket;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < kForwardTimeoutMilliseconds) {
        socket.connectToServer(endpoint_);
        const int remaining =
            kForwardTimeoutMilliseconds - static_cast<int>(timer.elapsed());
        if (socket.waitForConnected(std::min(remaining, 250))) {
            break;
        }
        socket.abort();
        QThread::msleep(50);
    }
    if (socket.state() != QLocalSocket::ConnectedState) {
        return false;
    }
    const QByteArray frame = Encode(normalized_lookup);
    if (socket.write(frame) != frame.size() ||
        !socket.waitForBytesWritten(kForwardTimeoutMilliseconds) ||
        !socket.waitForReadyRead(kForwardTimeoutMilliseconds)) {
        return false;
    }
    return socket.readAll() == QByteArray(kAcknowledgement);
}

void LinuxSingleInstanceLookup::ReceiveConnections() {
    while (server_->hasPendingConnections()) {
        auto* socket = server_->nextPendingConnection();
        if (socket == nullptr) {
            continue;
        }
        connect(socket, &QLocalSocket::readyRead, this,
                [this, socket]() { ReceiveMessage(socket); });
        connect(socket, &QLocalSocket::disconnected, socket,
                &QObject::deleteLater);
    }
}

void LinuxSingleInstanceLookup::ReceiveMessage(QObject* connection) {
    auto* socket = qobject_cast<QLocalSocket*>(connection);
    if (socket == nullptr || socket->property("lookupAccepted").toBool()) {
        return;
    }
    QByteArray bytes = socket->property("lookupFrame").toByteArray();
    bytes.append(socket->readAll());
    socket->setProperty("lookupFrame", bytes);
    if (bytes.size() < kHeaderSize) {
        return;
    }
    if (bytes.size() > kMaximumFrameSize) {
        socket->disconnectFromServer();
        return;
    }
    QDataStream stream(bytes);
    stream.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0;
    quint16 version = 0;
    quint32 payload_size = 0;
    stream >> magic >> version >> payload_size;
    const qsizetype expected = kHeaderSize + payload_size;
    if (magic != kProtocolMagic || version != kProtocolVersion ||
        payload_size > goldendict::core::kMaximumLookupTextBytes ||
        expected > kMaximumFrameSize) {
        socket->disconnectFromServer();
        return;
    }
    if (bytes.size() < expected) {
        return;
    }
    if (bytes.size() != expected) {
        socket->disconnectFromServer();
        return;
    }
    QByteArray payload(payload_size, Qt::Uninitialized);
    if (stream.readRawData(payload.data(), payload.size()) != payload.size()) {
        socket->disconnectFromServer();
        return;
    }
    const QString lookup = QString::fromUtf8(payload);
    if (lookup.toUtf8() != payload || !IsNormalizedLookupText(lookup) ||
        (!consumer_ && pending_.size() >= kMaximumPendingLookups)) {
        socket->disconnectFromServer();
        return;
    }
    if (consumer_) {
        consumer_(lookup);
    } else {
        pending_.push_back(lookup);
    }
    socket->setProperty("lookupAccepted", true);
    socket->write(kAcknowledgement);
    socket->flush();
    socket->disconnectFromServer();
}

QString LinuxSingleInstanceEndpoint() {
    const QString runtime =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        return {};
    }
    const QString directory =
        QDir(runtime).filePath(QStringLiteral("goldendict"));
    if (!QDir().mkpath(directory)) {
        return {};
    }
    const QByteArray profile =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
            .toUtf8();
    const QString profile_id = QString::fromLatin1(
        QCryptographicHash::hash(profile, QCryptographicHash::Sha256)
            .toHex()
            .left(16));
    return QDir(directory).filePath(QStringLiteral("lookup-instance-") +
                                    profile_id);
}

}  // namespace goldendict::app

#endif  // defined(Q_OS_LINUX)
