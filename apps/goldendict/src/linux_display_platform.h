// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_LINUX_DISPLAY_PLATFORM_H_
#define GOLDENDICT_LINUX_DISPLAY_PLATFORM_H_

#include <QtSystemDetection>

#if defined(Q_OS_LINUX)
namespace goldendict::app {

void ConfigureLinuxDisplayPlatform();

}  // namespace goldendict::app
#endif  // defined(Q_OS_LINUX)

#endif  // GOLDENDICT_LINUX_DISPLAY_PLATFORM_H_
