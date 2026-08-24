// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QUrl>
#include <QtTest>

#include "help_window.h"

class HelpWindowTest final : public QObject {
    Q_OBJECT

   private slots:
    void SelectsBoundedLegacyLocale();
    void OpensInstalledCollections();
    void RejectsMissingAndCorruptCollections();
    void FiltersExternalLinks();
};

void HelpWindowTest::SelectsBoundedLegacyLocale() {
    using goldendict::app::SelectHelpCollectionName;
    QCOMPARE(SelectHelpCollectionName(QStringLiteral("ru_RU"),
                                      QStringLiteral("en_US"),
                                      QStringLiteral("en_US")),
             QStringLiteral("gdhelp_ru.qch"));
    QCOMPARE(SelectHelpCollectionName(QStringLiteral("fr_FR"),
                                      QStringLiteral("ru_RU"),
                                      QStringLiteral("ru_RU")),
             QStringLiteral("gdhelp_en.qch"));
    QCOMPARE(SelectHelpCollectionName({}, QStringLiteral("ru_UA"),
                                      QStringLiteral("en_US")),
             QStringLiteral("gdhelp_ru.qch"));
    QCOMPARE(SelectHelpCollectionName({}, {}, QStringLiteral("ru_RU")),
             QStringLiteral("gdhelp_ru.qch"));
    QCOMPARE(SelectHelpCollectionName(QStringLiteral("../../ru"), {}, {}),
             QStringLiteral("gdhelp_en.qch"));
}

void HelpWindowTest::OpensInstalledCollections() {
    const QString directory = qEnvironmentVariable("GOLDENDICT_HELP_TEST_DIR");
    QVERIFY(!directory.isEmpty());
    for (const auto& locale :
         {QStringLiteral("en_US"), QStringLiteral("ru_RU")}) {
        goldendict::app::HelpWindow window(directory, locale, {}, {}, nullptr);
        QVERIFY(window.IsReady());
        QVERIFY(QFile::exists(window.CollectionPath()));
        QVERIFY(window.ShowIdentifier(QStringLiteral("Content")));
        QCOMPARE(window.windowTitle(), QStringLiteral("GoldenDict help"));
        QVERIFY(!(window.windowFlags() & Qt::WindowContextHelpButtonHint));
        auto* browser =
            window.findChild<QTextBrowser*>(QStringLiteral("helpBrowser"));
        auto* zoom_in =
            window.findChild<QAction*>(QStringLiteral("helpZoomInAction"));
        auto* zoom_out =
            window.findChild<QAction*>(QStringLiteral("helpZoomOutAction"));
        auto* normal =
            window.findChild<QAction*>(QStringLiteral("helpNormalSizeAction"));
        QVERIFY(browser != nullptr);
        QVERIFY(zoom_in != nullptr);
        QVERIFY(zoom_out != nullptr);
        QVERIFY(normal != nullptr);
        for (int index = 0; index < 30; ++index)
            zoom_in->trigger();
        QVERIFY(!zoom_in->isEnabled());
        QVERIFY(zoom_out->isEnabled());
        normal->trigger();
        QVERIFY(zoom_in->isEnabled());
        QVERIFY(zoom_out->isEnabled());
        QVERIFY(!normal->isEnabled());
    }
}

void HelpWindowTest::FiltersExternalLinks() {
    using goldendict::app::IsExternalHelpUrl;
    QVERIFY(IsExternalHelpUrl(QUrl(QStringLiteral("https://goldendict.org/"))));
    QVERIFY(IsExternalHelpUrl(QUrl(QStringLiteral("http://example.test/"))));
    QVERIFY(!IsExternalHelpUrl(QUrl(QStringLiteral("qthelp://org/page.html"))));
    QVERIFY(!IsExternalHelpUrl(QUrl(QStringLiteral("file:///tmp/help.html"))));
    QVERIFY(!IsExternalHelpUrl(QUrl(QStringLiteral("javascript:alert(1)"))));
}

void HelpWindowTest::RejectsMissingAndCorruptCollections() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    goldendict::app::HelpWindow missing(directory.path(), {}, {}, {}, nullptr);
    QVERIFY(!missing.IsReady());

    QFile corrupt(
        QDir(directory.path()).filePath(QStringLiteral("gdhelp_en.qch")));
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    QCOMPARE(corrupt.write("not a Qt help collection"), 24);
    corrupt.close();
    goldendict::app::HelpWindow invalid(directory.path(), {}, {}, {}, nullptr);
    QVERIFY(!invalid.IsReady());
}

QTEST_MAIN(HelpWindowTest)

#include "help_window_test.moc"
