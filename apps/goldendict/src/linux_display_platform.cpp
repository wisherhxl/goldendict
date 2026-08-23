// SPDX-License-Identifier: GPL-3.0-or-later

#include "linux_display_platform.h"

#if defined(Q_OS_LINUX)

#include <QByteArray>
#include <QString>

namespace goldendict::app {

void ConfigureLinuxDisplayPlatform() {
    if (qEnvironmentVariable("XDG_SESSION_TYPE")
            .compare(QStringLiteral("wayland"), Qt::CaseInsensitive) == 0) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
    }
}

}  // namespace goldendict::app

#endif  // defined(Q_OS_LINUX)
