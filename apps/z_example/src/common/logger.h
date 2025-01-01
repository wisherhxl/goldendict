// ------------------------------------------------------
//  Copyright (C) 2021 SHANGHAI INSTITUTE OF LASER TECHNOLOGY.
//                  - All Rights Reserved -
//
//  Unauthorized copying of this file, via any medium is strictly prohibited
//  Proprietary and confidential
//
//  Written by Xiling Huang <huangxiling@silt.top>
//  Created:     2021-03-17    16:39
// ------------------------------------------------------

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
