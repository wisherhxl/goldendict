// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWebEngineUrlScheme>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

#include "configuration_reload_transaction_coordinator.h"
#include "goldendict/core/application.h"
#include "goldendict/core/favorites_store.h"
#include "goldendict/core/history_store.h"
#include "goldendict/network/network_runtime.h"
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

struct PreparedProductionFacade {
    goldendict::core::application::PreparedCoreFacadeCandidate candidate;
    std::shared_ptr<goldendict::core::DesktopFacade> facade;
    std::vector<goldendict::network::RuntimeCompositionDiagnostic> diagnostics;
    bool session_restored = true;
};

struct StartupRecoverySelection {
    std::optional<goldendict::core::ConfigurationRecoveryRequest> request;
    bool desired = true;
    bool previous_attempt_started_here = false;
    bool history_replaced = false;
    std::optional<goldendict::core::ConfigurationPersistenceError> error;
};

StartupRecoverySelection ConvergeStartupPersistence(
    const std::filesystem::path& configuration_path,
    const std::filesystem::path& history_path) {
    using namespace goldendict::core;
    StartupRecoverySelection selection;
    auto inspected = InspectPendingConfigurationTransaction(configuration_path);
    if (inspected.error) {
        selection.error = inspected.error;
        return selection;
    }
    if (!inspected.present)
        return selection;
    if (!inspected.record) {
        selection.error = ConfigurationPersistenceError{
            {PendingFailureOperation::kReadRecord,
             PendingFailureDestination::kPendingRecord,
             PendingFailureCategory::kInvalidData, "pending_record_invalid"},
            "Pending configuration transaction is missing"};
        return selection;
    }

    auto record = *inspected.record;
    selection.history_replaced =
        record.history_intent == PendingHistoryIntent::kReplace;
    selection.request =
        ConfigurationRecoveryRequest{configuration_path, record.transaction_id};
    const auto fail =
        [&](const std::optional<ConfigurationPersistenceError>& error,
            const char* message) {
            selection.error = error.value_or(ConfigurationPersistenceError{
                {PendingFailureOperation::kValidateRecord,
                 PendingFailureDestination::kPendingRecord,
                 PendingFailureCategory::kInvariant, "startup_recovery_failed"},
                message});
        };

    if (record.phase == PendingTransactionPhase::kQuarantined) {
        fail(std::optional{ConfigurationPersistenceError{
                 *record.failure, "Pending transaction is quarantined"}},
             "Pending transaction is quarantined");
        return selection;
    }
    if (record.phase == PendingTransactionPhase::kPreviousRuntimeApplying) {
        const auto quarantined = QuarantineConfigurationTransaction(
            *selection.request, PendingFailureOperation::kReconstructPrevious,
            PendingFailureDestination::kRuntimeFoundation,
            PendingFailureCategory::kUnavailable,
            "previous_runtime_interrupted");
        fail(quarantined.error, "Previous runtime recovery was interrupted");
        return selection;
    }
    if (record.phase == PendingTransactionPhase::kPreviousPersistenceBlocked) {
        const auto evaluated =
            EvaluateConfigurationRecovery(*selection.request);
        fail(evaluated.primary_error,
             "Previous configuration persistence is blocked");
        return selection;
    }
    if (record.phase == PendingTransactionPhase::kPrepared) {
        const auto discarded = DiscardPreparedConfigurationTransaction(
            *selection.request, history_path);
        if (discarded.outcome != RuntimeTransitionOutcome::kApplied ||
            !discarded.removal_confirmed_durable) {
            fail(discarded.error,
                 "A pre-decision configuration transaction could not be "
                 "discarded");
        } else {
            selection.request.reset();
        }
        return selection;
    }

    const bool desired_persistence =
        record.phase == PendingTransactionPhase::kDesiredCommit ||
        record.phase == PendingTransactionPhase::kDesiredPersistenceApplying ||
        record.phase == PendingTransactionPhase::kDesiredPersistenceFailed;
    if (desired_persistence) {
        const auto policy = EvaluateConfigurationRecovery(*selection.request);
        if (policy.disposition == ConfigurationRecoveryDisposition::
                                      kAutomaticDesiredRecoveryAuthorized) {
            const auto replay =
                ReplayDesiredConfiguration(*selection.request, history_path);
            if (replay.outcome !=
                ConfigurationPersistenceOutcome::kDesiredPersistenceApplied) {
                fail(replay.error, "Desired configuration recovery failed");
                return selection;
            }
            record.phase = PendingTransactionPhase::kDesiredPersistenceApplied;
        } else if (policy.disposition == ConfigurationRecoveryDisposition::
                                             kPreviousFallbackSelected) {
            const auto previous = PersistPreviousConfiguration(
                {configuration_path, history_path, record.transaction_id});
            if (previous.outcome !=
                PreviousPersistenceOutcome::kPreviousPersistenceApplied) {
                fail(previous.error, "Previous configuration recovery failed");
                return selection;
            }
            selection.desired = false;
            selection.previous_attempt_started_here = true;
            return selection;
        } else {
            fail(policy.primary_error, "Configuration recovery policy failed");
            return selection;
        }
    }

    if (record.phase == PendingTransactionPhase::kDesiredRuntimeFailed) {
        const auto previous = PersistPreviousConfiguration(
            {configuration_path, history_path, record.transaction_id});
        if (previous.outcome !=
            PreviousPersistenceOutcome::kPreviousPersistenceApplied) {
            fail(previous.error, "Previous configuration recovery failed");
            return selection;
        }
        selection.desired = false;
        selection.previous_attempt_started_here = true;
        return selection;
    }
    if (record.phase == PendingTransactionPhase::kPreviousPersistenceApplying) {
        const auto previous = PersistPreviousConfiguration(
            {configuration_path, history_path, record.transaction_id});
        if (previous.outcome !=
            PreviousPersistenceOutcome::kPreviousPersistenceApplied) {
            fail(previous.error, "Previous configuration recovery failed");
            return selection;
        }
        selection.desired = false;
        selection.previous_attempt_started_here = true;
        return selection;
    }
    if (record.phase == PendingTransactionPhase::kDesiredPersistenceApplied) {
        const auto applying =
            BeginDesiredRuntimePublication(*selection.request);
        if (applying.outcome != RuntimeTransitionOutcome::kApplied) {
            fail(applying.error, "Cannot begin desired runtime recovery");
        }
    } else if (record.phase !=
               PendingTransactionPhase::kDesiredRuntimeApplying) {
        fail({}, "Pending transaction phase is not recoverable at startup");
    }
    return selection;
}

PreparedProductionFacade PrepareProductionFacade(
    const goldendict::core::CoreConfiguration& configuration,
    const std::shared_ptr<goldendict::network::NetworkRuntime>& network_runtime,
    goldendict::core::application::DesktopFacadeActivationOwner& owner,
    bool require_session_restoration = true) {
    auto composition = goldendict::network::ComposeConfiguredRuntimeSources(
        configuration, {}, network_runtime);
    auto candidate =
        owner.PrepareCandidate(configuration, std::move(composition.sources));
    if (!candidate)
        throw std::runtime_error("Unable to prepare the application runtime");
    auto facade = owner.PreparedFacadeSnapshot(candidate);
    if (!facade)
        throw std::runtime_error("Unable to inspect the application runtime");
    const bool session_restored =
        !configuration.article_tab_session.has_value() ||
        facade->RestoreArticleTabSession(*configuration.article_tab_session);
    if (require_session_restoration && !session_restored) {
        throw std::runtime_error("Unable to restore the article tab session");
    }
    return {std::move(candidate), std::move(facade),
            std::move(composition.diagnostics), session_restored};
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

bool HasPreferencesSmokeArgument(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument.contains(QStringLiteral("preferences-smoke")) ||
            argument == QStringLiteral("--edit-menu-smoke")) {
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
    QApplication::setApplicationVersion(
        QStringLiteral(GOLDENDICT_APPLICATION_VERSION));
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
    auto startup_recovery = ConvergeStartupPersistence(
        configuration_path.toStdString(), history_path.toStdString());
    if (startup_recovery.error) {
        qCritical().noquote()
            << "Configuration startup recovery blocked:"
            << QString::fromStdString(startup_recovery.error->message)
            << QString::fromStdString(
                   startup_recovery.error->identity.identifier);
        QMessageBox::warning(
            nullptr, QStringLiteral("GoldenDict"),
            QCoreApplication::translate(
                "GoldenDictStartup",
                "GoldenDict could not safely recover an interrupted "
                "configuration change. Startup has been stopped to avoid "
                "repeating the failure."));
        return 1;
    }
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
        if (startup_recovery.request)
            return 1;
    }
    if (configuration.index_directory.empty()) {
        configuration.index_directory = default_index_directory;
    }
    const QString command_line_root = DictionaryRootArgument(argc, argv);
    if (!startup_recovery.request && !command_line_root.isEmpty()) {
        configuration.dictionary_paths = {command_line_root.toStdString()};
    }
    if (!startup_recovery.request &&
        (HasArgument(argc, argv,
                     QStringLiteral("--source-directories-smoke")) ||
         HasArgument(argc, argv, QStringLiteral("--dictionary-bar-smoke")) ||
         HasArgument(argc, argv,
                     QStringLiteral("--widgets-facade-preparation-smoke")) ||
         HasArgument(
             argc, argv,
             QStringLiteral("--configuration-reload-coordinator-smoke")) ||
         HasArgument(
             argc, argv,
             QStringLiteral("--full-text-dictionary-projection-smoke")) ||
         HasArgument(argc, argv, QStringLiteral("--full-text-dialog-smoke")))) {
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

    const std::string network_cache_root =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            .toStdString();
    const auto block_runtime_recovery =
        [&](goldendict::core::PendingFailureDestination destination,
            const char* identifier, const QString& detail) {
            if (startup_recovery.request) {
                if (startup_recovery.desired) {
                    (void)goldendict::core::RecordDesiredRuntimeFailure(
                        *startup_recovery.request, destination,
                        goldendict::core::PendingFailureCategory::kUnavailable,
                        identifier);
                } else if (startup_recovery.previous_attempt_started_here) {
                    (void)goldendict::core::QuarantineConfigurationTransaction(
                        *startup_recovery.request,
                        goldendict::core::PendingFailureOperation::
                            kReconstructPrevious,
                        destination,
                        goldendict::core::PendingFailureCategory::kUnavailable,
                        identifier);
                }
            }
            qCritical().noquote()
                << "Configuration runtime recovery blocked:" << detail;
            QMessageBox::warning(
                nullptr, QStringLiteral("GoldenDict"),
                QCoreApplication::translate(
                    "GoldenDictStartup",
                    "GoldenDict could not safely reconstruct an interrupted "
                    "configuration change. Startup has been stopped to avoid "
                    "repeating the failure."));
        };
    std::shared_ptr<goldendict::network::NetworkRuntime> network_runtime;
    try {
        auto network_preparation = goldendict::network::NetworkRuntime::Prepare(
            {configuration.preferences.maximum_network_cache_megabytes,
             configuration.preferences.clear_network_cache_on_exit},
            network_cache_root);
        network_runtime = goldendict::network::NetworkRuntime::Create(
            std::move(network_preparation));
    } catch (const std::exception& error) {
        if (!startup_recovery.request)
            throw;
        block_runtime_recovery(
            goldendict::core::PendingFailureDestination::kRuntimeTransport,
            "network_construction_failed",
            QString::fromLocal8Bit(error.what()));
        return 1;
    }
    if (!network_runtime->diagnostic().empty()) {
        qWarning().noquote()
            << QString::fromStdString(network_runtime->diagnostic());
    }
    goldendict::core::application::DesktopFacadeActivationOwner facade_owner;
    std::optional<PreparedProductionFacade> prepared_initial_facade;
    try {
        prepared_initial_facade.emplace(PrepareProductionFacade(
            configuration, network_runtime, facade_owner, false));
    } catch (const std::exception& error) {
        if (!startup_recovery.request)
            throw;
        block_runtime_recovery(
            goldendict::core::PendingFailureDestination::kRuntimeFoundation,
            "core_construction_failed", QString::fromLocal8Bit(error.what()));
        return 1;
    }
    auto initial_facade = std::move(*prepared_initial_facade);
    ReportRuntimeCompositionDiagnostics(initial_facade.diagnostics);
    if (!initial_facade.session_restored) {
        QMessageBox::warning(
            nullptr, QStringLiteral("GoldenDict"),
            QStringLiteral("Unable to restore the saved article tab session"));
    }
    auto composition_diagnostics = std::move(initial_facade.diagnostics);
    auto facade = initial_facade.facade;
    if (!facade_owner.Activate(initial_facade.candidate)) {
        if (!startup_recovery.request)
            throw std::runtime_error(
                "Unable to activate the application runtime");
        block_runtime_recovery(
            goldendict::core::PendingFailureDestination::kRuntimeFoundation,
            "core_activation_failed",
            QStringLiteral("Unable to activate the application runtime"));
        return 1;
    }
    std::vector<goldendict::core::HistoryEntry> history;
    try {
        goldendict::app::ValidateAutoDiscoveredLegacyHistory(
            configuration_locations, goldendict::app::ProbePath);
        history = goldendict::core::LoadOrMigrateHistory(
            history_path.toStdString(), legacy_history_path.toStdString(),
            configuration.preferences.maximum_history_entries);
    } catch (const std::exception& error) {
        QMessageBox::warning(nullptr, QStringLiteral("GoldenDict history"),
                             QString::fromLocal8Bit(error.what()));
        if (startup_recovery.request && startup_recovery.history_replaced)
            return 1;
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
    std::unique_ptr<MainWindow> owned_window;
    try {
        owned_window = std::make_unique<MainWindow>(configuration_directory);
    } catch (const std::exception& error) {
        if (!startup_recovery.request)
            throw;
        block_runtime_recovery(
            goldendict::core::PendingFailureDestination::kRuntimePresentation,
            "widgets_construction_failed",
            QString::fromLocal8Bit(error.what()));
        return 1;
    }
    MainWindow& window = *owned_window;
    window.SetPreferences(configuration.preferences);
    window.SetNetworkCacheDirectory(
        QString::fromStdString(network_runtime->cache_directory()));
    window.RestoreMainWindowGeometry(configuration.main_window_geometry);
    window.RestoreMainWindowState(configuration.main_window_state);
    window.SetFullTextDialogGeometry(configuration.full_text_dialog_geometry);
    window.SetDictionaryGroups(configuration.dictionary_groups);
    window.SetSourceDirectories(configuration.dictionary_paths,
                                configuration.sound_directories);
    window.SetFacade(facade.get());
    if (startup_recovery.request) {
        const auto finalized =
            goldendict::core::FinishRecoveredConfigurationTransaction(
                *startup_recovery.request, history_path.toStdString(),
                startup_recovery.desired);
        if (finalized.outcome !=
                goldendict::core::RuntimeTransitionOutcome::kApplied ||
            !finalized.removal_confirmed_durable) {
            QMessageBox::warning(
                nullptr, QStringLiteral("GoldenDict"),
                QCoreApplication::translate(
                    "GoldenDictStartup",
                    "GoldenDict could not safely finalize an interrupted "
                    "configuration change. Startup has been stopped."));
            return 1;
        }
    }
    goldendict::app::ConfigurationReloadTransactionCoordinator coordinator(
        network_runtime, facade_owner, window);
    using ReloadBoundary = goldendict::app::ConfigurationReloadBoundary;
    const std::vector<ReloadBoundary> preferences_predecision_boundaries{
        ReloadBoundary::kPersistencePrepare,
        ReloadBoundary::kNetworkPrepare,
        ReloadBoundary::kCorePrepare,
        ReloadBoundary::kWidgetsPrepare,
        ReloadBoundary::kNetworkReserve,
        ReloadBoundary::kCoreReserve,
        ReloadBoundary::kWidgetsBegin,
        ReloadBoundary::kPersistenceDecision};
    std::size_t preferences_predecision_injection = 0U;
    std::optional<ReloadBoundary> source_predecision_injection;
    std::vector<ReloadBoundary> source_reload_boundaries;
    std::optional<ReloadBoundary> group_reload_injection;
    std::vector<std::vector<ReloadBoundary>> group_reload_traces;
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
    QObject::connect(
        &window, &MainWindow::FullTextDialogGeometryCaptured, &window,
        [&](const std::string& geometry) {
            auto updated = configuration;
            updated.full_text_dialog_geometry = geometry;
            updated.article_tab_session = facade->ExportArticleTabSession();
            updated.main_window_geometry = window.CaptureMainWindowGeometry();
            updated.main_window_state = window.CaptureMainWindowState();
            try {
                goldendict::core::SaveConfiguration(
                    configuration_path.toStdString(), updated);
                configuration = std::move(updated);
                window.SetFullTextDialogGeometry(
                    configuration.full_text_dialog_geometry);
            } catch (const std::exception& error) {
                QMessageBox::warning(&window, QStringLiteral("GoldenDict"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
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
    window.SetHistoryExportCallback([&history](const QString& path) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return file.errorString();
        }
        if (file.write(QByteArray::fromHex("efbbbf")) != 3) {
            return file.errorString();
        }
        for (const auto& entry : history) {
            QByteArray line = QString::fromStdString(entry.word).toUtf8();
            line.replace('\n', ' ');
            line.replace('\r', ' ');
            line.push_back('\n');
            if (file.write(line) != line.size()) {
                return file.errorString();
            }
        }
        if (!file.commit()) {
            return file.errorString();
        }
        return QString();
    });
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
            if (!configuration.preferences.store_history ||
                configuration.preferences.maximum_history_entries == 0U) {
                return;
            }
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
            if (updated.size() >
                configuration.preferences.maximum_history_entries) {
                updated.resize(
                    configuration.preferences.maximum_history_entries);
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
                    path.toStdString(),
                    configuration.preferences.maximum_history_entries,
                    group_id);
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
            auto prepared_network =
                goldendict::network::NetworkRuntime::Prepare(
                    {updated.preferences.maximum_network_cache_megabytes,
                     updated.preferences.clear_network_cache_on_exit},
                    network_cache_root);
            std::vector<goldendict::network::RuntimeCompositionDiagnostic>
                desired_diagnostics;
            goldendict::app::ConfigurationReloadRequest request{
                {configuration_path.toStdString(),
                 history_path.toStdString(),
                 updated,
                 goldendict::core::PendingHistoryIntent::kUnchanged,
                 {}},
                std::move(prepared_network),
                {},
                [&]() -> std::optional<
                          goldendict::app::PreparedConfigurationReloadCore> {
                    auto replacement = PrepareProductionFacade(
                        updated, network_runtime, facade_owner);
                    desired_diagnostics = std::move(replacement.diagnostics);
                    return goldendict::app::PreparedConfigurationReloadCore{
                        std::move(replacement.candidate),
                        std::move(replacement.facade)};
                }};
            goldendict::app::ConfigurationReloadDependencies dependencies;
            if (HasArgument(argc, argv,
                            QStringLiteral("--source-directories-smoke"))) {
                dependencies.observe_boundary = [&](auto boundary) {
                    source_reload_boundaries.push_back(boundary);
                };
                dependencies.inject_failure = [&](auto boundary) {
                    return source_predecision_injection == boundary;
                };
            }
            const auto result =
                coordinator.Execute(std::move(request), dependencies);
            if (result.outcome == goldendict::app::ConfigurationReloadOutcome::
                                      kRejectedBeforeDecision) {
                if (result.error)
                    return QString::fromLocal8Bit(
                        result.error->message.c_str());
                return QStringLiteral(
                    "Unable to activate the application runtime");
            }

            facade = facade_owner.CurrentSnapshot();
            composition_diagnostics = std::move(desired_diagnostics);
            ReportRuntimeCompositionDiagnostics(composition_diagnostics);
            configuration = std::move(updated);
            window.SetSourceDirectories(configuration.dictionary_paths,
                                        configuration.sound_directories);
            window.SetOnlineSources(
                configuration.mediawiki_sources, configuration.website_sources,
                configuration.forvo_sources, configuration.dict_server_sources,
                configuration.external_program_sources, {});
            if (result.outcome == goldendict::app::ConfigurationReloadOutcome::
                                      kPublishedWithForwardFailure) {
                qCritical().noquote()
                    << QStringLiteral(
                           "Source transaction published with forward failure; "
                           "durable phase %1: %2")
                           .arg(result.durable_phase
                                    ? static_cast<int>(*result.durable_phase)
                                    : -1)
                           .arg(result.error ? QString::fromStdString(
                                                   result.error->message)
                                             : QString{});
            }
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
            try {
                goldendict::core::ValidateConfiguration(updated);
                auto prepared_network =
                    goldendict::network::NetworkRuntime::Prepare(
                        {updated.preferences.maximum_network_cache_megabytes,
                         updated.preferences.clear_network_cache_on_exit},
                        network_cache_root);
                std::vector<goldendict::network::RuntimeCompositionDiagnostic>
                    desired_diagnostics;
                goldendict::core::ConfigurationTransactionPreparationInput
                    persistence;
                persistence.configuration_path =
                    configuration_path.toStdString();
                persistence.history_path = history_path.toStdString();
                persistence.desired_configuration = updated;
                persistence.history_intent =
                    goldendict::core::PendingHistoryIntent::kUnchanged;
                auto runtime_configuration = updated;
                runtime_configuration.article_tab_session =
                    facade->ExportArticleTabSession();
                goldendict::app::ConfigurationReloadRequest request{
                    std::move(persistence),
                    std::move(prepared_network),
                    {},
                    [&]()
                        -> std::optional<
                            goldendict::app::PreparedConfigurationReloadCore> {
                        auto replacement = PrepareProductionFacade(
                            runtime_configuration, network_runtime,
                            facade_owner);
                        desired_diagnostics =
                            std::move(replacement.diagnostics);
                        return goldendict::app::PreparedConfigurationReloadCore{
                            std::move(replacement.candidate),
                            std::move(replacement.facade)};
                    }};
                goldendict::app::ConfigurationReloadDependencies dependencies;
                if (HasArgument(argc, argv,
                                QStringLiteral("--dictionary-groups-smoke"))) {
                    dependencies.observe_boundary = [&](auto boundary) {
                        if (!group_reload_traces.empty())
                            group_reload_traces.back().push_back(boundary);
                    };
                    dependencies.inject_failure = [&](auto boundary) {
                        return group_reload_injection == boundary;
                    };
                }
                const auto result =
                    coordinator.Execute(std::move(request), dependencies);
                if (result.outcome ==
                    goldendict::app::ConfigurationReloadOutcome::
                        kRejectedBeforeDecision) {
                    window.SetDictionaryGroups(configuration.dictionary_groups);
                    const auto message =
                        result.error
                            ? QString::fromLocal8Bit(
                                  result.error->message.c_str())
                            : QStringLiteral(
                                  "Unable to activate the application runtime");
                    QMessageBox::warning(
                        &window, QStringLiteral("Dictionary Groups"), message);
                    return;
                }

                facade = facade_owner.CurrentSnapshot();
                composition_diagnostics = std::move(desired_diagnostics);
                ReportRuntimeCompositionDiagnostics(composition_diagnostics);
                configuration = std::move(updated);
                if (result.outcome ==
                    goldendict::app::ConfigurationReloadOutcome::
                        kPublishedWithForwardFailure) {
                    qCritical().noquote()
                        << QStringLiteral(
                               "Dictionary-group transaction published with "
                               "forward failure; durable phase %1: %2")
                               .arg(
                                   result.durable_phase
                                       ? static_cast<int>(*result.durable_phase)
                                       : -1)
                               .arg(result.error ? QString::fromStdString(
                                                       result.error->message)
                                                 : QString{});
                }
            } catch (const std::exception& error) {
                window.SetDictionaryGroups(configuration.dictionary_groups);
                QMessageBox::warning(&window,
                                     QStringLiteral("Dictionary Groups"),
                                     QString::fromLocal8Bit(error.what()));
            }
        });
    window.SetPreferencesApplyCallback(
        [&](const goldendict::core::ApplicationPreferences& preferences) {
            if (preferences == configuration.preferences)
                return QString{};
            auto updated = configuration;
            updated.preferences = preferences;
            updated.article_tab_session = facade->ExportArticleTabSession();
            auto bounded_history = history;
            if (bounded_history.size() > preferences.maximum_history_entries) {
                bounded_history.resize(preferences.maximum_history_entries);
            }
            const bool history_changed = bounded_history != history;
            try {
                goldendict::core::ValidateConfiguration(updated);
                auto prepared_network =
                    goldendict::network::NetworkRuntime::Prepare(
                        {preferences.maximum_network_cache_megabytes,
                         preferences.clear_network_cache_on_exit},
                        network_cache_root);
                if (preferences.maximum_network_cache_megabytes != 0U &&
                    !prepared_network.cache_available) {
                    throw std::runtime_error(prepared_network.diagnostic);
                }

                std::vector<goldendict::network::RuntimeCompositionDiagnostic>
                    desired_diagnostics;
                goldendict::app::ConfigurationReloadRequest request{
                    {configuration_path.toStdString(),
                     history_path.toStdString(), updated,
                     history_changed
                         ? goldendict::core::PendingHistoryIntent::kReplace
                         : goldendict::core::PendingHistoryIntent::kUnchanged,
                     history_changed
                         ? bounded_history
                         : std::vector<goldendict::core::HistoryEntry>{}},
                    std::move(prepared_network),
                    {},
                    [&]()
                        -> std::optional<
                            goldendict::app::PreparedConfigurationReloadCore> {
                        auto replacement = PrepareProductionFacade(
                            updated, network_runtime, facade_owner);
                        desired_diagnostics =
                            std::move(replacement.diagnostics);
                        return goldendict::app::PreparedConfigurationReloadCore{
                            std::move(replacement.candidate),
                            std::move(replacement.facade)};
                    }};
                goldendict::app::ConfigurationReloadDependencies dependencies;
                std::optional<ReloadBoundary> last_boundary;
                dependencies.observe_boundary = [&](auto boundary) {
                    last_boundary = boundary;
                };
                if (HasArgument(
                        argc, argv,
                        QStringLiteral(
                            "--preferences-coordinator-predecision-smoke"))) {
                    dependencies.inject_failure = [&](auto boundary) {
                        if (preferences_predecision_injection >=
                            preferences_predecision_boundaries.size()) {
                            return false;
                        }
                        if (boundary !=
                            preferences_predecision_boundaries
                                [preferences_predecision_injection]) {
                            return false;
                        }
                        ++preferences_predecision_injection;
                        return true;
                    };
                }
                const auto result =
                    coordinator.Execute(std::move(request), dependencies);
                if (result.outcome ==
                    goldendict::app::ConfigurationReloadOutcome::
                        kRejectedBeforeDecision) {
                    qWarning().noquote()
                        << "Preferences transaction rejected before decision at"
                        << (last_boundary ? static_cast<int>(*last_boundary)
                                          : -1)
                        << (result.error
                                ? QString::fromStdString(result.error->message)
                                : QString{});
                    if (result.error)
                        return QString::fromLocal8Bit(
                            result.error->message.c_str());
                    return QCoreApplication::translate(
                        "MainWindow",
                        "Preferences cannot be applied in this context");
                }

                configuration = std::move(updated);
                facade = facade_owner.CurrentSnapshot();
                composition_diagnostics = std::move(desired_diagnostics);
                ReportRuntimeCompositionDiagnostics(composition_diagnostics);
                if (history_changed) {
                    history = std::move(bounded_history);
                    refresh_history();
                }
                if (result.outcome ==
                    goldendict::app::ConfigurationReloadOutcome::
                        kPublishedWithForwardFailure) {
                    qCritical().noquote()
                        << QStringLiteral(
                               "Preferences transaction published with "
                               "forward failure; durable phase %1: %2")
                               .arg(
                                   result.durable_phase
                                       ? static_cast<int>(*result.durable_phase)
                                       : -1)
                               .arg(result.error ? QString::fromStdString(
                                                       result.error->message)
                                                 : QString{});
                }
                return QString{};
            } catch (const std::exception& error) {
                qWarning().noquote()
                    << "Unable to prepare Preferences transaction:"
                    << error.what();
                return QString::fromLocal8Bit(error.what());
            }
        });
    window.show();

    if (HasPreferencesSmokeArgument(argc, argv)) {
        try {
            if (!QDir().mkpath(configuration_directory))
                throw std::runtime_error(
                    "Unable to prepare Preferences smoke configuration");
            goldendict::core::SaveConfiguration(
                configuration_path.toStdString(), configuration);
        } catch (const std::exception& error) {
            qWarning().noquote()
                << "Unable to seed Preferences smoke configuration:"
                << error.what();
        }
    }

    if (HasArgument(
            argc, argv,
            QStringLiteral("--preferences-coordinator-predecision-smoke"))) {
        configuration.main_window_geometry.clear();
        configuration.main_window_state.clear();
        configuration.full_text_dialog_geometry.clear();
        configuration.article_tab_session.reset();
        configuration.index_directory = default_index_directory;
        configuration.dictionary_paths.clear();
        configuration.sound_directories.clear();
        const auto initial_configuration = configuration;
        const auto initial_facade_snapshot = facade;
        QTimer::singleShot(15000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration_directory, &configuration_path, &history_path,
             &window, initial_configuration, initial_facade_snapshot,
             &facade_owner, &preferences_predecision_injection,
             &preferences_predecision_boundaries]() {
                QByteArray initial_configuration_bytes;
                try {
                    if (!QDir().mkpath(configuration_directory))
                        throw std::runtime_error(
                            "Unable to prepare smoke configuration directory");
                    goldendict::core::SaveConfiguration(
                        configuration_path.toStdString(),
                        initial_configuration);
                    QFile file(configuration_path);
                    if (!file.open(QIODevice::ReadOnly))
                        throw std::runtime_error(
                            "Unable to inspect smoke configuration");
                    initial_configuration_bytes = file.readAll();
                } catch (const std::exception& error) {
                    qWarning().noquote()
                        << "Unable to prepare Preferences coordinator smoke:"
                        << error.what();
                    app.exit(1);
                    return;
                }
                window.RunPreferencesCoordinatorPredecisionSmokeCheck(
                    [&](bool passed) {
                        try {
                            QFile file(configuration_path);
                            if (!file.open(QIODevice::ReadOnly))
                                throw std::runtime_error(
                                    "Unable to inspect smoke configuration");
                            passed =
                                passed &&
                                file.readAll() == initial_configuration_bytes &&
                                facade_owner.CurrentSnapshot() ==
                                    initial_facade_snapshot &&
                                preferences_predecision_injection ==
                                    preferences_predecision_boundaries.size() &&
                                !std::filesystem::exists(
                                    goldendict::core::
                                        PendingConfigurationTransactionPath(
                                            configuration_path.toStdString()));
                            if (!passed) {
                                qWarning()
                                    << "Preferences coordinator predecision "
                                       "smoke failed"
                                    << preferences_predecision_injection
                                    << preferences_predecision_boundaries.size()
                                    << (facade_owner.CurrentSnapshot() ==
                                        initial_facade_snapshot);
                            }
                            if (std::filesystem::exists(
                                    history_path.toStdString())) {
                                (void)goldendict::core::LoadHistory(
                                    history_path.toStdString());
                            }
                        } catch (...) {
                            passed = false;
                        }
                        app.exit(passed ? 0 : 1);
                    });
            });
    } else if (HasArgument(argc, argv, QStringLiteral("--search-menu-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunSearchMenuSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv, QStringLiteral("--edit-menu-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunEditMenuSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--history-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration_directory, &configuration_path, &history,
             &history_path, &window]() {
                QDir().mkpath(configuration_directory);
                history = {{3U, "Newest"}, {2U, "Middle"}, {1U, "Oldest"}};
                goldendict::core::SaveHistory(history_path.toStdString(),
                                              history);
                window.SetHistoryItems({{QStringLiteral("Newest"), 3U},
                                        {QStringLiteral("Middle"), 2U},
                                        {QStringLiteral("Oldest"), 1U}});
                const QString import_path =
                    QDir(configuration_directory)
                        .filePath(QStringLiteral("history-preferences.txt"));
                QFile import_file(import_path);
                const QByteArray contents(
                    "Imported one\nImported two\nIgnored\n");
                const bool prepared =
                    import_file.open(QIODevice::WriteOnly) &&
                    import_file.write(contents) == contents.size();
                import_file.close();
                window.RunHistoryPreferencesSmokeCheck(
                    import_path, [&app, &configuration_path, &history_path,
                                  prepared](bool passed) {
                        try {
                            const auto persisted_history =
                                goldendict::core::LoadHistory(
                                    history_path.toStdString(), 1U);
                            const auto persisted_configuration =
                                goldendict::core::LoadConfiguration(
                                    configuration_path.toStdString());
                            passed =
                                passed && prepared &&
                                persisted_history.size() == 1U &&
                                persisted_history.front().word == "Recorded" &&
                                persisted_configuration.preferences
                                    .store_history &&
                                persisted_configuration.preferences
                                        .maximum_history_entries == 1U;
                        } catch (...) {
                            passed = false;
                        }
                        app.exit(passed ? 0 : 1);
                    });
            });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--favorites-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration_directory, &configuration_path,
             &favorites_path, &window]() {
                QDir().mkpath(configuration_directory);
                window.RunFavoritesPreferencesSmokeCheck(
                    [&app, &configuration_path, &favorites_path](bool passed) {
                        try {
                            const auto persisted_favorites =
                                goldendict::core::LoadFavorites(
                                    favorites_path.toStdString());
                            const auto persisted_configuration =
                                goldendict::core::LoadConfiguration(
                                    configuration_path.toStdString());
                            passed = passed && persisted_favorites.empty() &&
                                     persisted_configuration.preferences
                                         .confirm_favorites_deletion;
                        } catch (...) {
                            passed = false;
                        }
                        app.exit(passed ? 0 : 1);
                    });
            });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--dictionary-context-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &configuration_path, &window]() {
            window.RunDictionaryContextPreferencesSmokeCheck(
                [&app, &configuration_path](bool passed) {
                    try {
                        const auto persisted =
                            goldendict::core::LoadConfiguration(
                                configuration_path.toStdString());
                        passed = passed &&
                                 persisted.preferences
                                         .maximum_dictionary_references == 0U;
                    } catch (...) {
                        passed = false;
                    }
                    app.exit(passed ? 0 : 1);
                });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--articles-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &configuration_path, &window]() {
            window.RunArticlesPreferencesSmokeCheck([&app, &configuration_path](
                                                        bool passed) {
                try {
                    const auto persisted_configuration =
                        goldendict::core::LoadConfiguration(
                            configuration_path.toStdString());
                    passed =
                        passed &&
                        persisted_configuration.preferences
                            .collapse_large_articles &&
                        persisted_configuration.preferences.ignore_diacritics &&
                        persisted_configuration.preferences
                                .article_size_limit == 3450U;
                } catch (...) {
                    passed = false;
                }
                app.exit(passed ? 0 : 1);
            });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--synonym-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &configuration_path, &window]() {
            window.RunSynonymPreferencesSmokeCheck(
                [&app, &configuration_path](bool passed) {
                    try {
                        const auto persisted_configuration =
                            goldendict::core::LoadConfiguration(
                                configuration_path.toStdString());
                        passed = passed && !persisted_configuration.preferences
                                                .synonym_search_enabled;
                    } catch (...) {
                        passed = false;
                    }
                    app.exit(passed ? 0 : 1);
                });
        });
    } else if (HasArgument(argc, argv, QStringLiteral("--file-menu-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window, [&app, &configuration_directory, &window]() {
                QDir().mkpath(configuration_directory);
                window.RunFileMenuSmokeCheck(
                    QDir(configuration_directory)
                        .filePath(QStringLiteral("file-menu-article.html")),
                    [&app](bool passed) { app.exit(passed ? 0 : 1); });
            });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--optional-parts-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &configuration_path, &window]() {
            window.RunOptionalPartsPreferencesSmokeCheck(
                [&app, &configuration_path](bool passed) {
                    try {
                        const auto persisted_configuration =
                            goldendict::core::LoadConfiguration(
                                configuration_path.toStdString());
                        passed = passed && persisted_configuration.preferences
                                               .always_expand_optional_parts;
                    } catch (...) {
                        passed = false;
                    }
                    app.exit(passed ? 0 : 1);
                });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--hide-single-tab-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &configuration_path, &window]() {
            window.RunHideSingleTabPreferencesSmokeCheck(
                [&app, &configuration_path, &window](bool passed) {
                    try {
                        const auto persisted =
                            goldendict::core::LoadConfiguration(
                                configuration_path.toStdString());
                        passed =
                            passed && persisted.preferences.hide_single_tab;
                        window.SetPreferences(persisted.preferences);
                    } catch (...) {
                        passed = false;
                    }
                    if (!passed) {
                        app.exit(1);
                        return;
                    }
                    window.RunHideSingleTabRestartSmokeCheck(
                        [&app](bool restart_passed) {
                            app.exit(restart_passed ? 0 : 1);
                        });
                });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral(
                       "--escape-hides-main-window-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration, &configuration_directory,
             &configuration_path, &window]() {
                QDir().mkpath(configuration_directory);
                if (configuration.preferences.escape_hides_main_window) {
                    configuration.preferences.escape_hides_main_window = false;
                    try {
                        goldendict::core::SaveConfiguration(
                            configuration_path.toStdString(), configuration);
                        window.SetPreferences(configuration.preferences);
                    } catch (...) {
                        app.exit(1);
                        return;
                    }
                }
                window.RunEscapeHidesMainWindowPreferencesSmokeCheck(
                    [&app, &configuration_path](bool passed) {
                        try {
                            const auto persisted =
                                goldendict::core::LoadConfiguration(
                                    configuration_path.toStdString());
                            passed =
                                passed &&
                                persisted.preferences.escape_hides_main_window;
                        } catch (...) {
                            passed = false;
                        }
                        app.exit(passed ? 0 : 1);
                    });
            });
    } else if (HasArgument(argc, argv,
                           QStringLiteral(
                               "--escape-hides-main-window-restart-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunEscapeHidesMainWindowRestartSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--article-click-preferences-smoke"))) {
        QTimer::singleShot(15000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &configuration_path, &window]() {
            window.RunArticleClickPreferencesSmokeCheck(
                [&app, &configuration_path](bool passed) {
                    try {
                        const auto persisted =
                            goldendict::core::LoadConfiguration(
                                configuration_path.toStdString());
                        passed =
                            passed &&
                            persisted.preferences.double_click_translates &&
                            persisted.preferences.select_word_by_single_click;
                    } catch (...) {
                        passed = false;
                    }
                    app.exit(passed ? 0 : 1);
                });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--article-click-restart-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunArticleClickRestartSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--proxy-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &configuration_path, &window]() {
            window.RunProxyPreferencesSmokeCheck([&app, &configuration_path](
                                                     bool passed) {
                try {
                    const auto persisted = goldendict::core::LoadConfiguration(
                        configuration_path.toStdString());
                    passed = passed &&
                             persisted.preferences.proxy_mode ==
                                 goldendict::core::ProxyMode::kManual &&
                             persisted.preferences.proxy_type ==
                                 goldendict::core::ProxyType::kHttpConnect;
                } catch (...) {
                    passed = false;
                }
                app.exit(passed ? 0 : 1);
            });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--proxy-preferences-restart-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunProxyPreferencesRestartSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--network-cache-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration_path, &network_runtime, &window]() {
                window.RunNetworkCachePreferencesSmokeCheck(
                    [&app, &configuration_path, &network_runtime](bool passed) {
                        const auto persisted =
                            goldendict::core::LoadConfiguration(
                                configuration_path.toStdString());
                        passed =
                            passed &&
                            persisted.preferences
                                    .maximum_network_cache_megabytes == 64U &&
                            !persisted.preferences
                                 .clear_network_cache_on_exit &&
                            network_runtime->maximum_cache_bytes() ==
                                64LL * 1024LL * 1024LL;
                        app.exit(passed ? 0 : 1);
                    });
            });
    } else if (HasArgument(argc, argv,
                           QStringLiteral(
                               "--network-cache-preferences-restart-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &network_runtime, &window]() {
            const bool runtime_restarted =
                network_runtime->maximum_cache_bytes() ==
                64LL * 1024LL * 1024LL;
            window.RunNetworkCachePreferencesRestartSmokeCheck(
                [&app, &network_runtime, runtime_restarted](bool passed) {
                    passed = passed && runtime_restarted &&
                             network_runtime->maximum_cache_bytes() ==
                                 32LL * 1024LL * 1024LL;
                    app.exit(passed ? 0 : 1);
                });
        });
    } else if (HasArgument(argc, argv, QStringLiteral("--view-menu-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunViewMenuSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--mru-tab-order-preferences-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window, [&app, &configuration, &configuration_path, &window]() {
                if (configuration.preferences.mru_tab_order) {
                    configuration.preferences.mru_tab_order = false;
                    try {
                        goldendict::core::SaveConfiguration(
                            configuration_path.toStdString(), configuration);
                        window.SetPreferences(configuration.preferences);
                    } catch (...) {
                        app.exit(1);
                        return;
                    }
                }
                window.RunMruTabOrderPreferencesSmokeCheck(
                    [&app, &configuration_path, &window](bool passed) {
                        try {
                            const auto persisted =
                                goldendict::core::LoadConfiguration(
                                    configuration_path.toStdString());
                            passed =
                                passed && persisted.preferences.mru_tab_order;
                            window.SetPreferences(persisted.preferences);
                        } catch (...) {
                            passed = false;
                        }
                        if (!passed) {
                            app.exit(1);
                            return;
                        }
                        window.RunMruTabOrderRestartSmokeCheck(
                            [&app](bool restart_passed) {
                                app.exit(restart_passed ? 0 : 1);
                            });
                    });
            });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--history-menu-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration_directory, &history, &history_path,
             &window]() {
                QDir().mkpath(configuration_directory);
                history = {{7U, "Alpha"}, {7U, "Beta"}};
                goldendict::core::SaveHistory(history_path.toStdString(),
                                              history);
                window.SetHistoryItems({{QStringLiteral("Alpha"), 7U},
                                        {QStringLiteral("Beta"), 7U}});
                window.RunHistoryMenuSmokeCheck(
                    QDir(configuration_directory)
                        .filePath(QStringLiteral("history-menu-export.txt")),
                    [&app](bool passed) { app.exit(passed ? 0 : 1); });
            });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--favorites-menu-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window, [&app, &configuration_directory, &window]() {
                QDir().mkpath(configuration_directory);
                window.RunFavoritesMenuSmokeCheck(
                    QDir(configuration_directory)
                        .filePath(QStringLiteral("favorites-menu-export.xml")),
                    [&app](bool passed) { app.exit(passed ? 0 : 1); });
            });
    } else if (HasArgument(argc, argv, QStringLiteral("--help-menu-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunHelpMenuSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv, QStringLiteral("--webengine-smoke"))) {
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
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--dictionary-context-navigation-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&app, &window]() {
            window.RunDictionaryContextNavigationCheck(
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
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--dictionary-bar-smoke"))) {
        QTimer::singleShot(0, &window, [&window, &app]() {
            window.RunDictionaryBarSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--widgets-facade-preparation-smoke"))) {
        QTimer::singleShot(10000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(0, &window, [&window, &app]() {
            window.RunWidgetsFacadePreparationSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral(
                               "--configuration-reload-coordinator-smoke"))) {
        QTimer::singleShot(15000, &app, [&app]() { app.exit(2); });
        QTimer::singleShot(
            0, &window,
            [&app, &configuration, &configuration_path, &history_path,
             &network_cache_root, &coordinator, &facade_owner]() {
                const auto previous_facade = facade_owner.CurrentSnapshot();
                auto desired = configuration;
                desired.dictionary_paths.clear();
                desired.sound_directories.clear();
                desired.mediawiki_sources.clear();
                desired.website_sources.clear();
                desired.forvo_sources.clear();
                desired.dict_server_sources.clear();
                desired.external_program_sources.clear();
                desired.dictionary_groups.clear();
                try {
                    goldendict::core::SaveConfiguration(
                        configuration_path.toStdString(), configuration);
                } catch (...) {
                    app.exit(1);
                    return;
                }
                auto network = goldendict::network::NetworkRuntime::Prepare(
                    {desired.preferences.maximum_network_cache_megabytes,
                     desired.preferences.clear_network_cache_on_exit},
                    network_cache_root);
                goldendict::app::ConfigurationReloadRequest request{
                    {configuration_path.toStdString(),
                     history_path.toStdString(),
                     desired,
                     goldendict::core::PendingHistoryIntent::kUnchanged,
                     {}},
                    std::move(network),
                    {},
                    {}};
                std::vector<goldendict::app::ConfigurationReloadBoundary> trace;
                goldendict::app::ConfigurationReloadDependencies dependencies;
                dependencies.observe_boundary = [&](auto boundary) {
                    trace.push_back(boundary);
                };
                const auto result =
                    coordinator.Execute(std::move(request), dependencies);
                using Boundary = goldendict::app::ConfigurationReloadBoundary;
                const std::vector<Boundary> expected{
                    Boundary::kPersistencePrepare,
                    Boundary::kNetworkPrepare,
                    Boundary::kCorePrepare,
                    Boundary::kWidgetsPrepare,
                    Boundary::kNetworkReserve,
                    Boundary::kCoreReserve,
                    Boundary::kWidgetsBegin,
                    Boundary::kPersistenceDecision,
                    Boundary::kDesiredRuntimeApplying,
                    Boundary::kNetworkPublish,
                    Boundary::kCorePublish,
                    Boundary::kWidgetsPublish,
                    Boundary::kNetworkPostWork,
                    Boundary::kWidgetsFinish,
                    Boundary::kCoreForwardWork,
                    Boundary::kFinalizeTransaction};
                const bool passed =
                    result.outcome ==
                        goldendict::app::ConfigurationReloadOutcome::
                            kPublished &&
                    result.network_published && result.core_published &&
                    result.widgets_published && trace == expected &&
                    previous_facade != nullptr &&
                    facade_owner.CurrentSnapshot() != nullptr &&
                    facade_owner.CurrentSnapshot() != previous_facade &&
                    !std::filesystem::exists(
                        goldendict::core::PendingConfigurationTransactionPath(
                            configuration_path.toStdString()));
                if (!passed) {
                    qWarning()
                        << "coordinator smoke failed"
                        << static_cast<int>(result.outcome)
                        << result.network_published << result.core_published
                        << result.widgets_published << trace.size();
                    for (const auto boundary : trace)
                        qWarning() << static_cast<int>(boundary);
                    if (result.error)
                        qWarning().noquote()
                            << QString::fromStdString(result.error->message);
                }
                app.exit(passed ? 0 : 1);
            });
    } else if (HasArgument(
                   argc, argv,
                   QStringLiteral("--full-text-dictionary-projection-smoke"))) {
        QTimer::singleShot(0, &window, [&window, &app]() {
            window.RunFullTextDictionaryProjectionSmokeCheck(
                [&app](bool passed) { app.exit(passed ? 0 : 1); });
        });
    } else if (HasArgument(argc, argv,
                           QStringLiteral("--full-text-dialog-smoke"))) {
        QTimer::singleShot(0, &window, [&window, &app, &configuration_path]() {
            window.RunFullTextDialogSmokeCheck([&app, &configuration_path](
                                                   bool passed) {
                try {
                    const auto persisted = goldendict::core::LoadConfiguration(
                        configuration_path.toStdString());
                    passed =
                        passed && !persisted.full_text_dialog_geometry.empty();
                } catch (const std::exception&) {
                    passed = false;
                }
                app.exit(passed ? 0 : 1);
            });
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
            [&app, &configuration, &configuration_path, &facade, &facade_owner,
             &favorites_path, &group_reload_injection, &group_reload_traces,
             &history_path, &preferences_predecision_boundaries, &window]() {
                const auto read_bytes = [](const QString& path) {
                    QFile file(path);
                    return file.open(QIODevice::ReadOnly) ? file.readAll()
                                                          : QByteArray{};
                };
                struct SmokeState {
                    bool preserved = true;
                    bool transaction_files_unchanged = false;
                    bool exact_desired_configuration = false;
                    QByteArray configuration_bytes;
                    QByteArray history_bytes;
                    QByteArray favorites_bytes;
                    bool history_present = false;
                    bool favorites_present = false;
                    goldendict::core::CoreConfiguration initial_configuration;
                    std::shared_ptr<goldendict::core::DesktopFacade>
                        initial_facade;
                };
                auto state = std::make_shared<SmokeState>();
                try {
                    configuration.dictionary_groups.clear();
                    window.SetDictionaryGroups({});
                    if (!QDir().mkpath(
                            QFileInfo(configuration_path).absolutePath())) {
                        throw std::runtime_error(
                            "Unable to prepare dictionary-group smoke");
                    }
                    goldendict::core::SaveConfiguration(
                        configuration_path.toStdString(), configuration);
                    state->initial_configuration = configuration;
                } catch (const std::exception& error) {
                    qWarning().noquote()
                        << "Unable to seed dictionary-group smoke:"
                        << error.what();
                    app.exit(1);
                    return;
                }
                state->configuration_bytes = read_bytes(configuration_path);
                state->history_bytes = read_bytes(history_path);
                state->favorites_bytes = read_bytes(favorites_path);
                state->history_present = QFileInfo::exists(history_path);
                state->favorites_present = QFileInfo::exists(favorites_path);
                state->initial_facade = facade;
                window.RunDictionaryGroupsSmokeCheck(
                    [&, state, read_bytes](std::size_t attempt) {
                        if (attempt == 10U) {
                            auto expected = state->initial_configuration;
                            expected.dictionary_groups =
                                window.DictionaryGroups();
                            const QString expected_path =
                                configuration_path +
                                QStringLiteral(".expected");
                            goldendict::core::SaveConfiguration(
                                expected_path.toStdString(), expected);
                            state->exact_desired_configuration =
                                read_bytes(configuration_path) ==
                                read_bytes(expected_path);
                            QFile::remove(expected_path);
                            state->transaction_files_unchanged =
                                QFileInfo::exists(history_path) ==
                                    state->history_present &&
                                read_bytes(history_path) ==
                                    state->history_bytes &&
                                QFileInfo::exists(favorites_path) ==
                                    state->favorites_present &&
                                read_bytes(favorites_path) ==
                                    state->favorites_bytes;
                            return;
                        }
                        if (attempt > 0U && attempt <= 8U) {
                            state->preserved =
                                state->preserved &&
                                read_bytes(configuration_path) ==
                                    state->configuration_bytes &&
                                facade == state->initial_facade &&
                                facade_owner.CurrentSnapshot() ==
                                    state->initial_facade &&
                                !std::filesystem::exists(
                                    goldendict::core::
                                        PendingConfigurationTransactionPath(
                                            configuration_path
                                                .toStdString())) &&
                                QFileInfo::exists(history_path) ==
                                    state->history_present &&
                                read_bytes(history_path) ==
                                    state->history_bytes &&
                                QFileInfo::exists(favorites_path) ==
                                    state->favorites_present &&
                                read_bytes(favorites_path) ==
                                    state->favorites_bytes;
                        }
                        group_reload_traces.emplace_back();
                        if (attempt <
                            preferences_predecision_boundaries.size()) {
                            group_reload_injection =
                                preferences_predecision_boundaries[attempt];
                        } else if (attempt == 9U) {
                            group_reload_injection =
                                ReloadBoundary::kNetworkPostWork;
                        } else {
                            group_reload_injection.reset();
                        }
                    },
                    [&app, &configuration, &configuration_path, &facade,
                     &facade_owner, &favorites_path, &group_reload_traces,
                     &history_path, &preferences_predecision_boundaries,
                     read_bytes, state](bool passed) {
                        const bool window_passed = passed;
                        try {
                            const std::vector<ReloadBoundary>
                                successful_boundaries{
                                    ReloadBoundary::kPersistencePrepare,
                                    ReloadBoundary::kNetworkPrepare,
                                    ReloadBoundary::kCorePrepare,
                                    ReloadBoundary::kWidgetsPrepare,
                                    ReloadBoundary::kNetworkReserve,
                                    ReloadBoundary::kCoreReserve,
                                    ReloadBoundary::kWidgetsBegin,
                                    ReloadBoundary::kPersistenceDecision,
                                    ReloadBoundary::kDesiredRuntimeApplying,
                                    ReloadBoundary::kNetworkPublish,
                                    ReloadBoundary::kCorePublish,
                                    ReloadBoundary::kWidgetsPublish,
                                    ReloadBoundary::kNetworkPostWork,
                                    ReloadBoundary::kWidgetsFinish,
                                    ReloadBoundary::kCoreForwardWork,
                                    ReloadBoundary::kFinalizeTransaction};
                            passed = passed && state->preserved &&
                                     state->transaction_files_unchanged &&
                                     group_reload_traces.size() == 10U;
                            for (std::size_t index = 0U;
                                 passed &&
                                 index <
                                     preferences_predecision_boundaries.size();
                                 ++index) {
                                passed = !group_reload_traces[index].empty() &&
                                         group_reload_traces[index].back() ==
                                             preferences_predecision_boundaries
                                                 [index];
                            }
                            passed =
                                passed &&
                                group_reload_traces[8] ==
                                    successful_boundaries &&
                                !group_reload_traces[9].empty() &&
                                group_reload_traces[9].back() ==
                                    ReloadBoundary::kCoreForwardWork &&
                                std::find(
                                    group_reload_traces[9].begin(),
                                    group_reload_traces[9].end(),
                                    ReloadBoundary::kRecordRuntimeFailure) !=
                                    group_reload_traces[9].end();
                            const auto persisted =
                                goldendict::core::LoadConfiguration(
                                    configuration_path.toStdString());
                            passed =
                                passed &&
                                persisted.dictionary_groups.size() == 1U &&
                                persisted.dictionary_groups.front().id == 7U &&
                                persisted.dictionary_groups.front().name ==
                                    "Forward Published" &&
                                persisted.dictionary_paths ==
                                    configuration.dictionary_paths &&
                                persisted.index_directory ==
                                    configuration.index_directory &&
                                persisted.sound_directories ==
                                    configuration.sound_directories &&
                                state->exact_desired_configuration &&
                                configuration.dictionary_groups ==
                                    persisted.dictionary_groups;
                            const auto inspected = goldendict::core::
                                InspectPendingConfigurationTransaction(
                                    configuration_path.toStdString());
                            passed = passed &&
                                     facade != state->initial_facade &&
                                     facade_owner.CurrentSnapshot() == facade &&
                                     QFileInfo::exists(favorites_path) ==
                                         state->favorites_present &&
                                     read_bytes(favorites_path) ==
                                         state->favorites_bytes &&
                                     inspected.present && inspected.record &&
                                     inspected.record->history_intent ==
                                         goldendict::core::
                                             PendingHistoryIntent::kUnchanged &&
                                     !inspected.record->desired_history
                                          .has_value() &&
                                     inspected.record->failure.has_value();
                            if (!passed) {
                                qWarning()
                                    << "group evidence" << inspected.present
                                    << inspected.record.has_value()
                                    << (inspected.record &&
                                        inspected.record->desired_history
                                            .has_value())
                                    << (inspected.record &&
                                        inspected.record->failure.has_value())
                                    << state->transaction_files_unchanged
                                    << (read_bytes(favorites_path) ==
                                        state->favorites_bytes);
                            }
                        } catch (const std::exception&) {
                            passed = false;
                        }
                        if (!passed) {
                            qWarning()
                                << "Dictionary-group coordinator smoke failed"
                                << window_passed << state->preserved
                                << group_reload_traces.size()
                                << (facade != state->initial_facade)
                                << (facade_owner.CurrentSnapshot() == facade);
                            for (const auto& trace : group_reload_traces)
                                qWarning() << "group trace" << trace.size();
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
             &apply_source_directories, &apply_sources,
             &preferences_predecision_boundaries, &source_predecision_injection,
             &source_reload_boundaries, &facade_owner, &window]() {
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
                    const auto original_facade = facade;
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

                    auto successful_paths = configuration.dictionary_paths;
                    successful_paths.push_back(successful_paths.front());
                    auto successful_sounds = configuration.sound_directories;
                    successful_sounds.push_back(
                        {configuration.dictionary_paths.front(), ""});
                    successful_sounds.push_back(successful_sounds.back());

                    for (const auto boundary :
                         preferences_predecision_boundaries) {
                        source_reload_boundaries.clear();
                        source_predecision_injection = boundary;
                        passed =
                            passed &&
                            !apply_source_directories(
                                successful_paths, successful_sounds, false) &&
                            !source_reload_boundaries.empty() &&
                            source_reload_boundaries.back() == boundary &&
                            same_as_original(configuration) &&
                            same_as_original(
                                goldendict::core::LoadConfiguration(
                                    configuration_path.toStdString())) &&
                            facade == original_facade &&
                            facade_owner.CurrentSnapshot() == original_facade &&
                            !std::filesystem::exists(
                                goldendict::core::
                                    PendingConfigurationTransactionPath(
                                        configuration_path.toStdString()));
                    }
                    source_predecision_injection.reset();
                    const std::vector<ReloadBoundary> successful_boundaries{
                        ReloadBoundary::kPersistencePrepare,
                        ReloadBoundary::kNetworkPrepare,
                        ReloadBoundary::kCorePrepare,
                        ReloadBoundary::kWidgetsPrepare,
                        ReloadBoundary::kNetworkReserve,
                        ReloadBoundary::kCoreReserve,
                        ReloadBoundary::kWidgetsBegin,
                        ReloadBoundary::kPersistenceDecision,
                        ReloadBoundary::kDesiredRuntimeApplying,
                        ReloadBoundary::kNetworkPublish,
                        ReloadBoundary::kCorePublish,
                        ReloadBoundary::kWidgetsPublish,
                        ReloadBoundary::kNetworkPostWork,
                        ReloadBoundary::kWidgetsFinish,
                        ReloadBoundary::kCoreForwardWork,
                        ReloadBoundary::kFinalizeTransaction};
                    source_reload_boundaries.clear();
                    passed = passed &&
                             apply_source_directories(successful_paths,
                                                      successful_sounds, false);
                    passed = passed &&
                             source_reload_boundaries == successful_boundaries;
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
                    source_reload_boundaries.clear();
                    passed = passed &&
                             apply_sources(
                                 configuration.dictionary_paths,
                                 configuration.sound_directories, edited_wikis,
                                 configuration.website_sources, empty_forvo,
                                 configuration.dict_server_sources,
                                 edited_programs, false)
                                 .isEmpty();
                    passed = passed &&
                             source_reload_boundaries == successful_boundaries;
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
            0, &window,
            [&app, &configuration_directory, &history, &history_path,
             &window]() {
                QDir().mkpath(configuration_directory);
                history = {{0U, "Alpha"}, {0U, "Beta"}};
                goldendict::core::SaveHistory(history_path.toStdString(),
                                              history);
                window.SetHistoryItems({{QStringLiteral("Alpha"), 0U},
                                        {QStringLiteral("Beta"), 0U}});
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

    const int result = app.exec();
    coordinator.Shutdown();
    window.SetFacade(nullptr);
    facade.reset();
    initial_facade.facade.reset();
    facade_owner.Shutdown();
    network_runtime->Shutdown();
    return result;
}
