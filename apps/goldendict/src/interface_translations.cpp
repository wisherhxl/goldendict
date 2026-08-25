// SPDX-License-Identifier: GPL-3.0-or-later

#include "interface_translations.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocale>

namespace goldendict::app {
namespace {

bool IsRussian(const QString& locale_name) {
    if (locale_name.isEmpty())
        return false;
    const QLocale locale(locale_name);
    return locale.language() == QLocale::Russian;
}

}  // namespace

InterfaceLocale ResolveInterfaceLocale(const QString& interface_language,
                                       const QString& system_locale) {
    const QString selected =
        interface_language.isEmpty() ? system_locale : interface_language;
    return IsRussian(selected) ? InterfaceLocale::kRussian
                               : InterfaceLocale::kEnglish;
}

QString ApplicationLocaleDirectory() {
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../share/goldendict/locale"));
}

InterfaceTranslations::InterfaceTranslations(QCoreApplication& application,
                                             const QString& locale_directory,
                                             const QString& interface_language,
                                             const QString& system_locale)
    : application_(&application),
      locale_(ResolveInterfaceLocale(interface_language, system_locale)) {
    if (locale_ != InterfaceLocale::kRussian)
        return;

    application_catalog_loaded_ =
        application_translator_.load(QStringLiteral("ru_RU"), locale_directory);
    if (application_catalog_loaded_)
        application_->installTranslator(&application_translator_);

    qtbase_catalog_loaded_ =
        qtbase_translator_.load(QStringLiteral("qtbase_ru"), locale_directory);
    if (qtbase_catalog_loaded_)
        application_->installTranslator(&qtbase_translator_);

    qt_catalog_loaded_ = qtbase_catalog_loaded_;
    qt_umbrella_catalog_loaded_ =
        qt_translator_.load(QStringLiteral("qt_ru"), locale_directory);
    if (qt_umbrella_catalog_loaded_)
        application_->installTranslator(&qt_translator_);
}

InterfaceTranslations::~InterfaceTranslations() {
    if (qt_umbrella_catalog_loaded_)
        application_->removeTranslator(&qt_translator_);
    if (qtbase_catalog_loaded_)
        application_->removeTranslator(&qtbase_translator_);
    if (application_catalog_loaded_)
        application_->removeTranslator(&application_translator_);
}

}  // namespace goldendict::app
