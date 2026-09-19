// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "../src/legacy_configuration_location.h"
#include "goldendict/network/network_runtime.h"

namespace {
using goldendict::app::ConfigurationLocations;
using goldendict::app::ResolveNetworkCacheRoot;
using goldendict::network::NetworkRuntime;

ConfigurationLocations Portable(const QTemporaryDir& directory) {
    goldendict::app::LegacyConfigurationEnvironment environment;
    environment.application_directory = directory.path().toStdString();
    environment.current_config_directory =
        environment.application_directory / "default-config";
    QDir().mkpath(directory.filePath("portable"));
    return goldendict::app::ResolveConfigurationLocations(
        environment, goldendict::app::ProbePath);
}

bool WriteSentinel(const QString& path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("sentinel") == 8;
}

bool SentinelIntact(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && file.readAll() == "sentinel";
}

class PortableNetworkCacheTest final : public QObject {
    Q_OBJECT
   private slots:

    void SelectsPortableBeforeRealPreparation() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto locations = Portable(directory);
        QVERIFY(locations.portable);
        const auto root = ResolveNetworkCacheRoot(
            locations, directory.filePath("simulated-default").toStdString());
        QCOMPARE(QString::fromStdString(root),
                 directory.filePath("portable/cache"));
        const auto preparation = NetworkRuntime::Prepare({1U, false}, root);
        QVERIFY(preparation.cache_available);
        QCOMPARE(QString::fromStdString(preparation.cache_directory),
                 directory.filePath("portable/cache/qt-network-http"));
        QVERIFY(!QFileInfo::exists(directory.filePath("simulated-default")));
        auto runtime = NetworkRuntime::Create(preparation);
        QCOMPARE(runtime->cache_directory(), preparation.cache_directory);
        QCOMPARE(runtime->maximum_cache_bytes(), 1024LL * 1024LL);
        runtime->Shutdown();
    }

    void PreservesExplicitAndNonPortableWithoutIo() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ConfigurationLocations locations;
        const auto fallback =
            directory.filePath("uncreated-default").toStdString();
        const auto explicit_root =
            directory.filePath("explicit/../exact-root").toStdString();
        QCOMPARE(ResolveNetworkCacheRoot(locations, fallback), fallback);
        QCOMPARE(ResolveNetworkCacheRoot(locations, fallback, explicit_root),
                 explicit_root);
        locations = Portable(directory);
        QCOMPARE(ResolveNetworkCacheRoot(locations, fallback, explicit_root),
                 explicit_root);
        QVERIFY(!QFileInfo::exists(QString::fromStdString(fallback)));
        QVERIFY(!QFileInfo::exists(directory.filePath("explicit")));
        QVERIFY(!QFileInfo::exists(directory.filePath("portable/cache")));
    }

    void UnavailablePortableNeverFallsBack() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto locations = Portable(directory);
        const auto sentinel = directory.filePath("simulated-default/sentinel");
        QVERIFY(WriteSentinel(sentinel));
        QVERIFY(WriteSentinel(directory.filePath("portable/cache")));
        const auto root = ResolveNetworkCacheRoot(
            locations, directory.filePath("simulated-default").toStdString());
        QCOMPARE(QString::fromStdString(root),
                 directory.filePath("portable/cache"));
        const auto preparation = NetworkRuntime::Prepare({1U, false}, root);
        QVERIFY(!preparation.cache_available);
        QVERIFY(!preparation.diagnostic.empty());
        auto runtime = NetworkRuntime::Create(preparation);
        // This accessor preserves policy, including when disk setup fails.
        QCOMPARE(runtime->maximum_cache_bytes(), 1024LL * 1024LL);
        QCOMPARE(runtime->diagnostic(), preparation.diagnostic);
        QCOMPARE(runtime->cache_directory(), preparation.cache_directory);
        runtime->Shutdown();
        QVERIFY(SentinelIntact(sentinel));
        QVERIFY(!QFileInfo::exists(
            directory.filePath("simulated-default/qt-network-http")));
        QVERIFY(SentinelIntact(directory.filePath("portable/cache")));
    }

    void ReapplyAbandonAbortAndCleanupStayInOwnedChild() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto locations = Portable(directory);
        const auto root = ResolveNetworkCacheRoot(
            locations, directory.filePath("simulated-default").toStdString());
        QCOMPARE(QString::fromStdString(root),
                 directory.filePath("portable/cache"));
        const auto sibling =
            directory.filePath("portable/cache/sibling/sentinel");
        const auto default_sentinel =
            directory.filePath("simulated-default/qt-network-http/sentinel");
        const auto owned = directory.filePath("portable/cache/qt-network-http");
        QVERIFY(WriteSentinel(sibling));
        QVERIFY(WriteSentinel(default_sentinel));
        auto runtime =
            NetworkRuntime::Create(NetworkRuntime::Prepare({1U, false}, root));
        QVERIFY(WriteSentinel(owned + "/sentinel"));
        {
            auto candidate = runtime->PrepareCandidate(
                NetworkRuntime::Prepare({0U, false}, root));
            QVERIFY(candidate);
        }
        QVERIFY(SentinelIntact(owned + "/sentinel"));
        {
            auto candidate = runtime->PrepareCandidate(
                NetworkRuntime::Prepare({0U, false}, root));
            auto reservation = runtime->Reserve(candidate);
            QVERIFY(reservation);
            runtime->Abort(reservation);
        }
        QVERIFY(SentinelIntact(owned + "/sentinel"));
        QCOMPARE(runtime->maximum_cache_bytes(), 1024LL * 1024LL);
        auto disabled = runtime->PrepareCandidate(
            NetworkRuntime::Prepare({0U, false}, root));
        QCOMPARE(runtime->Commit(disabled),
                 NetworkRuntime::CommitResult::kPublished);
        QVERIFY(!disabled);
        QCOMPARE(runtime->Commit(disabled),
                 NetworkRuntime::CommitResult::kRejected);
        QCOMPARE(runtime->maximum_cache_bytes(), 0LL);
        QVERIFY(!QFileInfo::exists(owned));
        auto enabled = runtime->PrepareCandidate(
            NetworkRuntime::Prepare({2U, true}, root));
        QCOMPARE(runtime->Commit(enabled),
                 NetworkRuntime::CommitResult::kPublished);
        QCOMPARE(QString::fromStdString(runtime->cache_directory()), owned);
        QCOMPARE(runtime->maximum_cache_bytes(), 2LL * 1024LL * 1024LL);
        QVERIFY(WriteSentinel(owned + "/sentinel"));
        runtime->Shutdown();
        QVERIFY(!QFileInfo::exists(owned));
        QVERIFY(SentinelIntact(sibling));
        QVERIFY(SentinelIntact(default_sentinel));
        // A new disabled startup also cleans only its selected owned child.
        QVERIFY(WriteSentinel(owned + "/sentinel"));
        runtime =
            NetworkRuntime::Create(NetworkRuntime::Prepare({0U, false}, root));
        QVERIFY(!QFileInfo::exists(owned));
        runtime->Shutdown();
        QVERIFY(SentinelIntact(sibling));
        QVERIFY(SentinelIntact(default_sentinel));
    }
};
}  // namespace

QTEST_GUILESS_MAIN(PortableNetworkCacheTest)
#include "portable_network_cache_test.moc"
