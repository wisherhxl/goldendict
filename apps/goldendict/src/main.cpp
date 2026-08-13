// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QDir>
#include <QFile>
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
    const goldendict::core::FavoriteItem& item, QList<int> path) {
    FavoriteViewItem view;
    view.text = QString::fromStdString(item.text);
    view.folder = item.kind == goldendict::core::FavoriteItemKind::kFolder;
    view.expanded = item.expanded;
    view.path = path;
    view.children.reserve(item.children.size());
    for (std::size_t index = 0; index < item.children.size(); ++index) {
        auto child_path = path;
        child_path.push_back(static_cast<int>(index));
        view.children.push_back(
            MakeFavoriteViewItem(item.children[index], std::move(child_path)));
    }
    return view;
}

bool RemoveFavoriteAtPath(goldendict::core::Favorites* favorites,
                          const QList<int>& path) {
    if (favorites == nullptr || path.empty()) {
        return false;
    }
    auto* items = favorites;
    for (qsizetype depth = 0; depth + 1 < path.size(); ++depth) {
        const int index = path[depth];
        if (index < 0 || static_cast<std::size_t>(index) >= items->size()) {
            return false;
        }
        auto& item = (*items)[static_cast<std::size_t>(index)];
        if (item.kind != goldendict::core::FavoriteItemKind::kFolder) {
            return false;
        }
        items = &item.children;
    }
    const int index = path.back();
    if (index < 0 || static_cast<std::size_t>(index) >= items->size()) {
        return false;
    }
    items->erase(items->begin() + index);
    return true;
}

goldendict::core::Favorites* FavoriteContainerAtPath(
    goldendict::core::Favorites* favorites, const QList<int>& path) {
    if (favorites == nullptr) {
        return nullptr;
    }
    auto* items = favorites;
    for (const int index : path) {
        if (index < 0 || static_cast<std::size_t>(index) >= items->size()) {
            return nullptr;
        }
        auto& item = (*items)[static_cast<std::size_t>(index)];
        if (item.kind != goldendict::core::FavoriteItemKind::kFolder) {
            return nullptr;
        }
        item.expanded = true;
        items = &item.children;
    }
    return items;
}

bool RenameFavoriteAtPath(goldendict::core::Favorites* favorites,
                          const QList<int>& path, std::string name) {
    if (path.empty()) {
        return false;
    }
    auto parent_path = path;
    const int index = parent_path.takeLast();
    auto* items = FavoriteContainerAtPath(favorites, parent_path);
    if (items == nullptr || index < 0 ||
        static_cast<std::size_t>(index) >= items->size()) {
        return false;
    }
    (*items)[static_cast<std::size_t>(index)].text = std::move(name);
    return true;
}

bool MoveFavoriteAtPath(goldendict::core::Favorites* favorites,
                        const QList<int>& path, int offset) {
    if (path.empty() || (offset != -1 && offset != 1)) {
        return false;
    }
    auto parent_path = path;
    const int index = parent_path.takeLast();
    auto* items = FavoriteContainerAtPath(favorites, parent_path);
    const int destination = index + offset;
    if (items == nullptr || index < 0 || destination < 0 ||
        static_cast<std::size_t>(index) >= items->size() ||
        static_cast<std::size_t>(destination) >= items->size()) {
        return false;
    }
    std::iter_swap(items->begin() + index, items->begin() + destination);
    return true;
}

bool MoveFavoriteToRoot(goldendict::core::Favorites* favorites,
                        const QList<int>& path) {
    if (favorites == nullptr || path.size() < 2) {
        return false;
    }
    auto parent_path = path;
    const int index = parent_path.takeLast();
    auto* source = FavoriteContainerAtPath(favorites, parent_path);
    if (source == nullptr || index < 0 ||
        static_cast<std::size_t>(index) >= source->size()) {
        return false;
    }
    auto item = std::move((*source)[static_cast<std::size_t>(index)]);
    source->erase(source->begin() + index);
    favorites->push_back(std::move(item));
    return true;
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
    const QString favorites_path =
        QDir(configuration_directory).filePath(QStringLiteral("favorites-v1"));
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
    if (configuration.article_tab_session.has_value() &&
        !facade->RestoreArticleTabSession(*configuration.article_tab_session)) {
        QMessageBox::warning(
            nullptr, QStringLiteral("GoldenDict"),
            QStringLiteral("Unable to restore the saved article tab session"));
    }
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
    window.SetDictionaryGroups(configuration.dictionary_groups);
    window.SetFacade(facade.get());
    const auto persist_article_tab_session = [&]() {
        auto updated = configuration;
        updated.article_tab_session = facade->ExportArticleTabSession();
        try {
            goldendict::core::SaveConfiguration(
                configuration_path.toStdString(), updated);
            configuration = std::move(updated);
        } catch (const std::exception& error) {
            QMessageBox::warning(&window, QStringLiteral("GoldenDict"),
                                 QString::fromLocal8Bit(error.what()));
        }
    };
    QObject::connect(&window, &MainWindow::ArticleTabSessionMutated, &window,
                     persist_article_tab_session);
    QObject::connect(&app, &QApplication::aboutToQuit, &window,
                     persist_article_tab_session);
    const auto refresh_history = [&window, &history]() {
        std::vector<HistoryViewItem> items;
        items.reserve(history.size());
        for (const auto& entry : history) {
            items.push_back(
                {QString::fromStdString(entry.word), entry.group_id});
        }
        window.SetHistoryItems(items);
    };
    refresh_history();
    const auto refresh_favorites = [&window, &favorites]() {
        std::vector<FavoriteViewItem> items;
        items.reserve(favorites.size());
        for (std::size_t index = 0; index < favorites.size(); ++index) {
            items.push_back(MakeFavoriteViewItem(favorites[index],
                                                 {static_cast<int>(index)}));
        }
        window.SetFavoriteItems(items);
    };
    refresh_favorites();
    QObject::connect(
        &window, &MainWindow::LookupSubmitted, &window,
        [&](const QString& word, std::uint32_t group_id) {
            auto updated = history;
            const std::string encoded = word.toStdString();
            updated.erase(std::remove_if(
                              updated.begin(), updated.end(),
                              [&word](const auto& entry) {
                                  return QString::fromStdString(entry.word)
                                             .compare(word,
                                                      Qt::CaseInsensitive) == 0;
                              }),
                          updated.end());
            updated.insert(updated.begin(), {group_id, encoded});
            if (updated.size() > kMaximumHistoryEntries) {
                updated.resize(kMaximumHistoryEntries);
            }
            try {
                goldendict::core::SaveHistory(history_path.toStdString(),
                                              updated);
                history = std::move(updated);
                refresh_history();
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict history"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(&window, &MainWindow::RemoveFavoriteRequested, &window,
                     [&](const QList<int>& path) {
                         auto updated = favorites;
                         if (!RemoveFavoriteAtPath(&updated, path)) {
                             refresh_favorites();
                             return;
                         }
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
    QObject::connect(
        &window, &MainWindow::ClearHistoryRequested, &window, [&]() {
            try {
                const std::vector<goldendict::core::HistoryEntry> empty;
                goldendict::core::SaveHistory(history_path.toStdString(),
                                              empty);
                history.clear();
                refresh_history();
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict history"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(
        &window, &MainWindow::ImportHistoryRequested, &window,
        [&](const QString& path, std::uint32_t group_id) {
            try {
                auto imported = goldendict::core::ImportHistoryText(
                    path.toStdString(), kMaximumHistoryEntries, group_id);
                goldendict::core::SaveHistory(history_path.toStdString(),
                                              imported);
                history = std::move(imported);
                refresh_history();
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict history"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(
        &window, &MainWindow::AddFavoriteRequested, &window,
        [&](const QString& word, const QList<int>& parent_path) {
            auto updated = favorites;
            auto* target = FavoriteContainerAtPath(&updated, parent_path);
            if (target == nullptr) {
                refresh_favorites();
                return;
            }
            const bool exists = std::any_of(
                target->begin(), target->end(), [&word](const auto& item) {
                    return item.kind ==
                               goldendict::core::FavoriteItemKind::kHeadword &&
                           QString::fromStdString(item.text).compare(
                               word, Qt::CaseInsensitive) == 0;
                });
            if (exists) {
                refresh_favorites();
                return;
            }
            target->push_back({goldendict::core::FavoriteItemKind::kHeadword,
                               word.toStdString(),
                               false,
                               {}});
            try {
                goldendict::core::SaveFavorites(favorites_path.toStdString(),
                                                updated);
                favorites = std::move(updated);
                refresh_favorites();
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict favorites"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(
        &window, &MainWindow::AddFavoriteFolderRequested, &window,
        [&](const QString& name, const QList<int>& parent_path) {
            auto updated = favorites;
            auto* target = FavoriteContainerAtPath(&updated, parent_path);
            if (target == nullptr) {
                refresh_favorites();
                return;
            }
            target->push_back({goldendict::core::FavoriteItemKind::kFolder,
                               name.toStdString(),
                               true,
                               {}});
            try {
                goldendict::core::SaveFavorites(favorites_path.toStdString(),
                                                updated);
                favorites = std::move(updated);
                refresh_favorites();
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict favorites"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(
        &window, &MainWindow::RenameFavoriteRequested, &window,
        [&](const QList<int>& path, const QString& name) {
            auto updated = favorites;
            if (!RenameFavoriteAtPath(&updated, path, name.toStdString())) {
                refresh_favorites();
                return;
            }
            try {
                goldendict::core::SaveFavorites(favorites_path.toStdString(),
                                                updated);
                favorites = std::move(updated);
                refresh_favorites();
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict favorites"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(&window, &MainWindow::MoveFavoriteRequested, &window,
                     [&](const QList<int>& path, int offset) {
                         auto updated = favorites;
                         if (!MoveFavoriteAtPath(&updated, path, offset)) {
                             refresh_favorites();
                             return;
                         }
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
    QObject::connect(&window, &MainWindow::MoveFavoriteToRootRequested, &window,
                     [&](const QList<int>& path) {
                         auto updated = favorites;
                         if (!MoveFavoriteToRoot(&updated, path)) {
                             refresh_favorites();
                             return;
                         }
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
    QObject::connect(
        &window, &MainWindow::ImportFavoritesRequested, &window,
        [&](const QString& path) {
            try {
                auto imported =
                    goldendict::core::ImportFavoritesXml(path.toStdString());
                goldendict::core::SaveFavorites(favorites_path.toStdString(),
                                                imported);
                favorites = std::move(imported);
                refresh_favorites();
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict favorites"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(&window, &MainWindow::ExportFavoritesRequested, &window,
                     [&](const QString& path) {
                         try {
                             goldendict::core::ExportFavoritesXml(
                                 path.toStdString(), favorites);
                         } catch (const std::exception& error) {
                             QMessageBox::warning(
                                 &window,
                                 QStringLiteral("GoldenDict favorites"),
                                 QString::fromLocal8Bit(error.what()));
                         }
                     });
    QObject::connect(
        &window, &MainWindow::DictionaryDirectorySelected, &window,
        [&](const QString& directory) {
            auto updated = configuration;
            updated.dictionary_paths = {directory.toStdString()};
            updated.article_tab_session = facade->ExportArticleTabSession();
            try {
                auto replacement =
                    goldendict::core::CreateDesktopFacade(updated);
                if (!replacement->RestoreArticleTabSession(
                        *updated.article_tab_session)) {
                    throw std::runtime_error(
                        "Unable to restore the article tab "
                        "session");
                }
                goldendict::core::SaveConfiguration(
                    configuration_path.toStdString(), updated);
                window.SetFacade(replacement.get());
                facade = std::move(replacement);
                configuration = std::move(updated);
            } catch (const std::exception& error) {
                QMessageBox::warning(&window, QStringLiteral("GoldenDict"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(
        &window, &MainWindow::DictionaryGroupsEdited, &window, [&]() {
            auto updated = configuration;
            updated.dictionary_groups = window.DictionaryGroups();
            updated.article_tab_session = facade->ExportArticleTabSession();
            try {
                auto replacement =
                    goldendict::core::CreateDesktopFacade(updated);
                if (!replacement->RestoreArticleTabSession(
                        *updated.article_tab_session)) {
                    throw std::runtime_error(
                        "Unable to restore the article tab "
                        "session");
                }
                goldendict::core::SaveConfiguration(
                    configuration_path.toStdString(), updated);
                window.SetDictionaryGroups(updated.dictionary_groups);
                window.SetFacade(replacement.get());
                facade = std::move(replacement);
                configuration = std::move(updated);
            } catch (const std::exception& error) {
                window.SetDictionaryGroups(configuration.dictionary_groups);
                QMessageBox::warning(&window,
                                     QStringLiteral("Dictionary Groups"),
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
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--article-tabs-smoke"))) {
        QTimer::singleShot(15000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunArticleTabsSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--article-tab-session-restart-prepare"))) {
        QTimer::singleShot(15000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunArticleTabSessionRestartSmokeCheck(
                true, [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--article-tab-session-restart-verify"))) {
        QTimer::singleShot(15000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunArticleTabSessionRestartSmokeCheck(
                false, [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv, QStringLiteral("--history-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &history_path, &window]() {
            window.RunHistorySmokeCheck([&app, &history_path](bool passed) {
                try {
                    const auto persisted = goldendict::core::LoadHistory(
                        history_path.toStdString());
                    passed = passed && !persisted.empty() &&
                             persisted.front().word == "history-smoke-entry" &&
                             persisted.front().group_id == 7U;
                } catch (const std::exception&) {
                    passed = false;
                }
                app.exit(passed ? 0 : 1);
            });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--dictionary-groups-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration, &configuration_path, &window]() {
                window.RunDictionaryGroupsSmokeCheck(
                    [&app, &configuration, &configuration_path](bool passed) {
                        try {
                            const auto persisted =
                                goldendict::core::LoadConfiguration(
                                    configuration_path.toStdString());
                            passed = passed &&
                                     persisted.dictionary_groups.size() == 1U &&
                                     persisted.dictionary_groups.front().id == 7U &&
                                     persisted.dictionary_paths ==
                                         configuration.dictionary_paths &&
                                     persisted.index_directory ==
                                         configuration.index_directory &&
                                     persisted.sound_directories ==
                                         configuration.sound_directories;
                            auto invalid = persisted;
                            invalid.dictionary_groups.front().name.clear();
                            try {
                                goldendict::core::SaveConfiguration(
                                    configuration_path.toStdString(), invalid);
                                passed = false;
                            } catch (const std::exception&) {
                            }
                            passed = passed &&
                                     goldendict::core::LoadConfiguration(
                                         configuration_path.toStdString())
                                             .dictionary_groups ==
                                         persisted.dictionary_groups;
                        } catch (const std::exception&) {
                            passed = false;
                        }
                        app.exit(passed ? 0 : 1);
                    });
            });
    } else if (HasArgument(argc, argv, QStringLiteral("--favorites-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &favorites_path, &window]() {
            window.RunFavoritesSmokeCheck([&app, &favorites_path](bool passed) {
                try {
                    const auto persisted = goldendict::core::LoadFavorites(
                        favorites_path.toStdString());
                    passed = passed && persisted.empty();
                } catch (const std::exception&) {
                    passed = false;
                }
                app.exit(passed ? 0 : 1);
            });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--favorites-transfer-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration_directory, &favorites_path, &window]() {
                QDir().mkpath(configuration_directory);
                const QString transfer_path =
                    QDir(configuration_directory)
                        .filePath(QStringLiteral("favorites-transfer.xml"));
                window.RunFavoritesTransferSmokeCheck(
                    transfer_path, [&app, &favorites_path](bool passed) {
                        try {
                            const auto persisted =
                                goldendict::core::LoadFavorites(
                                    favorites_path.toStdString());
                            passed =
                                passed &&
                                std::any_of(
                                    persisted.begin(), persisted.end(),
                                    [](const auto& item) {
                                        return item.kind ==
                                                   goldendict::core::
                                                       FavoriteItemKind::
                                                           kFolder &&
                                               item.text == "Transfer Folder";
                                    });
                        } catch (const std::exception&) {
                            passed = false;
                        }
                        app.exit(passed ? 0 : 1);
                    });
            });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--dictionary-browser-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunDictionaryBrowserSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--dictionary-browser-export-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window, [&app, &configuration_directory, &window]() {
                QDir().mkpath(configuration_directory);
                const QString export_path =
                    QDir(configuration_directory)
                        .filePath(QStringLiteral("headwords-export.txt"));
                window.RunDictionaryBrowserExportSmokeCheck(
                    export_path,
                    [&app](bool passed) { app.exit(passed ? 0 : 1); });
            });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--history-management-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &history_path, &window]() {
            window.RunHistoryManagementSmokeCheck(
                [&app, &history_path](bool passed) {
                    try {
                        passed = passed && goldendict::core::LoadHistory(
                                               history_path.toStdString())
                                               .empty();
                    } catch (const std::exception&) {
                        passed = false;
                    }
                    app.exit(passed ? 0 : 1);
                });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--history-export-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window, [&app, &configuration_directory, &window]() {
                QDir().mkpath(configuration_directory);
                window.RunHistoryExportSmokeCheck(
                    QDir(configuration_directory)
                        .filePath(QStringLiteral("history-export-smoke.txt")),
                    [&app](bool passed) { app.exit(passed ? 0 : 1); });
            });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--history-import-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration_directory, &history_path, &window]() {
                QDir().mkpath(configuration_directory);
                const QString import_path =
                    QDir(configuration_directory)
                        .filePath(QStringLiteral("history-import-smoke.txt"));
                QFile fixture(import_path);
                const bool prepared =
                    fixture.open(QIODevice::WriteOnly) &&
                    fixture.write(QByteArray::fromHex("efbbbf") +
                                  " Alpha \r\n第二个\n") > 0;
                fixture.close();
                window.RunHistoryImportSmokeCheck(
                    import_path, [&app, &history_path, prepared](bool passed) {
                        try {
                            const auto persisted =
                                goldendict::core::LoadHistory(
                                    history_path.toStdString());
                            passed = prepared && passed &&
                                     persisted.size() == 2U &&
                                     persisted[0].word == "Alpha" &&
                                     persisted[1].word == "第二个";
                        } catch (const std::exception&) {
                            passed = false;
                        }
                        app.exit(passed ? 0 : 1);
                    });
            });
    }

    return app.exec();
}
