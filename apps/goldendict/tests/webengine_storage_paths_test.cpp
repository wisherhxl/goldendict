// SPDX-License-Identifier: GPL-3.0-or-later
#include "webengine_storage_paths.h"
#include "article_inspector.h"
#include "legacy_configuration_location.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <QWebEngineProfile>
#include <QWebEngineView>

// Each selection case starts with a fresh Qt singleton and test-owned identity.
int CheckFreshSelection(const QString& mode, const QString& root) {
    goldendict::app::ConfigurationLocations locations;
    locations.portable = mode != "nonportable";
    locations.current_configuration_path =
        QDir(root).filePath("core.conf").toStdString();
    std::optional<QString> override;
    if (mode == "explicit")
        override = QDir(root).filePath("explicit");
    if (mode == "empty")
        override = QString();
    if (mode == "blocked") {
        override = QDir(root).filePath("blocked");
        QFile file(*override);
        if (!file.open(QIODevice::WriteOnly))
            return 10;
    }
    const auto fallback =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        "/QtWebEngine/OffTheRecord";
    try {
        goldendict::app::InitializeWebEngineStorage(locations, override);
        if (mode == "empty" || mode == "blocked")
            return 11;
    } catch (const std::exception&) {
        if (mode != "empty" && mode != "blocked")
            return 12;
        if (QFileInfo::exists(fallback))
            return 13;
        return 0;
    }
    auto* profile = QWebEngineProfile::defaultProfile();
    const auto expected =
        mode == "nonportable"
            ? fallback
            : QDir(root).filePath(mode == "explicit" ? "explicit/article"
                                                     : "webengine/article");
    if (profile->persistentStoragePath() != expected)
        return 14;
    if (!profile->isOffTheRecord() ||
        profile->httpCacheType() != QWebEngineProfile::MemoryHttpCache ||
        profile->persistentCookiesPolicy() !=
            QWebEngineProfile::NoPersistentCookies ||
        profile->persistentPermissionsPolicy() !=
            QWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory)
        return 15;
    if (mode == "explicit" &&
        QFileInfo::exists(QDir(root).filePath("webengine")))
        return 16;
    return 0;
}

class WebEngineStoragePathsTest : public QObject {
    Q_OBJECT
   public:
    QString root;

   private slots:

    void freshSelection_data() {
        QTest::addColumn<QString>("mode");
        for (const auto* mode :
             {"portable", "explicit", "nonportable", "empty", "blocked"})
            QTest::newRow(mode) << QString(mode);
    }

    void freshSelection() {
        QFETCH(QString, mode);
        QProcess process;
        process.start(QCoreApplication::applicationFilePath(),
                      {"--selection-case", mode});
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished(20000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    }

    void configuredBeforeRealPage() {
        goldendict::app::ConfigurationLocations locations;
        locations.portable = true;
        locations.current_configuration_path =
            QDir(root).filePath("core.conf").toStdString();
        goldendict::app::InitializeWebEngineStorage(locations);
        auto* profile = QWebEngineProfile::defaultProfile();
        QCOMPARE(profile->persistentStoragePath(),
                 QDir(root).filePath("webengine/article"));
        QVERIFY(profile->isOffTheRecord());
        QCOMPARE(profile->httpCacheType(), QWebEngineProfile::MemoryHttpCache);
        QCOMPARE(profile->persistentCookiesPolicy(),
                 QWebEngineProfile::NoPersistentCookies);
        QCOMPARE(profile->persistentPermissionsPolicy(),
                 QWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory);
        QWebEnginePage page;
        QCOMPARE(page.profile(), profile);
        QSignalSpy loaded(&page, &QWebEnginePage::loadFinished);
        page.setHtml("<title>P1 real page</title><p>owned content</p>");
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 15000);
        QVERIFY(loaded.last().at(0).toBool());
        QCOMPARE(page.title(), QString("P1 real page"));
        const auto path = profile->persistentStoragePath();
        goldendict::app::InitializeWebEngineStorage(locations);
        QCOMPARE(profile->persistentStoragePath(), path);
        goldendict::app::ConfigurationLocations nonportable;
        goldendict::app::InitializeWebEngineStorage(nonportable);
        QCOMPARE(profile->persistentStoragePath(), path);
    }

    void rejectsInvalidAndConflictingPathsWithoutFallback() {
        const auto path =
            QWebEngineProfile::defaultProfile()->persistentStoragePath();
        goldendict::app::ConfigurationLocations locations;
        QVERIFY_EXCEPTION_THROWN(
            goldendict::app::InitializeWebEngineStorage(locations, QString()),
            std::exception);
        QVERIFY_EXCEPTION_THROWN(goldendict::app::InitializeWebEngineStorage(
                                     locations, QString("relative")),
                                 std::exception);
        QFile file(QDir(root).filePath("not-a-directory"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY_EXCEPTION_THROWN(goldendict::app::InitializeWebEngineStorage(
                                     locations, file.fileName()),
                                 std::exception);
        QVERIFY_EXCEPTION_THROWN(
            goldendict::app::InitializeWebEngineStorage(
                locations, QDir(root).filePath("different")),
            std::exception);
        QCOMPARE(QWebEngineProfile::defaultProfile()->persistentStoragePath(),
                 path);
        QVERIFY(!QFileInfo::exists(QDir(root).filePath("different")));
    }

    void inspectorOwnsIndependentDirectoryUntilProfileDestruction() {
        QFile sentinel(QDir(root).filePath("webengine/adjacent-sentinel"));
        QVERIFY(sentinel.open(QIODevice::WriteOnly));
        QCOMPARE(sentinel.write("preserve"), qint64(8));
        sentinel.close();
        QWebEnginePage page;
        auto* profile = page.profile();
        QString inspector_path;
        bool page_destroyed = false;
        bool profile_destroyed = false;
        bool destruction_order = false;
        {
            ArticleInspector first(&page);
            auto* tools = page.devToolsPage();
            QVERIFY(tools);
            QVERIFY(tools->profile() != profile);
            auto* inspector_profile = tools->profile();
            inspector_path = inspector_profile->persistentStoragePath();
            QVERIFY(inspector_path.startsWith(
                QDir(root).filePath("webengine/inspectors/")));
            QVERIFY(inspector_profile->isOffTheRecord());
            QCOMPARE(inspector_profile->httpCacheType(),
                     QWebEngineProfile::MemoryHttpCache);
            QCOMPARE(inspector_profile->persistentCookiesPolicy(),
                     QWebEngineProfile::NoPersistentCookies);
            QCOMPARE(
                inspector_profile->persistentPermissionsPolicy(),
                QWebEngineProfile::PersistentPermissionsPolicy::StoreInMemory);
            connect(tools, &QObject::destroyed, this,
                    [&] { page_destroyed = true; });
            connect(inspector_profile, &QObject::destroyed, this, [&] {
                profile_destroyed = true;
                destruction_order =
                    page_destroyed && QFileInfo::exists(inspector_path);
            });
            QWebEnginePage other;
            ArticleInspector second(&other);
            QVERIFY(other.devToolsPage()->profile()->persistentStoragePath() !=
                    inspector_path);
            first.Inspect(false);
            QTRY_VERIFY_WITH_TIMEOUT(tools->renderProcessPid() > 0, 15000);
        }
        QVERIFY(page_destroyed);
        QVERIFY(profile_destroyed);
        QVERIFY(destruction_order);
        QVERIFY(!QFileInfo::exists(inspector_path));
        QVERIFY(!page.devToolsPage());
        QCOMPARE(page.profile(), profile);
        QVERIFY(sentinel.open(QIODevice::ReadOnly));
        QCOMPARE(sentinel.readAll(), QByteArray("preserve"));
    }

    void inspectorFailureDoesNotAttachOrFallBack() {
        const auto directory = QDir(root).filePath("webengine/inspectors");
        QVERIFY(QDir().rmdir(directory));
        QFile blocker(directory);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QWebEnginePage page;
        const auto path = page.profile()->persistentStoragePath();
        QVERIFY_EXCEPTION_THROWN(ArticleInspector inspector(&page),
                                 std::exception);
        QVERIFY(!page.devToolsPage());
        QCOMPARE(page.profile()->persistentStoragePath(), path);
        QVERIFY(blocker.remove());
        QVERIFY(QDir().mkpath(directory));
    }
};

int main(int argc, char** argv) {
    QTemporaryDir directory;
    if (!directory.isValid())
        return 2;
    // Safe red/default controls have a unique identity, never GoldenDict's.
    QCoreApplication::setApplicationName(
        "GDStorageTest-" + QUuid::createUuid().toString(QUuid::Id128));
    QCoreApplication::setOrganizationName("GDStorageTest");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    if (argc == 3 && QByteArray(argv[1]) == "--selection-case")
        return CheckFreshSelection(QString::fromLocal8Bit(argv[2]),
                                   directory.path());
    WebEngineStoragePathsTest test;
    test.root = directory.path();
    return QTest::qExec(&test, argc, argv);
}

#include "webengine_storage_paths_test.moc"
