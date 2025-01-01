/*
 * Copyright (c) 2021 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2021-06-04
 */

#include "tiger/base/file_utils.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>

namespace ti {
void GetImageFilesInDirectory(const QString& dir_url, QStringList& file_list) {
    const QDir img_dir(dir_url);
    QStringList filters;
    filters << "*.jpg" << "*.bmp" << "*.png" << "*.tif";
    file_list =
        img_dir.entryList(filters, QDir::Filter::Files, QDir::SortFlag::Name);
}

void GetFilesInDirectory(const QString& dir_url, const QString& file_type,
                         QStringList& file_list) {
    const QDir img_dir(dir_url);
    qDebug() << img_dir;
    QStringList filters;
    filters << QString("*.").append(file_type);
    file_list =
        img_dir.entryList(filters, QDir::Filter::Files, QDir::SortFlag::Name);
    qDebug() << file_list;
}

QString GenerateFileNameByTime(const QString& prefix, const QString& suffix,
                               const QString& date_format) {
    QString name = prefix;
    name.append(QDateTime::currentDateTime().toString(date_format));
    name.append(suffix);
    return name;
}
}  // namespace ti
