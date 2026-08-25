// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_APPS_GOLDENDICT_INTERFACE_TRANSLATIONS_H_
#define GOLDENDICT_APPS_GOLDENDICT_INTERFACE_TRANSLATIONS_H_

#include <QString>
#include <QTranslator>

class QCoreApplication;

namespace goldendict::app {

enum class InterfaceLocale { kEnglish, kRussian };

InterfaceLocale ResolveInterfaceLocale(const QString& interface_language,
                                       const QString& system_locale);
QString ApplicationLocaleDirectory();

class InterfaceTranslations final {
   public:
    InterfaceTranslations(QCoreApplication& application,
                          const QString& locale_directory,
                          const QString& interface_language,
                          const QString& system_locale);
    ~InterfaceTranslations();

    InterfaceTranslations(const InterfaceTranslations&) = delete;
    InterfaceTranslations& operator=(const InterfaceTranslations&) = delete;

    InterfaceLocale locale() const { return locale_; }

    bool application_catalog_loaded() const {
        return application_catalog_loaded_;
    }

    bool qt_catalog_loaded() const { return qt_catalog_loaded_; }

   private:
    QCoreApplication* application_;
    InterfaceLocale locale_;
    QTranslator application_translator_;
    QTranslator qtbase_translator_;
    QTranslator qt_translator_;
    bool application_catalog_loaded_ = false;
    bool qtbase_catalog_loaded_ = false;
    bool qt_catalog_loaded_ = false;
    bool qt_umbrella_catalog_loaded_ = false;
};

}  // namespace goldendict::app

#endif  // GOLDENDICT_APPS_GOLDENDICT_INTERFACE_TRANSLATIONS_H_
