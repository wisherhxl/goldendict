// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QSet>
#include <QStringList>

namespace {

class LegacyCatalogSourceInventoryTest : public QObject {
    Q_OBJECT

   private slots:
    void CompletePinnedInventoryIsPresent();
};

void LegacyCatalogSourceInventoryTest::CompletePinnedInventoryIsPresent() {
    const QSet<QString> expected = {
        "ar_SA.ts", "ay_WI.ts", "be_BY.ts", "be_BY@latin.ts", "bg_BG.ts",
        "ca_CT.ts", "cs_CZ.ts", "de_DE.ts", "el_GR.ts",       "eo_EO.ts",
        "es_AR.ts", "es_BO.ts", "es_ES.ts", "fa_IR.ts",       "fi_FI.ts",
        "fr_FR.ts", "hi_IN.ts", "hu_HU.ts", "ie_001.ts",      "it_IT.ts",
        "ja_JP.ts", "jb_JB.ts", "ko_KR.ts", "lt_LT.ts",       "mk_MK.ts",
        "nl_NL.ts", "pl_PL.ts", "pt_BR.ts", "pt_PT.ts",       "qt_es.ts",
        "qt_it.ts", "qt_lt.ts", "qu_WI.ts", "ru_RU.ts",       "sk_SK.ts",
        "sq_AL.ts", "sr_SR.ts", "sv_SE.ts", "tg_TJ.ts",       "tk_TM.ts",
        "tr_TR.ts", "uk_UA.ts", "vi_VN.ts", "zh_CN.ts",       "zh_TW.ts"};

    const QDir source_directory(GOLDENDICT_CATALOG_SOURCE_DIRECTORY);
    const QStringList actual_files = source_directory.entryList(
        {"*.ts"}, QDir::Files | QDir::NoSymLinks, QDir::Name);
    QCOMPARE(QSet<QString>(actual_files.cbegin(), actual_files.cend()),
             expected);
    QCOMPARE(actual_files.size(), 45);

    QFile build_inventory(source_directory.filePath("catalog_sources.cmake"));
    QVERIFY2(build_inventory.open(QIODevice::ReadOnly),
             qPrintable(build_inventory.errorString()));
    const QByteArray build_inventory_contents = build_inventory.readAll();
    QVERIFY(build_inventory_contents.contains(
        "set(GOLDENDICT_ENABLED_CATALOG_SOURCES\n  ru_RU.ts)"));
    for (const QString& file_name : actual_files) {
        if (file_name != "ru_RU.ts") {
            QVERIFY2(
                build_inventory_contents.contains(("  " + file_name).toUtf8()),
                qPrintable(file_name));
        }
    }

    for (const QString& file_name : actual_files) {
        QFile source(source_directory.filePath(file_name));
        QVERIFY2(source.open(QIODevice::ReadOnly),
                 qPrintable(source.errorString()));
        const QByteArray contents = source.readAll();
        QVERIFY(
            contents.startsWith("<?xml version=\"1.0\" encoding=\"utf-8\"?>"));
        QVERIFY2(contents.contains("<TS version=\"2.0\"") ||
                     contents.contains("<TS version=\"2.1\""),
                 qPrintable(file_name));
        QVERIFY(contents.endsWith("</TS>\n"));
    }
}

}  // namespace

QTEST_APPLESS_MAIN(LegacyCatalogSourceInventoryTest)

#include "legacy_catalog_source_inventory_test.moc"
