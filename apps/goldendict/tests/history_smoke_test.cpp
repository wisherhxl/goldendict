// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "goldendict/core/application.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QListWidget>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebEngineUrlScheme>
#include <QtTest>
#include "history_application.h"
#include "legacy_configuration_location.h"
#include "preferences_application.h"
#include "webengine_storage_paths.h"
#include "widgets_presentation_host.h"

namespace {
void PrepareFixture(const std::filesystem::path& root) {
    if (root.empty() || !root.is_absolute() || std::filesystem::exists(root))
        throw std::runtime_error("Fixture requires a new absolute owned root");
    std::filesystem::create_directories(root / "current-config");
    std::filesystem::create_directories(root / "indexes");
    goldendict::core::CoreConfiguration configuration;
    configuration.index_directory = (root / "indexes").generic_string();
    const auto path = (root / "current-config/core.conf").generic_string();
    goldendict::core::SaveConfiguration(path, configuration);
    const auto loaded = goldendict::core::LoadConfiguration(path);
    if (!std::filesystem::is_regular_file(path) ||
        loaded.index_directory != configuration.index_directory ||
        !(loaded.preferences == configuration.preferences))
        throw std::runtime_error("Fixture serialization round-trip failed");
    std::cout << "Owned fixture saved and loaded: " << path << '\n';
}
}  // namespace

namespace core = goldendict::core;
namespace app = goldendict::app;

class HistorySmokeTest : public QObject {
    Q_OBJECT
   public:
    explicit HistorySmokeTest(QString owned_root)
        : owned_root_(std::move(owned_root)) {}

   private:
    const QString owned_root_;
   private slots:

    void scenarioThroughRealApplication() {
        const auto profile = owned_root_ + "/profile";
        qInfo() << "Preparing owned fixture" << profile;
        PrepareFixture(profile.toStdString());
        qInfo() << "Fixture round-trip complete";
        const auto configuration_path = profile + "/current-config/core.conf";
        const auto history_path = profile + "/history";
        auto configuration =
            core::LoadConfiguration(configuration_path.toStdString());
        std::vector<core::HistoryEntry> history;
        const auto network_root = (profile + "/network-cache").toStdString();
        auto runtime = goldendict::network::NetworkRuntime::Create(
            goldendict::network::NetworkRuntime::Prepare({}, network_root));
        const goldendict::network::ForvoCredentialMap credentials;
        core::application::DesktopFacadeActivationOwner owner;
        auto initial = app::PrepareProductionFacade(configuration, credentials,
                                                    runtime, owner);
        QVERIFY(owner.Activate(initial.candidate));
        auto facade = initial.facade;
        auto diagnostics = std::move(initial.diagnostics);
        qInfo() << "Initial production facade activated";
        MainWindow window(profile);
        window.SetPreferences(configuration.preferences);
        window.SetNetworkCacheDirectory(
            QString::fromStdString(runtime->cache_directory()));
        window.RestoreMainWindowGeometry(configuration.main_window_geometry);
        window.RestoreMainWindowState(configuration.main_window_state);
        window.SetFullTextDialogGeometry(
            configuration.full_text_dialog_geometry);
        window.SetInspectorGeometry(configuration.inspector_geometry);
        window.SetDictionaryGroups(configuration.dictionary_groups);
        window.SetSourceDirectories(configuration.dictionary_paths,
                                    configuration.sound_directories);
        window.SetFacade(facade.get());
        app::ConfigurationReloadTransactionCoordinator coordinator(
            runtime, owner, window);
        app::InstallHistoryRecording(window, configuration, history,
                                     history_path);
        window.show();
        QApplication::processEvents();
        auto previous_facade = facade;
        const auto shutdown = qScopeGuard([&]() {
            coordinator.Shutdown();
            window.SetFacade(nullptr);
            previous_facade.reset();
            facade.reset();
            initial.facade.reset();
            owner.Shutdown();
            runtime->Shutdown();
            qInfo() << "Execution-chain resource cleanup complete";
        });
        QTimer::singleShot(10000, &window, []() { QCoreApplication::exit(2); });
        auto* list = window.findChild<QListWidget*>("historyList");
        auto* groups = window.findChild<QComboBox*>("groupSelector");
        QVERIFY(list);
        QVERIFY(groups);
        const QString expected = QStringLiteral("history-smoke-entry");
        window.SetDictionaryGroups({{7U, "History Smoke Group", "", {}}});
        const int group_seven = groups->findData(7U);
        const int group_zero = groups->findData(0U);
        QVERIFY(group_seven >= 0);
        QVERIFY(group_zero >= 0);
        bool completed = false;
        bool passed = false;
        QObject::connect(
            &window, &MainWindow::LookupSubmitted, &window,
            [&](const QString& submitted, std::uint32_t group_id) {
                const bool recorded =
                    submitted == expected && group_id == 7U &&
                    list->count() > 0 && list->item(0)->text() == expected &&
                    list->item(0)->data(Qt::UserRole).value<quint32>() == 7U;
                groups->setCurrentIndex(group_zero);
                QObject::connect(
                    &window, &MainWindow::LookupSubmitted, &window,
                    [&, recorded](const QString& restored,
                                  std::uint32_t restored_group) {
                        completed = true;
                        passed = recorded && restored == expected &&
                                 restored_group == 7U;
                    },
                    Qt::SingleShotConnection);
                emit list->itemActivated(list->item(0));
            },
            Qt::SingleShotConnection);
        groups->setCurrentIndex(group_seven);
        window.SubmitInitialLookup(expected);
        QVERIFY(completed);
        QVERIFY(passed);
        const auto persisted = core::LoadHistory(history_path.toStdString());
        QVERIFY(!persisted.empty());
        QCOMPARE(persisted.front().word, std::string("history-smoke-entry"));
        QCOMPARE(persisted.front().group_id, 7U);
        QVERIFY2(
            qEnvironmentVariableIntValue("GOLDENDICT_TEST_EXPECT_FAILURE") != 1,
            "controlled execution-chain assertion failure");
    }
};

int main(int argc, char** argv) {
    QTemporaryDir profile(QDir::tempPath() + "/hs-XXXXXX");
    if (!profile.isValid())
        return 2;
    for (const auto* name :
         {"HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "APPDATA",
          "LOCALAPPDATA", "GOLDENDICT_TEST_CONFIG_ROOT", "TEMP", "TMP"}) {
        const auto path = profile.filePath(QString::fromLatin1(name));
        if (!QDir().mkpath(path))
            return 2;
        qputenv(name, path.toUtf8());
    }
    QWebEngineUrlScheme scheme(QByteArrayLiteral("goldendict"));
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
    HistorySmokeTest test(profile.path());
    int test_result = 2;
    QTimer::singleShot(0, &application, [&]() {
        test_result = QTest::qExec(&test, argc, argv);
        application.exit(test_result);
    });
    const int event_loop_result = application.exec();
    const int process_result =
        test_result != 0 ? test_result : event_loop_result;
    std::cerr << "Execution-chain results: qtest=" << test_result
              << " event_loop=" << event_loop_result
              << " process=" << process_result << '\n';
    return process_result;
}

#include "history_smoke_test.moc"
