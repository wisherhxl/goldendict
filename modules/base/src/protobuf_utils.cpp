/*
 * Copyright (c) 2023 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2023-08-21
 */

#include "tiger/base/protobuf_utils.h"

#include <QDebug>
#include <QDir>
#include <QSaveFile>
#include <QTextStream>
#include <fstream>
#include "google/protobuf/util/json_util.h"

namespace ti {

bool ProtobufUtil::writeMessageToJson(const google::protobuf::Message& msg,
                                      const QString& url) {
    QSaveFile target(url);
    const QFileInfo target_info(url);
    const auto target_dir = target_info.absoluteDir();
    if (!target_dir.mkpath(target_dir.absolutePath())) {
        qCritical() << QObject::tr("Cannot create dir at: %1")
                           .arg(target_dir.absolutePath());
        return false;
    }

    google::protobuf::util::JsonOptions options;
    options.add_whitespace = true;
    std::string msg_json;
    (void)MessageToJsonString(msg, &msg_json, options);

    if (!target.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << QObject::tr("Cannot open file for writing at: %1.")
                           .arg(target_info.absoluteFilePath());
        return false;
    }

    const auto msg_length = qstrlen(msg_json.c_str());
    const auto msg_written = target.write(msg_json.c_str());

    if (msg_length != msg_written) {
        qCritical() << QObject::tr("Writing pipe broken.");
        return false;
    }

    if (!target.commit()) {
        qCritical() << QObject::tr("File writing error at: %1.")
                           .arg(target_info.absoluteFilePath());
        return false;
    }

    qInfo()
        << QObject::tr("File wrote to: %1").arg(target_info.absoluteFilePath());
    return true;
}

bool ProtobufUtil::readMessageFromJson(google::protobuf::Message* msg,
                                       const QString& url) {
    QFile target(url);
    if (!target.exists()) {
        qCritical() << QObject::tr("File doesn't exist: %1.").arg(url);
        return false;
    }

    if (!target.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << QObject::tr("Cannot open file at: %1.").arg(url);
        return false;
    }

    QTextStream in(&target);
    const auto in_str = in.readAll().toStdString();

    google::protobuf::util::JsonParseOptions options;
    options.ignore_unknown_fields = true;
    (void)JsonStringToMessage(in_str, msg, options);

    target.close();
    qInfo() << QObject::tr("File loaded at: %1").arg(url);
    return true;
}

bool ProtobufUtil::writeMessageToBin(const google::protobuf::Message& msg,
                                     const QString& url) {
    const QFileInfo target_info(url);
    const auto target_dir = target_info.absoluteDir();
    if (!target_dir.mkpath(target_dir.absolutePath())) {
        qCritical() << QObject::tr("Cannot create dir at: %1")
                           .arg(target_dir.absolutePath());
        return false;
    }

    std::fstream output(url.toStdString(),
                        std::ios::out | std::ios::trunc | std::ios::binary);

    if (!msg.SerializeToOstream(&output)) {
        qCritical() << QObject::tr("Failed to write file: %1.").arg(url);
        return false;
    }

    qInfo() << QObject::tr("File wrote to: %1").arg(url);
    return true;
}

bool ProtobufUtil::readMessageFromBin(google::protobuf::Message* msg,
                                      const QString& url) {
    std::fstream input(url.toStdString(), std::ios::in | std::ios::binary);

    if (!input) {
        qCritical() << QObject::tr("File doesn't exist: %1.").arg(url);
        return false;
    }

    if (!msg->ParseFromIstream(&input)) {
        qCritical() << QObject::tr("Failed to parse file: %1.").arg(url);
        return false;
    }

    qInfo() << QObject::tr("File loaded at: %1").arg(url);
    return true;
}

}  // namespace ti
