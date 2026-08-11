// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QWebEngineUrlScheme>

#include <memory>
#include <stdexcept>

#include "goldendict/core/application.h"
#include "main_window.h"

namespace {

bool HasSmokeArgument(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--smoke")) {
            return true;
        }
    }
    return false;
}

bool HasArgument(int argc, char* argv[], const QString& expected) {
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == expected) {
            return true;
        }
    }
    return false;
}

QString DictionaryRootArgument(int argc, char* argv[]) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) ==
            QStringLiteral("--dictionary-root")) {
            return QString::fromLocal8Bit(argv[i + 1]);
        }
    }
    return {};
}

void RegisterArticleScheme() {
    QWebEngineUrlScheme scheme(QByteArrayLiteral("goldendict"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                    QWebEngineUrlScheme::LocalScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (HasSmokeArgument(argc, argv)) {
        return 0;
    }

    RegisterArticleScheme();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("GoldenDict"));
    QApplication::setOrganizationName(QStringLiteral("GoldenDict"));

    const QString configuration_directory =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString configuration_path =
        QDir(configuration_directory).filePath(QStringLiteral("core.conf"));
    goldendict::core::CoreConfiguration configuration;
    try {
        configuration = goldendict::core::LoadConfiguration(
            configuration_path.toStdString());
    } catch (const std::exception& error) {
        QMessageBox::warning(nullptr, QStringLiteral("GoldenDict"),
                             QString::fromLocal8Bit(error.what()));
    }
    if (configuration.index_directory.empty()) {
        configuration.index_directory = QDir(QStandardPaths::writableLocation(
                                                 QStandardPaths::CacheLocation))
                                            .filePath(QStringLiteral("indexes"))
                                            .toStdString();
    }
    const QString command_line_root = DictionaryRootArgument(argc, argv);
    if (!command_line_root.isEmpty()) {
        configuration.dictionary_paths = {command_line_root.toStdString()};
    }

    auto facade = goldendict::core::CreateDesktopFacade(configuration);
    MainWindow window;
    window.SetFacade(facade.get());
    QObject::connect(&window, &MainWindow::DictionaryDirectorySelected, &window,
                     [&](const QString& directory) {
                         auto updated = configuration;
                         updated.dictionary_paths = {directory.toStdString()};
                         try {
                             auto replacement =
                                 goldendict::core::CreateDesktopFacade(updated);
                             goldendict::core::SaveConfiguration(
                                 configuration_path.toStdString(), updated);
                             window.SetFacade(replacement.get());
                             facade = std::move(replacement);
                             configuration = std::move(updated);
                         } catch (const std::exception& error) {
                             QMessageBox::warning(
                                 &window, QStringLiteral("GoldenDict"),
                                 QString::fromLocal8Bit(error.what()));
                         }
                     });
    window.show();

    if (HasArgument(argc, argv, QStringLiteral("--webengine-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        window.RunWebEngineSmokeCheck(
            [&app](bool passed) { app.exit(passed ? 0 : 1); });
    }

    return app.exec();
}
