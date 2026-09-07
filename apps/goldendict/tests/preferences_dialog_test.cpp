// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCheckBox>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QtTest>

#include "preferences_dialog.h"

namespace {

using goldendict::core::ApplicationPreferences;

QWidget* Page(PreferencesDialog& dialog, const char* name) {
    return dialog.findChild<QWidget*>(QString::fromLatin1(name));
}

void VerifyCell(QGridLayout* layout, QWidget* widget, int row, int column) {
    QVERIFY(layout != nullptr);
    QVERIFY(widget != nullptr);
    QVERIFY(layout->itemAtPosition(row, column) != nullptr);
    QCOMPARE(layout->itemAtPosition(row, column)->widget(), widget);
}

class PreferencesDialogTest final : public QObject {
    Q_OBJECT

   private slots:

    void initTestCase() {
        if (!qEnvironmentVariableIsEmpty(
                "GOLDENDICT_PREFERENCES_CAPTURE_DIR")) {
            QApplication::setFont(QFont("Segoe UI", 9));
        }
    }

    void BackedPageOrderAndOwnership() {
        PreferencesDialog dialog({}, [](const auto&) { return QString(); }, {});
        auto* tabs = dialog.findChild<QTabWidget*>("preferencesTabs");
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 3);
        QCOMPARE(tabs->tabText(0), QString("&Interface"));
        QCOMPARE(tabs->tabText(1), QString("&Network"));
        QCOMPARE(tabs->tabText(2), QString("Ad&vanced"));
        QCOMPARE(tabs->currentIndex(), 0);
        QCOMPARE(tabs->iconSize(), QSize(15, 15));
        QVERIFY(!tabs->usesScrollButtons());
        QVERIFY(!tabs->tabIcon(0).isNull());
        QVERIFY(!tabs->tabIcon(1).isNull());
        QVERIFY(tabs->tabIcon(2).isNull());
        QVERIFY(!dialog.windowIcon().isNull());
        QVERIFY(dialog.isModal());
        auto* interface_page = Page(dialog, "preferencesInterfacePage");
        auto* advanced_page = Page(dialog, "preferencesAdvancedPage");
        QVERIFY(interface_page != nullptr);
        QVERIFY(advanced_page != nullptr);
        QCOMPARE(tabs->widget(0), interface_page);
        QCOMPARE(tabs->widget(2), advanced_page);
        QVERIFY(Page(dialog, "preferencesGeneralPage") == nullptr);
        for (const char* name :
             {"newTabsOpenInBackground", "hideSingleTab",
              "doubleClickTranslates", "selectBySingleClick",
              "escKeyHidesMainWindow", "maxDictsInContextMenu"}) {
            QVERIFY(interface_page->isAncestorOf(Page(dialog, name)));
        }
        for (const char* name :
             {"storeHistory", "historyMaxSizeField", "confirmFavoritesDeletion",
              "collapseBigArticles", "articleSizeLimit",
              "limitInputPhraseLength", "inputPhraseLengthLimit",
              "ignoreDiacritics", "alwaysExpandOptionalParts",
              "synonymSearchEnabled"}) {
            QVERIFY(advanced_page->isAncestorOf(Page(dialog, name)));
        }
        for (const char* name :
             {"enableTrayIcon", "cbAutostart", "displayStyle",
              "enableScanPopup", "enableMainWindowHotkey"}) {
            QVERIFY(Page(dialog, name) == nullptr);
        }
    }

    void FrozenGridAndText() {
        PreferencesDialog dialog({}, [](const auto&) { return QString(); }, {});
        auto* tab_group = dialog.findChild<QGroupBox*>("preferencesTabGroup");
        QVERIFY(tab_group != nullptr);
        QCOMPARE(tab_group->title(), QString("Tabbed browsing"));
        auto* grid = qobject_cast<QGridLayout*>(tab_group->layout());
        VerifyCell(grid, Page(dialog, "newTabsOpenInBackground"), 0, 0);
        VerifyCell(grid, Page(dialog, "newTabsOpenAfterCurrentOne"), 1, 0);
        VerifyCell(grid, Page(dialog, "hideSingleTab"), 0, 1);
        VerifyCell(grid, Page(dialog, "mruTabOrder"), 1, 1);
        auto* background =
            dialog.findChild<QCheckBox*>("newTabsOpenInBackground");
        QVERIFY(background != nullptr);
        QCOMPARE(background->text(), QString("Open new tabs in background"));
        QCOMPARE(
            background->toolTip(),
            QString("Normally, opening a new tab switches to it immediately.\n"
                    "With this on however, new tabs will be opened without\n"
                    "switching to them."));
        auto* article_group =
            dialog.findChild<QGroupBox*>("preferencesArticlesGroup");
        QVERIFY(article_group != nullptr);
        auto* article_grid =
            qobject_cast<QGridLayout*>(article_group->layout());
        VerifyCell(article_grid, Page(dialog, "collapseBigArticles"), 0, 0);
        VerifyCell(article_grid, Page(dialog, "articleSizeLimit"), 0, 1);
        VerifyCell(article_grid, Page(dialog, "alwaysExpandOptionalParts"), 0,
                   4);
        VerifyCell(article_grid, Page(dialog, "limitInputPhraseLength"), 1, 0);
        VerifyCell(article_grid, Page(dialog, "inputPhraseLengthLimit"), 1, 1);
        VerifyCell(article_grid, Page(dialog, "ignoreDiacritics"), 1, 4);
        auto* ignore = dialog.findChild<QCheckBox*>("ignoreDiacritics");
        QVERIFY(ignore != nullptr);
        QCOMPARE(ignore->text(), QString("Ignore diacritics while searching"));
    }

    void CompleteCandidateAcrossPages_data() {
        QTest::addColumn<int>("result");
        QTest::newRow("cancel") << 0;
        QTest::newRow("failed-apply") << 1;
        QTest::newRow("successful-apply") << 2;
    }

    void CompleteCandidateAcrossPages() {
        QFETCH(int, result);
        ApplicationPreferences initial;
        initial.maximum_history_entries = 751;
        initial.interface_language = "retained-locale";
        initial.maximum_network_cache_megabytes = 25;
        ApplicationPreferences captured;
        int calls = 0;
        PreferencesDialog dialog(
            initial,
            [&](const auto& candidate) {
                ++calls;
                captured = candidate;
                return result == 1 ? QString("injected apply failure")
                                   : QString();
            },
            {});
        auto* tabs = dialog.findChild<QTabWidget*>("preferencesTabs");
        auto* background =
            dialog.findChild<QCheckBox*>("newTabsOpenInBackground");
        auto* history = dialog.findChild<QSpinBox*>("historyMaxSizeField");
        auto* cache = dialog.findChild<QSpinBox*>("maxNetworkCacheSize");
        auto* buttons =
            dialog.findChild<QDialogButtonBox*>("preferencesButtonBox");
        QVERIFY(tabs && background && history && cache && buttons);
        dialog.show();
        QTest::qWait(10);
        tabs->setCurrentIndex(0);
        background->setChecked(!initial.open_new_tabs_in_background);
        tabs->setCurrentIndex(1);
        cache->setValue(42);
        tabs->setCurrentIndex(2);
        history->setValue(345);
        if (result == 0) {
            buttons->button(QDialogButtonBox::Cancel)->click();
            QCOMPARE(calls, 0);
            QCOMPARE(dialog.result(), int(QDialog::Rejected));
            QVERIFY(!dialog.isVisible());
        } else {
            buttons->button(QDialogButtonBox::Ok)->click();
            QCOMPARE(calls, 1);
            auto expected = initial;
            expected.open_new_tabs_in_background =
                !initial.open_new_tabs_in_background;
            expected.maximum_history_entries = 345;
            expected.maximum_network_cache_megabytes = 42;
            expected.proxy_type = goldendict::core::ProxyType::kHttpConnect;
#if defined(Q_OS_LINUX)
            // Retain the pre-existing unsupported-locale fallback of the
            // selector.
            expected.interface_language.clear();
#endif
            QVERIFY(captured == expected);
            QCOMPARE(dialog.result(), result == 2 ? int(QDialog::Accepted)
                                                  : int(QDialog::Rejected));
            auto* error =
                dialog.findChild<QLabel*>("preferencesValidationError");
            QVERIFY(error != nullptr);
            QCOMPARE(error->isHidden(), result != 1);
            QCOMPARE(dialog.isVisible(), result == 1);
        }
    }

    void EmbeddedIconProvenance() {
        const QList<QPair<QString, QByteArray>> icons = {
            {":/icons/interface.png",
             "a1abcd2571d1c668ed0f57504ea0f8d6b7d858fea7468eb4513c4b555177fd1"
             "d"},
            {":/icons/network.png",
             "08b0a3564e3b3c2ee86a61f1ee9321fe54ff057ff3566b56a3a06c35ae8b933"
             "7"}};
        for (const auto& icon : icons) {
            QFile file(icon.first);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(QCryptographicHash::hash(file.readAll(),
                                              QCryptographicHash::Sha256)
                         .toHex(),
                     icon.second);
        }
    }

    void UsableGeometryAndOptionalCapture() {
        PreferencesDialog dialog({}, [](const auto&) { return QString(); }, {});
        dialog.show();
        QTest::qWait(100);
        QVERIFY(dialog.width() < 1200);
        QVERIFY(dialog.height() < 700);
        auto* tabs = dialog.findChild<QTabWidget*>("preferencesTabs");
        QVERIFY(tabs != nullptr);
        QJsonArray pages;
        const QString output =
            qEnvironmentVariable("GOLDENDICT_PREFERENCES_CAPTURE_DIR");
        if (!output.isEmpty()) {
            QVERIFY(QDir().mkpath(output));
            dialog.resize(670, 475);
        }
        for (int index = 0; index < tabs->count(); ++index) {
            tabs->setCurrentIndex(index);
            QTest::qWait(100);
            QWidget* page = tabs->widget(index);
            for (auto* control : page->findChildren<QWidget*>()) {
                if (!control->isVisible())
                    continue;
                const QRect bounds(control->mapTo(page, QPoint()),
                                   control->size());
                QVERIFY2(page->rect().contains(bounds),
                         qPrintable(control->objectName()));
            }
            if (!output.isEmpty()) {
                const QString filename = QString::number(index) + ".png";
                QVERIFY(dialog.grab().save(QDir(output).filePath(filename)));
                pages.append(QJsonObject{{"page", tabs->tabText(index)},
                                         {"file", filename}});
            }
        }
        auto* history = Page(dialog, "preferencesHistoryGroup");
        auto* favorites = Page(dialog, "favoritesBox");
        QVERIFY(history && favorites);
        QCOMPARE(history->y(), favorites->y());
        QVERIFY(history->geometry().right() < favorites->geometry().left());
        auto* proxy = Page(dialog, "useProxyServer");
        auto* cache = Page(dialog, "maxNetworkCacheSize");
        QVERIFY(proxy && cache);
        QVERIFY(proxy->geometry().bottom() < cache->geometry().top());
        if (!output.isEmpty()) {
            QJsonObject metadata{
                {"qt_version", qVersion()},
                {"platform", QApplication::platformName()},
                {"style", dialog.style()->objectName()},
                {"width", dialog.width()},
                {"height", dialog.height()},
                {"device_pixel_ratio", dialog.devicePixelRatioF()},
                {"font", dialog.font().toString()},
                {"pages", pages}};
            QFile file(QDir(output).filePath("metadata.json"));
            QVERIFY(file.open(QIODevice::WriteOnly));
            const auto bytes = QJsonDocument(metadata).toJson();
            QCOMPARE(file.write(bytes), qint64(bytes.size()));
        }
    }
};

}  // namespace

QTEST_MAIN(PreferencesDialogTest)
#include "preferences_dialog_test.moc"
