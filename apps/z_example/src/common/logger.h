/*
 * Copyright (c) 2021 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2021-03-17
 */

#pragma once

#include <log4cplus/logger.h>
#include <QDir>

namespace ti {

/**
 * @brief Outputs log messages using log4cplus based on Qt message types.
 *
 * This function maps Qt logging messages to log4cplus loggers and formats them
 * with relevant contextual information (file, line, function).
 *
 * @param type The type of Qt message (e.g., debug, info, warning).
 * @param context The context of the log message (file, line, function).
 * @param msg The log message string.
 */
void TigerLogOutput(QtMsgType type, const QMessageLogContext& context,
                    const QString& msg);
}  // namespace ti
