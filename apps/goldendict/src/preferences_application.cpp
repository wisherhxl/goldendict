// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences_application.h"

#include <QCoreApplication>
#include <QDebug>
#include <stdexcept>
#include <utility>

namespace goldendict::app {
using ReloadBoundary = ConfigurationReloadBoundary;

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

PreparedProductionFacade PrepareProductionFacade(
    const goldendict::core::CoreConfiguration& configuration,
    const goldendict::network::ForvoCredentialMap& forvo_credentials,
    const std::shared_ptr<goldendict::network::NetworkRuntime>& network_runtime,
    goldendict::core::application::DesktopFacadeActivationOwner& owner,
    bool require_session_restoration) {
    auto composition = goldendict::network::ComposeConfiguredRuntimeSources(
        configuration, forvo_credentials, network_runtime);
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

MainWindow::PreferencesApplyCallback InstallPreferencesApplication(
    MainWindow& window, PreferencesApplicationBindings bindings,
    std::function<bool(ConfigurationReloadBoundary)> inject_failure) {
    MainWindow::PreferencesApplyCallback callback =
        [bindings = std::move(bindings),
         inject_failure = std::move(inject_failure)](
            const core::ApplicationPreferences& preferences) {
            auto& configuration = bindings.configuration;
            auto& history = bindings.history;
            auto& facade = bindings.facade;
            auto& facade_owner = bindings.facade_owner;
            auto& network_runtime = bindings.network_runtime;
            auto& coordinator = bindings.coordinator;
            auto& forvo_credentials = bindings.forvo_credentials;
            auto& composition_diagnostics = bindings.composition_diagnostics;
            auto& configuration_path = bindings.configuration_path;
            auto& history_path = bindings.history_path;
            auto& network_cache_root = bindings.network_cache_root;
            auto& refresh_history = bindings.refresh_history;
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
                            updated, forvo_credentials, network_runtime,
                            facade_owner);
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
                dependencies.inject_failure = inject_failure;
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
        };
    window.SetPreferencesApplyCallback(callback);
    return callback;
}

}  // namespace goldendict::app
