// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QStyle>
#include <QToolBar>
#include <cstdio>

namespace goldendict::app::test {

// Invoked after normal startup and production callback wiring. The read phase
// never changes the choices: it verifies state loaded by a different process.
inline bool ViewPreferencesRestartSmoke(QMainWindow& window,
                                        const QString& phase, bool enabled) {
    auto* menu = window.findChild<QAction*>(QStringLiteral("toggleMenuBar"));
    auto* names =
        window.findChild<QAction*>(QStringLiteral("showDictBarNames"));
    auto* small =
        window.findChild<QAction*>(QStringLiteral("useSmallIconsInToolbars"));
    auto* button =
        window.findChild<QAction*>(QStringLiteral("menuButtonAction"));
    auto* navigation =
        window.findChild<QToolBar*>(QStringLiteral("navToolbar"));
    auto* dictionaries =
        window.findChild<QToolBar*>(QStringLiteral("dictionaryBar"));
    if (!menu || !names || !small || !button || !navigation || !dictionaries)
        return false;
    if (phase == QStringLiteral("write")) {
        menu->setChecked(!enabled);
        names->setChecked(enabled);
        small->setChecked(enabled);
    } else if (phase != QStringLiteral("read")) {
        return false;
    }
    const int extent = window.style()->pixelMetric(
        enabled ? QStyle::PM_SmallIconSize : QStyle::PM_ToolBarIconSize);
    const bool passed =
        menu->isChecked() == !enabled &&
        window.menuBar()->isVisible() == !enabled &&
        button->isVisible() == enabled && names->isChecked() == enabled &&
        dictionaries->toolButtonStyle() ==
            (enabled ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly) &&
        small->isChecked() == enabled &&
        navigation->iconSize() == QSize(extent, extent) &&
        dictionaries->iconSize() == QSize(extent, extent);
    if (!passed) {
        const auto* status =
            window.findChild<QLabel*>(QStringLiteral("statusText"));
        std::fprintf(
            stderr,
            "View restart phase=%s desired=%d menu=%d visible=%d button=%d "
            "names=%d mode=%d small=%d icon=%d expected=%d status=%s\n",
            phase.toUtf8().constData(), enabled, menu->isChecked(),
            window.menuBar()->isVisible(), button->isVisible(),
            names->isChecked(), int(dictionaries->toolButtonStyle()),
            small->isChecked(), navigation->iconSize().width(), extent,
            status ? status->text().toUtf8().constData() : "missing");
    }
    const QString capture = qEnvironmentVariable("GOLDENDICT_VIEW_CAPTURE_DIR");
    if (!capture.isEmpty()) {
        window.resize(800, 600);
        QApplication::processEvents();
        if (!QDir().mkpath(capture))
            return false;
        const QString stem = phase + (enabled ? QStringLiteral("-enabled")
                                              : QStringLiteral("-default"));
        if (!window.grab().save(
                QDir(capture).filePath(stem + QStringLiteral(".png"))))
            return false;
        QJsonObject metadata{
            {"passed", passed},
            {"qt", qVersion()},
            {"style_class", window.style()->metaObject()->className()},
            {"style_name", window.style()->objectName()},
            {"font", window.font().toString()},
            {"width", window.width()},
            {"height", window.height()},
            {"dpr", window.devicePixelRatioF()},
            {"icon_extent", extent}};
        QFile output(QDir(capture).filePath(stem + QStringLiteral(".json")));
        if (!output.open(QIODevice::WriteOnly))
            return false;
        const auto bytes = QJsonDocument(metadata).toJson();
        if (output.write(bytes) != bytes.size())
            return false;
    }
    return passed;
}

}  // namespace goldendict::app::test
