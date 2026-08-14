// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
#include "goldendict/network/runtime_composition.h"
#include "legacy_configuration_location.h"
#include "main_window.h"

namespace {

void ReportRuntimeCompositionDiagnostics(
    const std::vector<goldendict::network::RuntimeCompositionDiagnostic>&
        diagnostics) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.code ==
            goldendict::network::RuntimeCompositionDiagnosticCode::
                kMissingForvoCredential) {
            qWarning().noquote()
                << QStringLiteral(
                       "Forvo source '%1' is enabled but has no in-memory "
                       "credential; the source was not activated")
                       .arg(QString::fromStdString(diagnostic.source_id));
        }
    }
}

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

bool MakeFavoritePath(const QList<int>& path,
                      goldendict::core::FavoritePath* converted) {
    converted->clear();
    converted->reserve(static_cast<std::size_t>(path.size()));
    for (const int index : path) {
        if (index < 0) {
            return false;
        }
        converted->push_back(static_cast<std::size_t>(index));
    }
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

goldendict::app::DesktopPlatform CurrentDesktopPlatform() {
#if defined(Q_OS_WIN)
    return goldendict::app::DesktopPlatform::kWindows;
#elif defined(Q_OS_MACOS)
    return goldendict::app::DesktopPlatform::kMacOS;
#else
    return goldendict::app::DesktopPlatform::kLinuxUnix;
#endif
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

    const QString standard_configuration_directory =
        QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString current_configuration_directory =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString generic_data_directory =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const goldendict::app::LegacyConfigurationEnvironment location_environment{
        CurrentDesktopPlatform(),
        QDir::homePath().toStdString(),
        standard_configuration_directory.toStdString(),
        QCoreApplication::applicationDirPath().toStdString(),
        qEnvironmentVariable("APPDATA").toStdString(),
        generic_data_directory.toStdString(),
        current_configuration_directory.toStdString()};
    goldendict::app::ConfigurationLocations configuration_locations;
    try {
        configuration_locations =
            goldendict::app::ResolveConfigurationLocations(
                location_environment, goldendict::app::ProbePath);
        goldendict::app::ValidateAutoDiscoveredLegacyConfiguration(
            configuration_locations, goldendict::app::ProbePath);
    } catch (const std::exception& error) {
        QMessageBox::warning(nullptr, QStringLiteral("GoldenDict"),
                             QString::fromLocal8Bit(error.what()));
        return 1;
    }
    const QString configuration_path = QString::fromStdString(
        configuration_locations.current_configuration_path.string());
    const QString legacy_configuration_path = QString::fromStdString(
        configuration_locations.legacy_configuration_path.string());
    const QString configuration_directory =
        QFileInfo(configuration_path).absolutePath();
    const QString history_path = QString::fromStdString(
        configuration_locations.current_history_path.string());
    const QString legacy_history_path = QString::fromStdString(
        configuration_locations.legacy_history_path.string());
    const QString favorites_path = QString::fromStdString(
        configuration_locations.current_favorites_path.string());
    const QString legacy_favorites_path = QString::fromStdString(
        configuration_locations.legacy_favorites_path.string());
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
    if (HasArgument(argc, argv, QStringLiteral("--source-directories-smoke"))) {
        configuration.mediawiki_sources = {
            {"smoke.wiki", "Smoke Wiki", false, "https://wiki.example.test/w"}};
        configuration.website_sources = {
            {"smoke.website", "Smoke Website", false,
             "https://website.example.test/?q=%GDWORD%"}};
        configuration.forvo_sources = {{"smoke.forvo",
                                        "Smoke Forvo",
                                        false,
                                        "https://apifree.forvo.com",
                                        {"en", "ru"}}};
        configuration.dict_server_sources = {{"smoke.dict", "Smoke DICT", false,
                                              "dict.example.test", 2628U, "*",
                                              "prefix"}};
        configuration.external_program_sources = {
            {"smoke.external",
             "Smoke External",
             true,
             goldendict::core::ExternalProgramOutputKind::kPlainText,
             QCoreApplication::applicationFilePath().toStdString(),
             {"--smoke", "%GDWORD%"},
             ""}};
    }

    auto application_composition =
        goldendict::network::ComposeConfiguredApplication(configuration);
    ReportRuntimeCompositionDiagnostics(application_composition.diagnostics);
    auto facade = std::move(application_composition.facade);
    auto composition_diagnostics =
        std::move(application_composition.diagnostics);
    if (configuration.article_tab_session.has_value() &&
        !facade->RestoreArticleTabSession(*configuration.article_tab_session)) {
        QMessageBox::warning(
            nullptr, QStringLiteral("GoldenDict"),
            QStringLiteral("Unable to restore the saved article tab session"));
    }
    constexpr std::size_t kMaximumHistoryEntries = 500U;
    std::vector<goldendict::core::HistoryEntry> history;
    try {
        goldendict::app::ValidateAutoDiscoveredLegacyHistory(
            configuration_locations, goldendict::app::ProbePath);
        history = goldendict::core::LoadOrMigrateHistory(
            history_path.toStdString(), legacy_history_path.toStdString(),
            kMaximumHistoryEntries);
    } catch (const std::exception& error) {
        QMessageBox::warning(nullptr, QStringLiteral("GoldenDict history"),
                             QString::fromLocal8Bit(error.what()));
    }
    goldendict::core::Favorites favorites;
    try {
        goldendict::app::ValidateAutoDiscoveredLegacyFavorites(
            configuration_locations, goldendict::app::ProbePath);
        favorites = goldendict::core::LoadOrMigrateFavorites(
            favorites_path.toStdString(), legacy_favorites_path.toStdString());
    } catch (const std::exception& error) {
        QMessageBox::warning(nullptr, QStringLiteral("GoldenDict favorites"),
                             QString::fromLocal8Bit(error.what()));
    }
    MainWindow window;
    window.SetPreferences(configuration.preferences);
    window.RestoreMainWindowGeometry(configuration.main_window_geometry);
    window.RestoreMainWindowState(configuration.main_window_state);
    window.SetDictionaryGroups(configuration.dictionary_groups);
    window.SetSourceDirectories(configuration.dictionary_paths,
                                configuration.sound_directories);
    window.SetFacade(facade.get());
    const auto persist_article_tab_session = [&]() {
        auto updated = configuration;
        updated.article_tab_session = facade->ExportArticleTabSession();
        updated.main_window_geometry = window.CaptureMainWindowGeometry();
        updated.main_window_state = window.CaptureMainWindowState();
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
    const auto refresh_favorites =
        [&window, &favorites](const QList<int>& current_path = {}) {
            std::vector<FavoriteViewItem> items;
            items.reserve(favorites.size());
            for (std::size_t index = 0; index < favorites.size(); ++index) {
                items.push_back(MakeFavoriteViewItem(
                    favorites[index], {static_cast<int>(index)}));
            }
            window.SetFavoriteItems(items, current_path);
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
    QObject::connect(
        &window, &MainWindow::MoveFavoriteRequested, &window,
        [&](const QList<int>& path, int offset) {
            if (path.empty() || (offset != -1 && offset != 1)) {
                return;
            }
            auto parent_path = path;
            const int source_index = parent_path.takeLast();
            const int destination_index =
                offset < 0 ? source_index - 1 : source_index + 2;
            goldendict::core::FavoritePath source;
            goldendict::core::FavoritePath destination;
            if (destination_index < 0 || !MakeFavoritePath(path, &source) ||
                !MakeFavoritePath(parent_path, &destination)) {
                return;
            }
            try {
                auto result = goldendict::core::MoveFavorite(
                    favorites_path.toStdString(), favorites, source,
                    destination, static_cast<std::size_t>(destination_index));
                if (result.changed()) {
                    favorites = std::move(result.favorites);
                    QList<int> moved_path;
                    for (const auto index : result.moved_path) {
                        moved_path.push_back(static_cast<int>(index));
                    }
                    refresh_favorites(moved_path);
                }
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict favorites"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(
        &window, &MainWindow::MoveFavoriteToRootRequested, &window,
        [&](const QList<int>& path) {
            goldendict::core::FavoritePath source;
            if (path.size() < 2 || !MakeFavoritePath(path, &source)) {
                return;
            }
            try {
                auto result = goldendict::core::MoveFavorite(
                    favorites_path.toStdString(), favorites, source, {},
                    favorites.size());
                if (result.changed()) {
                    favorites = std::move(result.favorites);
                    QList<int> moved_path;
                    for (const auto index : result.moved_path) {
                        moved_path.push_back(static_cast<int>(index));
                    }
                    refresh_favorites(moved_path);
                }
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
                                     QStringLiteral("GoldenDict favorites"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    QObject::connect(
        &window, &MainWindow::MoveFavoriteAcrossFoldersRequested, &window,
        [&](const QList<int>& source_path, const QList<int>& destination_path,
            int destination_index, const QList<QList<int>>& expanded_paths) {
            goldendict::core::FavoritePath source;
            goldendict::core::FavoritePath destination;
            std::vector<goldendict::core::FavoritePath> expanded;
            if (destination_index < 0 ||
                !MakeFavoritePath(source_path, &source) ||
                !MakeFavoritePath(destination_path, &destination)) {
                return;
            }
            expanded.reserve(static_cast<std::size_t>(expanded_paths.size()));
            for (const auto& path : expanded_paths) {
                goldendict::core::FavoritePath converted;
                if (!MakeFavoritePath(path, &converted)) {
                    return;
                }
                expanded.push_back(std::move(converted));
            }
            try {
                auto result = goldendict::core::MoveFavorite(
                    favorites_path.toStdString(), favorites, source,
                    destination, static_cast<std::size_t>(destination_index),
                    expanded, true);
                if (result.changed()) {
                    favorites = std::move(result.favorites);
                    QList<int> moved_path;
                    for (const auto index : result.moved_path) {
                        moved_path.push_back(static_cast<int>(index));
                    }
                    refresh_favorites(moved_path);
                }
            } catch (const std::exception& error) {
                QMessageBox::warning(&window,
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
    const auto apply_sources =
        [&](const std::vector<std::string>& dictionary_paths,
            const std::vector<goldendict::core::SoundDirectoryConfiguration>&
                sound_directories,
            const std::vector<goldendict::core::MediaWikiSourceConfiguration>&
                mediawiki_sources,
            const std::vector<goldendict::core::WebsiteSourceConfiguration>&
                website_sources,
            const std::vector<goldendict::core::ForvoSourceConfiguration>&
                forvo_sources,
            const std::vector<goldendict::core::DictServerSourceConfiguration>&
                dict_server_sources,
            const std::vector<
                goldendict::core::ExternalProgramSourceConfiguration>&
                external_program_sources,
            bool show_error) -> QString {
        if (dictionary_paths == configuration.dictionary_paths &&
            sound_directories == configuration.sound_directories &&
            mediawiki_sources == configuration.mediawiki_sources &&
            website_sources == configuration.website_sources &&
            forvo_sources == configuration.forvo_sources &&
            dict_server_sources == configuration.dict_server_sources &&
            external_program_sources ==
                configuration.external_program_sources) {
            return {};
        }
        auto updated = configuration;
        updated.dictionary_paths = dictionary_paths;
        updated.sound_directories = sound_directories;
        updated.mediawiki_sources = mediawiki_sources;
        updated.website_sources = website_sources;
        updated.forvo_sources = forvo_sources;
        updated.dict_server_sources = dict_server_sources;
        updated.external_program_sources = external_program_sources;
        updated.article_tab_session = facade->ExportArticleTabSession();
        try {
            goldendict::core::ValidateConfiguration(updated);
            auto replacement =
                goldendict::network::ComposeConfiguredApplication(updated);
            if (!replacement.facade->RestoreArticleTabSession(
                    *updated.article_tab_session)) {
                throw std::runtime_error(
                    "Unable to restore the article tab "
                    "session");
            }
            goldendict::core::SaveConfiguration(
                configuration_path.toStdString(), updated);
            window.SetSourceDirectories(updated.dictionary_paths,
                                        updated.sound_directories);
            window.SetFacade(replacement.facade.get());
            facade = std::move(replacement.facade);
            composition_diagnostics = std::move(replacement.diagnostics);
            ReportRuntimeCompositionDiagnostics(composition_diagnostics);
            configuration = std::move(updated);
            window.SetOnlineSources(
                configuration.mediawiki_sources, configuration.website_sources,
                configuration.forvo_sources, configuration.dict_server_sources,
                configuration.external_program_sources, {});
            return {};
        } catch (const std::exception& error) {
            if (show_error) {
                QMessageBox::warning(&window, QStringLiteral("GoldenDict"),
                                     QString::fromLocal8Bit(error.what()));
            }
            return QString::fromLocal8Bit(error.what());
        }
    };
    const auto apply_source_directories =
        [&](const std::vector<std::string>& dictionary_paths,
            const std::vector<goldendict::core::SoundDirectoryConfiguration>&
                sound_directories,
            bool show_error) {
            return apply_sources(dictionary_paths, sound_directories,
                                 configuration.mediawiki_sources,
                                 configuration.website_sources,
                                 configuration.forvo_sources,
                                 configuration.dict_server_sources,
                                 configuration.external_program_sources,
                                 show_error)
                .isEmpty();
        };
    window.SetOnlineSources(
        configuration.mediawiki_sources, configuration.website_sources,
        configuration.forvo_sources, configuration.dict_server_sources,
        configuration.external_program_sources,
        [&](const auto& dictionary_paths, const auto& sound_directories,
            const auto& mediawiki_sources, const auto& website_sources,
            const auto& forvo_sources, const auto& dict_server_sources,
            const auto& external_program_sources) {
            return apply_sources(dictionary_paths, sound_directories,
                                 mediawiki_sources, website_sources,
                                 forvo_sources, dict_server_sources,
                                 external_program_sources, false);
        });
    QObject::connect(
        &window, &MainWindow::SourceDirectoriesEdited, &window,
        [&](const std::vector<std::string>& dictionary_paths,
            const std::vector<goldendict::core::SoundDirectoryConfiguration>&
                sound_directories) {
            apply_source_directories(dictionary_paths, sound_directories, true);
        });
    QObject::connect(
        &window, &MainWindow::DictionaryGroupsEdited, &window, [&]() {
            auto updated = configuration;
            updated.dictionary_groups = window.DictionaryGroups();
            updated.article_tab_session = facade->ExportArticleTabSession();
            try {
                auto replacement =
                    goldendict::network::ComposeConfiguredApplication(updated);
                if (!replacement.facade->RestoreArticleTabSession(
                        *updated.article_tab_session)) {
                    throw std::runtime_error(
                        "Unable to restore the article tab "
                        "session");
                }
                goldendict::core::SaveConfiguration(
                    configuration_path.toStdString(), updated);
                window.SetDictionaryGroups(updated.dictionary_groups);
                window.SetFacade(replacement.facade.get());
                facade = std::move(replacement.facade);
                composition_diagnostics = std::move(replacement.diagnostics);
                ReportRuntimeCompositionDiagnostics(composition_diagnostics);
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
                           QStringLiteral("--article-context-menu-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunArticleContextMenuCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--system-print-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunSystemPrintCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--article-tabs-smoke"))) {
        QTimer::singleShot(15000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunArticleTabsSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--suggestion-pane-smoke"))) {
        QTimer::singleShot(15000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunSuggestionPaneSmokeCheck(
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
            0, &window, [&app, &configuration, &configuration_path, &window]() {
                window.RunDictionaryGroupsSmokeCheck(
                    [&app, &configuration, &configuration_path](bool passed) {
                        try {
                            const auto persisted =
                                goldendict::core::LoadConfiguration(
                                    configuration_path.toStdString());
                            passed =
                                passed &&
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
                            } catch (const std::exception&) {}
                            passed =
                                passed && goldendict::core::LoadConfiguration(
                                              configuration_path.toStdString())
                                                  .dictionary_groups ==
                                              persisted.dictionary_groups;
                        } catch (const std::exception&) {
                            passed = false;
                        }
                        app.exit(passed ? 0 : 1);
                    });
            });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--source-directories-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration, &configuration_path, &facade,
             &apply_source_directories, &apply_sources, &window]() {
                bool passed = true;
                try {
                    configuration.dictionary_groups = {
                        {7U, "Preserved", "", {"unavailable"}}};
                    configuration.preferences.interface_language = "en_US";
                    configuration.main_window_geometry = "opaque-geometry";
                    configuration.main_window_state = "opaque-state";
                    configuration.article_tab_session =
                        facade->ExportArticleTabSession();
                    goldendict::core::SaveConfiguration(
                        configuration_path.toStdString(), configuration);
                    const auto original = configuration;
                    const auto original_session =
                        facade->ExportArticleTabSession();
                    const auto same_as_original = [&](const auto& candidate) {
                        return candidate.dictionary_paths ==
                                   original.dictionary_paths &&
                               candidate.index_directory ==
                                   original.index_directory &&
                               candidate.sound_directories ==
                                   original.sound_directories &&
                               candidate.dictionary_groups ==
                                   original.dictionary_groups &&
                               candidate.preferences == original.preferences &&
                               candidate.external_program_sources ==
                                   original.external_program_sources &&
                               candidate.mediawiki_sources ==
                                   original.mediawiki_sources &&
                               candidate.website_sources ==
                                   original.website_sources &&
                               candidate.forvo_sources ==
                                   original.forvo_sources &&
                               candidate.dict_server_sources ==
                                   original.dict_server_sources &&
                               candidate.article_tab_session ==
                                   original.article_tab_session &&
                               candidate.main_window_geometry ==
                                   original.main_window_geometry &&
                               candidate.main_window_state ==
                                   original.main_window_state;
                    };

                    passed = apply_source_directories(
                                 configuration.dictionary_paths,
                                 configuration.sound_directories, false) &&
                             same_as_original(configuration);

                    auto invalid_sounds = configuration.sound_directories;
                    invalid_sounds.push_back({"", "invalid"});
                    passed =
                        passed &&
                        !apply_source_directories(
                            configuration.dictionary_paths, invalid_sounds,
                            false) &&
                        same_as_original(configuration) &&
                        facade->ExportArticleTabSession() == original_session &&
                        facade->GetDictionaryService().GetCatalog().back().id ==
                            "smoke.external";

                    const QString temporary_path =
                        configuration_path + QStringLiteral(".tmp");
                    QDir().mkpath(temporary_path);
                    auto failed_paths = configuration.dictionary_paths;
                    failed_paths.push_back("/save-must-fail");
                    passed =
                        passed &&
                        !apply_source_directories(
                            failed_paths, configuration.sound_directories,
                            false) &&
                        same_as_original(configuration) &&
                        same_as_original(goldendict::core::LoadConfiguration(
                            configuration_path.toStdString())) &&
                        facade->ExportArticleTabSession() == original_session &&
                        facade->GetDictionaryService().GetCatalog().back().id ==
                            "smoke.external";
                    QDir(temporary_path).removeRecursively();

                    auto successful_paths = configuration.dictionary_paths;
                    successful_paths.push_back(successful_paths.front());
                    auto successful_sounds = configuration.sound_directories;
                    successful_sounds.push_back(
                        {configuration.dictionary_paths.front(), ""});
                    successful_sounds.push_back(successful_sounds.back());
                    passed = passed &&
                             apply_source_directories(successful_paths,
                                                      successful_sounds, false);
                    auto edited_wikis = configuration.mediawiki_sources;
                    edited_wikis.front().enabled = true;
                    const std::vector<
                        goldendict::core::ForvoSourceConfiguration>
                        empty_forvo;
                    auto edited_programs =
                        configuration.external_program_sources;
                    edited_programs.front().name = "Edited External";
                    edited_programs.front().argument_templates = {
                        "%GDWORD%", "", "--literal"};
                    passed = passed &&
                             apply_sources(
                                 configuration.dictionary_paths,
                                 configuration.sound_directories, edited_wikis,
                                 configuration.website_sources, empty_forvo,
                                 configuration.dict_server_sources,
                                 edited_programs, false)
                                 .isEmpty();
                    const auto persisted = goldendict::core::LoadConfiguration(
                        configuration_path.toStdString());
                    passed =
                        passed &&
                        persisted.dictionary_paths == successful_paths &&
                        persisted.sound_directories == successful_sounds &&
                        persisted.dictionary_groups ==
                            original.dictionary_groups &&
                        persisted.preferences == original.preferences &&
                        persisted.external_program_sources == edited_programs &&
                        persisted.mediawiki_sources == edited_wikis &&
                        persisted.forvo_sources.empty() &&
                        persisted.main_window_geometry ==
                            original.main_window_geometry &&
                        persisted.main_window_state ==
                            original.main_window_state &&
                        facade->ExportArticleTabSession() == original_session &&
                        facade->GetDictionaryService().GetCatalog().back().id ==
                            "smoke.external";
                } catch (const std::exception&) {
                    passed = false;
                }
                window.RunSourceDirectoriesSmokeCheck(
                    [&app, passed](bool ui_passed) {
                        app.exit(passed && ui_passed ? 0 : 1);
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
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--favorites-cross-folder-move-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &favorites_path, &window]() {
            window.RunFavoritesCrossFolderMoveSmokeCheck(
                [&app, &favorites_path](bool passed) {
                    try {
                        const auto persisted = goldendict::core::LoadFavorites(
                            favorites_path.toStdString());
                        passed = passed && persisted.size() == 3U &&
                                 persisted[0].text == "Source" &&
                                 persisted[0].children.empty() &&
                                 persisted[1].text == "Target" &&
                                 persisted[1].children.size() == 1U &&
                                 persisted[1].children[0].text == "Subtree" &&
                                 persisted[1].children[0].children.empty() &&
                                 persisted[2].text == "nested-entry";
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
