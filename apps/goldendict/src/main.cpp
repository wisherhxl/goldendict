// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWebEngineUrlScheme>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

#include "goldendict/core/application.h"
#include "goldendict/core/favorites_store.h"
#include "goldendict/core/history_store.h"
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

FavoriteViewItem MakeFavoriteViewItem(
    const goldendict::core::FavoriteItem& item) {
    FavoriteViewItem view;
    view.text = QString::fromStdString(item.text);
    view.folder = item.kind == goldendict::core::FavoriteItemKind::kFolder;
    view.expanded = item.expanded;
    view.children.reserve(item.children.size());
    for (const auto& child : item.children) {
        view.children.push_back(MakeFavoriteViewItem(child));
    }
    return view;
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
    const QString legacy_configuration_path =
        QDir(configuration_directory).filePath(QStringLiteral("config"));
    const QString history_path =
        QDir(configuration_directory).filePath(QStringLiteral("history-v1"));
    const QString legacy_history_path =
        QDir(configuration_directory).filePath(QStringLiteral("history"));
    const QString favorites_path = QDir(configuration_directory)
                                       .filePath(QStringLiteral("favorites-v1"));
    const QString legacy_favorites_path =
        QDir(configuration_directory).filePath(QStringLiteral("favorites"));
    const std::string default_index_directory =
        QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
            .filePath(QStringLiteral("indexes"))
            .toStdString();
    goldendict::core::CoreConfiguration configuration;
    try {
        configuration = goldendict::core::LoadOrMigrateConfiguration(
            configuration_path.toStdString(),
            legacy_configuration_path.toStdString(), default_index_directory);
    } catch (const std::exception& error) {
        QMessageBox::warning(nullptr, QStringLiteral("GoldenDict"),
                             QString::fromLocal8Bit(error.what()));
    }
    if (configuration.index_directory.empty()) {
        configuration.index_directory = default_index_directory;
    }
    const QString command_line_root = DictionaryRootArgument(argc, argv);
    if (!command_line_root.isEmpty()) {
        configuration.dictionary_paths = {command_line_root.toStdString()};
    }

    auto facade = goldendict::core::CreateDesktopFacade(configuration);
    constexpr std::size_t kMaximumHistoryEntries = 500U;
    std::vector<goldendict::core::HistoryEntry> history;
    try {
        history = goldendict::core::LoadOrMigrateHistory(
            history_path.toStdString(), legacy_history_path.toStdString(),
            kMaximumHistoryEntries);
    } catch (const std::exception& error) {
        QMessageBox::warning(nullptr, QStringLiteral("GoldenDict history"),
                             QString::fromLocal8Bit(error.what()));
    }
    goldendict::core::Favorites favorites;
    try {
        favorites = goldendict::core::LoadOrMigrateFavorites(
            favorites_path.toStdString(), legacy_favorites_path.toStdString());
    } catch (const std::exception& error) {
        QMessageBox::warning(nullptr, QStringLiteral("GoldenDict favorites"),
                             QString::fromLocal8Bit(error.what()));
    }
    MainWindow window;
    window.SetFacade(facade.get());
    const auto refresh_history = [&window, &history]() {
        QStringList words;
        words.reserve(static_cast<qsizetype>(history.size()));
        for (const auto& entry : history) {
            words.push_back(QString::fromStdString(entry.word));
        }
        window.SetHistoryWords(words);
    };
    refresh_history();
    const auto refresh_favorites = [&window, &favorites]() {
        std::vector<FavoriteViewItem> items;
        items.reserve(favorites.size());
        for (const auto& favorite : favorites) {
            items.push_back(MakeFavoriteViewItem(favorite));
        }
        window.SetFavoriteItems(items);
    };
    refresh_favorites();
    QObject::connect(&window, &MainWindow::LookupSubmitted, &window,
                     [&](const QString& word) {
                         auto updated = history;
                         const std::string encoded = word.toStdString();
                         updated.erase(
                             std::remove_if(
                                 updated.begin(), updated.end(),
                                 [&word](const auto& entry) {
                                     return QString::fromStdString(entry.word)
                                                .compare(word,
                                                         Qt::CaseInsensitive) ==
                                            0;
                                 }),
                             updated.end());
                         updated.insert(updated.begin(), {0U, encoded});
                         if (updated.size() > kMaximumHistoryEntries) {
                             updated.resize(kMaximumHistoryEntries);
                         }
                         try {
                             goldendict::core::SaveHistory(
                                 history_path.toStdString(), updated);
                             history = std::move(updated);
                             refresh_history();
                         } catch (const std::exception& error) {
                             QMessageBox::warning(
                                 &window, QStringLiteral("GoldenDict history"),
                                 QString::fromLocal8Bit(error.what()));
                         }
                     });
    QObject::connect(&window, &MainWindow::AddFavoriteRequested, &window,
                     [&](const QString& word) {
                         const bool exists = std::any_of(
                             favorites.begin(), favorites.end(),
                             [&word](const auto& item) {
                                 return item.kind ==
                                            goldendict::core::FavoriteItemKind::
                                                kHeadword &&
                                        QString::fromStdString(item.text)
                                                .compare(word,
                                                         Qt::CaseInsensitive) ==
                                            0;
                             });
                         if (exists) {
                             refresh_favorites();
                             return;
                         }
                         auto updated = favorites;
                         updated.push_back(
                             {goldendict::core::FavoriteItemKind::kHeadword,
                              word.toStdString(), false, {}});
                         try {
                             goldendict::core::SaveFavorites(
                                 favorites_path.toStdString(), updated);
                             favorites = std::move(updated);
                             refresh_favorites();
                         } catch (const std::exception& error) {
                             QMessageBox::warning(
                                 &window,
                                 QStringLiteral("GoldenDict favorites"),
                                 QString::fromLocal8Bit(error.what()));
                         }
                     });
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
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--webengine-interaction-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        window.RunWebEngineInteractionCheck(
            [&app](bool passed) { app.exit(passed ? 0 : 1); });
    } else if (HasArgument(argc, argv, QStringLiteral("--history-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &history_path, &window]() {
            window.RunHistorySmokeCheck([&app, &history_path](bool passed) {
                try {
                    const auto persisted = goldendict::core::LoadHistory(
                        history_path.toStdString());
                    passed = passed && !persisted.empty() &&
                             persisted.front().word == "history-smoke-entry";
                } catch (const std::exception&) {
                    passed = false;
                }
                app.exit(passed ? 0 : 1);
            });
        });
    } else if (HasArgument(argc, argv, QStringLiteral("--favorites-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &favorites_path, &window]() {
            window.RunFavoritesSmokeCheck(
                [&app, &favorites_path](bool passed) {
                    try {
                        const auto persisted = goldendict::core::LoadFavorites(
                            favorites_path.toStdString());
                        passed = passed && !persisted.empty() &&
                                 persisted.back().text ==
                                     "favorites-smoke-entry";
                    } catch (const std::exception&) {
                        passed = false;
                    }
                    app.exit(passed ? 0 : 1);
                });
        });
    }

    return app.exec();
}
