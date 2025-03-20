/*
 * Copyright (c) 2021 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2021-03-18
 */

#include <log4cplus/configurator.h>
#include <log4cplus/initializer.h>
#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QTranslator>

#include "common/logger.h"
#include "common/single_application.h"
#include "mainwindow.h"
#include "tiger/base.hpp"

// #include <QtPlugin>
// Q_IMPORT_PLUGIN (QWindowsIntegrationPlugin)

using namespace ti;

int main(int argc, char* argv[]) {
    try {
        // Initialization of log4cplus
        log4cplus::Initializer initializer;
        log4cplus::PropertyConfigurator log4c_configurator(
            LOG4CPLUS_TEXT("config/log4cplus.properties"));
        const auto log4c_properties = log4c_configurator.getProperties();
        bool log4c_configurator_status = true;
        if (log4c_properties.exists(LOG4CPLUS_TEXT("rootLogger")) &&
            log4c_properties.exists(LOG4CPLUS_TEXT("logger.error"))) {
            log4c_configurator.configure();
            qInstallMessageHandler(TigerLogOutput);
        } else {
            log4c_configurator_status = false;
        }

        // Initialization of Qt Application
        const SingleApplication s_app;
        QApplication a(argc, argv);
        QCoreApplication::setOrganizationName(TI_ORG_NAME);
        QCoreApplication::setApplicationName(TI_PROJECT_NAME);

        if (!log4c_configurator_status) {
            QMessageBox::warning(
                nullptr, QCoreApplication::applicationName(),
                QObject::tr("Unable to load log configuration file!"),
                QMessageBox::Ok);
            return 0;
        }

        qInfo() << QObject::tr("Application Started!");
        qInfo() << QObject::tr("Ver: ") << TI_VERSION;

        if (!s_app.isValid()) {
            QMessageBox::warning(
                nullptr, QCoreApplication::applicationName(),
                QObject::tr("This program only allows a single instance_!"),
                QMessageBox::Ok);
            qWarning() << QObject::tr("Trying to start multiple instances.");
            return 0;
        }

        // Splash
        const QPixmap pic("res/splash.png");
        auto* splash = new QSplashScreen(pic, Qt::WindowStaysOnTopHint);
        auto splash_font = splash->font();
        splash_font.setPixelSize(52);
        splash->setFont(splash_font);
        splash->show();

        MainWindow w(splash);
        w.setWindowTitle(TI_PROJECT_NAME);
        // w.setWindowFlag(Qt::WindowStaysOnBottomHint);
        // w.setWindowState(Qt::WindowFullScreen);
        w.show();
        QApplication::exec();
    } catch (...) {
        QApplication error_handling_app(argc, argv);
        QCoreApplication::setOrganizationName(TI_ORG_NAME);
        QCoreApplication::setApplicationName(TI_PROJECT_NAME);
        QTranslator translator;
        const auto translator_loaded = translator.load("lang/zh_cn.qm");
        if (translator_loaded) {
            qApp->installTranslator(&translator);
        }
        QMessageBox::critical(
            nullptr, QCoreApplication::applicationName(),
            QObject::tr(
                "Unknown exception encountered, the program must be closed."),
            QMessageBox::Ok);
        qCritical() << QObject::tr(
            "Unknown exception encountered, the program must be closed.");
    }
    return 0;
}
