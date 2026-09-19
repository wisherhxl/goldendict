// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QWebEngineUrlScheme>
#include <QtTest>
#include "legacy_configuration_location.h"
#include "webengine_storage_paths.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <new>
#ifdef _WIN32
#include <io.h>
#include <malloc.h>
#else
#include <unistd.h>
#endif

#include "../../../modules/core/src/application/core_facade_activation_test_access.h"
#include "../../../modules/network/src/network_runtime_test_access.h"
#include "configuration_reload_transaction_coordinator.h"
#include "main_window.h"

namespace core = goldendict::core;
namespace app = goldendict::app;
namespace network = goldendict::network;
using CoreAccess = core::application::CoreFacadeActivationTestAccess;
using NetworkAccess = network::NetworkRuntimeTestAccess;
using Boundary = app::ConfigurationReloadBoundary;

namespace {
// These counters observe allocation entry points linked into this executable.
// Core/Qt DLL internals and malloc-only paths are not intercepted. Core's
// actual private published allocation is independently observed inside its
// owning DLL.
std::atomic_bool publication_interval{false};
std::atomic_uint64_t publication_allocations{0};
std::atomic_int failing_module{0};
std::atomic_bool decision_published{false};

void CountAllocation() noexcept {
    if (publication_interval.load(std::memory_order_relaxed))
        publication_allocations.fetch_add(1, std::memory_order_relaxed);
}

struct StorageTrace {
    unsigned attempts = 0;
    unsigned constructed = 0;
    unsigned destroyed = 0;
    unsigned deallocated = 0;
    bool after_decision = false;
};

struct Trace {
    std::array<StorageTrace, 2> storage{};
    std::array<Boundary, 64> boundaries{};
    std::size_t boundary_count = 0;
    int fail_module = 0;
    bool later_failure = false;

    bool Observe(int module, int event) noexcept {
        auto& entry = storage[module - 1];
        switch (event) {
            case 0:
                ++entry.attempts;
                entry.after_decision |= decision_published.load();
                if (fail_module == module) {
                    failing_module.store(module);
                    return true;
                }
                break;
            case 1:
                ++entry.constructed;
                entry.after_decision |= decision_published.load();
                break;
            case 2:
                ++entry.destroyed;
                break;
            case 3:
                ++entry.deallocated;
                break;
        }
        return false;
    }
};

bool CoreStorage(void* context, CoreAccess::StorageEvent event) noexcept {
    return static_cast<Trace*>(context)->Observe(1, static_cast<int>(event));
}

bool NetworkStorage(void* context, NetworkAccess::StorageEvent event) noexcept {
    return static_cast<Trace*>(context)->Observe(2, static_cast<int>(event));
}

void Terminated() noexcept {
    const char* marker = "W2_UNEXPECTED_TERMINATION\n";
    int code = 99;
    if (decision_published.load() && failing_module.load() == 1) {
        marker = "W2_CORE_ALLOCATION_AFTER_DURABLE_DECISION\n";
        code = 91;
    } else if (decision_published.load() && failing_module.load() == 2) {
        marker = "W2_NETWORK_ALLOCATION_AFTER_DURABLE_DECISION\n";
        code = 92;
    }
#ifdef _WIN32
    _write(2, marker,
           static_cast<unsigned>(std::char_traits<char>::length(marker)));
#else
    static_cast<void>(write(2, marker, std::char_traits<char>::length(marker)));
#endif
    std::_Exit(code);
}
}  // namespace

void* operator new(std::size_t size) {
    CountAllocation();
    if (void* result = std::malloc(size ? size : 1))
        return result;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    CountAllocation();
    void* result = nullptr;
#ifdef _WIN32
    result =
        _aligned_malloc(size ? size : 1, static_cast<std::size_t>(alignment));
#else
    if (posix_memalign(&result, static_cast<std::size_t>(alignment),
                       size ? size : 1))
        result = nullptr;
#endif
    if (!result)
        throw std::bad_alloc();
    return result;
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}

void operator delete(void* pointer, std::align_val_t) noexcept {
#ifdef _WIN32
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}

void operator delete[](void* pointer, std::align_val_t alignment) noexcept {
    ::operator delete(pointer, alignment);
}

void operator delete(void* pointer, std::size_t,
                     std::align_val_t alignment) noexcept {
    ::operator delete(pointer, alignment);
}

void operator delete[](void* pointer, std::size_t,
                       std::align_val_t alignment) noexcept {
    ::operator delete(pointer, alignment);
}

class PublicationPreparationTest : public QObject {
    Q_OBJECT
   private slots:

    void transaction_data() {
        QTest::addColumn<QString>("scenario");
        for (const char* name :
             {"core-phase", "network-phase", "success", "core-failure",
              "network-failure", "later-failure", "two-transactions",
              "reserved-abort", "decision-write-failure", "reduce-cache",
              "disable-cache"}) {
            QTest::newRow(name) << QString::fromLatin1(name);
        }
    }

    void transaction() {
        QFETCH(QString, scenario);
        if (!qEnvironmentVariableIsSet("GOLDENDICT_W2_CHILD")) {
            QTemporaryDir child_output;
            QVERIFY(child_output.isValid());
            const auto log = child_output.filePath("child.txt");
            QProcess child;
            auto environment = QProcessEnvironment::systemEnvironment();
            environment.insert("GOLDENDICT_W2_CHILD", "1");
            child.setProcessEnvironment(environment);
            child.start(QCoreApplication::applicationFilePath(),
                        {"transaction:" + scenario, "-o", log + ",txt"});
            QVERIFY2(child.waitForStarted(10000),
                     qPrintable(child.errorString()));
            QVERIFY2(child.waitForFinished(60000), "W2 child did not complete");
            QFile output(log);
            QVERIFY(output.open(QIODevice::ReadOnly));
            const auto report = output.readAll() + child.readAllStandardError();
            qInfo().noquote() << report;
            QCOMPARE(child.exitStatus(), QProcess::NormalExit);
            QCOMPARE(child.exitCode(), 0);
            QVERIFY(report.contains("0 failed, 0 skipped"));
            return;
        }

        QTemporaryDir fixture(QDir::tempPath() + "/tx-XXXXXX");
        QVERIFY(fixture.isValid());
        core::CoreConfiguration original;
        original.index_directory = fixture.filePath("indexes").toStdString();
        original.preferences.maximum_network_cache_megabytes =
            scenario == "reduce-cache" ? 3 : 1;
        const auto configuration_path =
            fixture.filePath("config.json").toStdString();
        const auto history_path =
            fixture.filePath("history.json").toStdString();
        const auto cache_root = fixture.filePath("cache").toStdString();
        core::SaveConfiguration(configuration_path, original);
        auto runtime =
            network::NetworkRuntime::Create(network::NetworkRuntime::Prepare(
                {original.preferences.maximum_network_cache_megabytes, false},
                cache_root));
        core::application::DesktopFacadeActivationOwner owner;
        auto initial = owner.PrepareCandidate(original);
        QVERIFY(owner.Activate(initial));
        auto old_facade = owner.CurrentSnapshot();
        const std::weak_ptr<core::DesktopFacade> retired_facade = old_facade;
        const auto* bound_cache = NetworkAccess::BoundDiskCache(*runtime);
        MainWindow window(fixture.path());
        window.SetPreferences(original.preferences);
        window.SetFacade(old_facade.get());
        app::ConfigurationReloadTransactionCoordinator coordinator(
            runtime, owner, window);

        Trace trace;
        trace.fail_module = scenario == "core-failure"      ? 1
                            : scenario == "network-failure" ? 2
                                                            : 0;
        trace.later_failure = scenario == "later-failure";
        decision_published.store(false);
        publication_allocations.store(0);
        failing_module.store(0);
        CoreAccess::ObservePublicationStorage(CoreStorage, &trace);
        NetworkAccess::ObservePublicationStorage(NetworkStorage, &trace);
        const auto stop_observing = qScopeGuard([]() {
            publication_interval.store(false);
            CoreAccess::ObservePublicationStorage(nullptr, nullptr);
            NetworkAccess::ObservePublicationStorage(nullptr, nullptr);
        });

        auto desired = original;
        desired.preferences.maximum_network_cache_megabytes =
            scenario == "disable-cache" ? 0 : 2;
        app::ConfigurationReloadDependencies dependencies;
        dependencies.persistence.checkpoint = [](auto point, const auto&) {
            if (point == core::ConfigurationPersistenceCheckpoint::
                             kAfterDecisionPublished)
                decision_published.store(true);
        };
        dependencies.observe_boundary = [&](Boundary boundary) {
            if (trace.boundary_count < trace.boundaries.size())
                trace.boundaries[trace.boundary_count++] = boundary;
            if (boundary == Boundary::kNetworkPublish)
                publication_interval.store(true);
            if (boundary == Boundary::kWidgetsPublish)
                publication_interval.store(false);
        };
        dependencies.inject_failure = [&](Boundary boundary) {
            return (trace.later_failure &&
                    boundary == Boundary::kWidgetsPrepare) ||
                   (scenario == "reserved-abort" &&
                    boundary == Boundary::kPersistenceDecision);
        };
        dependencies.persistence.filesystem_failure =
            [&](auto operation, const auto&) -> std::optional<std::error_code> {
            if (scenario == "decision-write-failure" &&
                operation ==
                    core::ConfigurationPersistenceOperation::kPublishDecision)
                return std::make_error_code(std::errc::permission_denied);
            return std::nullopt;
        };
        const auto execute = [&]() {
            app::ConfigurationReloadRequest request{
                {configuration_path,
                 history_path,
                 desired,
                 core::PendingHistoryIntent::kUnchanged,
                 {}},
                network::NetworkRuntime::Prepare(
                    {desired.preferences.maximum_network_cache_megabytes,
                     false},
                    cache_root),
                {},
                {}};
            auto execution =
                coordinator.Execute(std::move(request), dependencies);
            if (execution.outcome !=
                    app::ConfigurationReloadOutcome::kPublished &&
                trace.boundary_count)
                qInfo() << "Last boundary"
                        << static_cast<int>(
                               trace.boundaries[trace.boundary_count - 1])
                        << "error"
                        << (execution.error ? execution.error->message.c_str()
                                            : "none");
            return execution;
        };

        app::ConfigurationReloadResult result;
        bool allocation_exception = false;
        try {
            result = execute();
        } catch (const std::bad_alloc&) {
            allocation_exception = true;
        }
        publication_interval.store(false);
        if (trace.fail_module || trace.later_failure ||
            scenario == "reserved-abort" ||
            scenario == "decision-write-failure") {
            QCOMPARE(allocation_exception, trace.fail_module == 2);
            QVERIFY(!decision_published.load());
            QCOMPARE(result.outcome,
                     app::ConfigurationReloadOutcome::kRejectedBeforeDecision);
            QCOMPARE(owner.CurrentSnapshot(), old_facade);
            QCOMPARE(runtime->maximum_cache_bytes(), std::int64_t(1024 * 1024));
            QCOMPARE(core::LoadConfiguration(configuration_path).preferences,
                     original.preferences);
            QVERIFY(!std::filesystem::exists(
                core::PendingConfigurationTransactionPath(configuration_path)));
            QVERIFY(!core::application::IsFullTextIndexExecutorStopped(
                old_facade->GetDictionaryService()));
            if (trace.fail_module == 1) {
                QCOMPARE(trace.storage[1].constructed,
                         1U);  // Network prepares first.
                QCOMPARE(trace.storage[0].constructed, 0U);
            } else if (trace.fail_module == 2) {
                QCOMPARE(trace.storage[0].attempts, 0U);
            }
            for (const auto& entry : trace.storage) {
                QCOMPARE(entry.constructed, entry.destroyed);
                QCOMPARE(entry.constructed, entry.deallocated);
            }
            trace.fail_module = 0;
            trace.later_failure = false;
            dependencies.inject_failure = {};
            dependencies.persistence.filesystem_failure = {};
            QCOMPARE(execute().outcome,
                     app::ConfigurationReloadOutcome::kPublished);
            QVERIFY(owner.CurrentSnapshot() != old_facade);
            QCOMPARE(runtime->maximum_cache_bytes(),
                     std::int64_t(
                         desired.preferences.maximum_network_cache_megabytes) *
                         1024 * 1024);
            QCOMPARE(NetworkAccess::BoundDiskCache(*runtime), bound_cache);
            QCOMPARE(publication_allocations.load(), std::uint64_t(0));
        } else {
            QVERIFY(!allocation_exception);
            QCOMPARE(result.outcome,
                     app::ConfigurationReloadOutcome::kPublished);
            QVERIFY(result.network_published && result.core_published &&
                    result.widgets_published);
            QVERIFY(decision_published.load());
            QVERIFY(owner.CurrentSnapshot() != old_facade);
            QVERIFY(core::application::IsFullTextIndexExecutorStopped(
                old_facade->GetDictionaryService()));
            QVERIFY(!core::application::IsFullTextIndexExecutorStopped(
                owner.CurrentSnapshot()->GetDictionaryService()));
            QCOMPARE(core::LoadConfiguration(configuration_path).preferences,
                     desired.preferences);
            QCOMPARE(runtime->maximum_cache_bytes(),
                     std::int64_t(
                         desired.preferences.maximum_network_cache_megabytes) *
                         1024 * 1024);
            QCOMPARE(NetworkAccess::BoundDiskCache(*runtime), bound_cache);
            const std::array expected{Boundary::kPersistencePrepare,
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
            QCOMPARE(trace.boundary_count, expected.size());
            for (std::size_t i = 0; i < expected.size(); ++i)
                QCOMPARE(trace.boundaries[i], expected[i]);
            if (scenario == "core-phase") {
                QCOMPARE(trace.storage[0].constructed, 1U);
                QVERIFY2(!trace.storage[0].after_decision,
                         "Core published object allocated/constructed after "
                         "actual durable decision");
            } else if (scenario == "network-phase") {
                QCOMPARE(trace.storage[1].constructed, 1U);
                QVERIFY2(!trace.storage[1].after_decision,
                         "Network published object allocated/constructed after "
                         "actual durable decision");
            } else {
                QVERIFY(!trace.storage[0].after_decision &&
                        !trace.storage[1].after_decision);
                QCOMPARE(publication_allocations.load(), std::uint64_t(0));
            }
            if (scenario == "two-transactions") {
                decision_published.store(false);
                auto stale_core = owner.PrepareCandidate(desired);
                auto stale_network = runtime->PrepareCandidate(
                    network::NetworkRuntime::Prepare({2, false}, cache_root));
                auto middle = owner.CurrentSnapshot();
                trace.boundary_count = 0;
                desired.preferences.maximum_network_cache_megabytes = 3;
                QCOMPARE(execute().outcome,
                         app::ConfigurationReloadOutcome::kPublished);
                QVERIFY(owner.CurrentSnapshot() != middle);
                QVERIFY(core::application::IsFullTextIndexExecutorStopped(
                    middle->GetDictionaryService()));
                QVERIFY(!owner.Reserve(stale_core));
                QVERIFY(!NetworkAccess::IsCurrent(*runtime, stale_network));
                stale_core.Abandon();
                stale_network = {};
                QCOMPARE(NetworkAccess::BoundDiskCache(*runtime), bound_cache);
                QCOMPARE(publication_allocations.load(), std::uint64_t(0));
            }
        }
        for (const auto& entry : trace.storage) {
            QCOMPARE(entry.constructed, entry.destroyed);
            QCOMPARE(entry.constructed, entry.deallocated);
        }
        QVERIFY(!retired_facade.expired());
        old_facade.reset();
        QVERIFY(retired_facade.expired());
    }
};

int main(int argc, char** argv) {
    QTemporaryDir profile(QDir::tempPath() + "/w2-XXXXXX");
    if (!profile.isValid())
        return 2;
    // Children inherit the parent's isolated profile. Nesting another profile
    // at each subprocess level would exceed Windows transaction path limits.
    if (!qEnvironmentVariableIsSet("GOLDENDICT_W2_CHILD"))
        for (const auto* name :
             {"HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "APPDATA",
              "LOCALAPPDATA", "GOLDENDICT_TEST_CONFIG_ROOT", "TEMP", "TMP"}) {
            auto directory = profile.filePath(name);
            if (!QDir().mkpath(directory))
                return 2;
            qputenv(name, directory.toUtf8());
        }
    std::set_terminate(Terminated);
    QWebEngineUrlScheme scheme("goldendict");
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(0);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                    QWebEngineUrlScheme::LocalScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
    QTemporaryDir webengine_storage;
    if (!webengine_storage.isValid())
        return 2;
    QApplication application(argc, argv);
    goldendict::app::InitializeWebEngineStorage(
        {}, webengine_storage.filePath("webengine"));
    PublicationPreparationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "publication_preparation_test.moc"
