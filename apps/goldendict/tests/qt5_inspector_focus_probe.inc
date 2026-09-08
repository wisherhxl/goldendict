// SPDX-License-Identifier: GPL-3.0-or-later
// External frozen-Qt-5 capture entry point; not part of the Qt 6 product.
#include <QContextMenuEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QSignalSpy>
#include <QStyle>
#include <QStyleFactory>
#include <QTest>
#include <QTimer>
#include <QWebSettings>
#include "articleinspector.hh"
#include "articleview.hh"
#include "articlewebview.hh"
#include "audioplayerfactory.hh"

static int RunInspectorFocusProbe(QApplication& app, const QString& output) {
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont("Segoe UI", 9));
    QWebSettings::globalSettings()->setAttribute(
        QWebSettings::DeveloperExtrasEnabled, true);
    Config::Class config;
    std::vector<sptr<Dictionary::Class>> dictionaries;
    Instances::Groups groups;
    ArticleMaker maker(dictionaries, groups, QString(), QString());
    const bool disallow = true, hide_header = false;
    ArticleNetworkAccessManager network(nullptr, dictionaries, maker, disallow,
                                        hide_header);
    AudioPlayerFactory audio(config.preferences);
    QAction search(nullptr);
    ArticleView article(nullptr, network, audio.player(), dictionaries, groups,
                        false, config, search);
    auto* web = article.findChild<ArticleWebView*>();
    if (!web || !QDir().mkpath(output))
        return 2;
    auto inspector = []() -> ArticleInspector* {
        for (auto* widget : QApplication::topLevelWidgets())
            if (auto* found = qobject_cast<ArticleInspector*>(widget))
                return found;
        return nullptr;
    };
    QAction* inspect = nullptr;
    for (auto* action : web->actions())
        if (action->shortcut() == QKeySequence(Qt::Key_F12))
            inspect = action;
    if (!inspect)
        return 3;
    QSignalSpy triggered(inspect, &QAction::triggered);
    QSignalSpy loaded(web, &QWebView::loadFinished);
    article.resize(800, 600);
    article.show();
    web->setHtml(QStringLiteral(
        "<!doctype html><html><head><title>Inspector focus "
        "fixture</title></head>"
        "<body style='margin:0'><h1 id='inspection-target' "
        "style='margin:0;padding:30px'>GoldenDict inspector fixture</h1>"
        "<p>Local deterministic article content.</p></body></html>"));
    if (loaded.isEmpty() && !loaded.wait(10000))
        return 4;
    if (!loaded.last().at(0).toBool())
        return 5;
    article.activateWindow();
    web->setFocus();
    QTest::qWait(250);
    QJsonObject result;
    result.insert("qt_version", qVersion());
    result.insert("platform", app.platformName());
    result.insert("style", article.style()->objectName());
    result.insert("font", article.font().toString());
    result.insert("device_pixel_ratio", article.devicePixelRatioF());
    result.insert("article_active_before_menu", article.isActiveWindow());
    bool menu_seen = false;
    QTimer driver;
    QObject::connect(&driver, &QTimer::timeout, &article, [&]() {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
            return;
        driver.stop();
        menu_seen = true;
        result.insert("popup_has_inspect", menu->actions().contains(inspect));
        QTest::keyClick(menu, Qt::Key_F12);
        QTest::qWait(250);
        result.insert("popup_f12_trigger_count", triggered.count());
        result.insert("popup_f12_inspector_created", inspector() != nullptr);
        result.insert("popup_f12_menu_still_visible", menu->isVisible());
        result.insert("popup_capture_saved",
                      menu->grab().save(QDir(output).filePath("popup.png")));
        QTest::keyClick(menu, Qt::Key_Escape);
    });
    driver.start(20);
    QTest::mouseClick(web, Qt::RightButton, Qt::NoModifier, QPoint(100, 50));
    QContextMenuEvent context(QContextMenuEvent::Mouse, QPoint(100, 50),
                              web->mapToGlobal(QPoint(100, 50)));
    QApplication::sendEvent(web, &context);
    QTest::qWait(500);
    driver.stop();
    result.insert("menu_seen", menu_seen);
    result.insert("popup_closed_after_escape",
                  !QApplication::activePopupWidget());
    result.insert("article_active_after_escape", article.isActiveWindow());
    result.insert("web_focused_after_escape", web->hasFocus());
    QTest::keyClick(web, Qt::Key_F12);
    QTest::qWait(1500);
    auto* first = inspector();
    result.insert("article_f12_trigger_count", triggered.count());
    result.insert("article_f12_inspector_visible", first && first->isVisible());
    result.insert("article_f12_inspector_active",
                  first && first->isActiveWindow());
    if (first) {
        first->resize(1000, 700);
        QTest::qWait(2000);
        result.insert(
            "inspector_capture_saved",
            first->grab().save(QDir(output).filePath("inspector.png")));
        article.activateWindow();
        web->setFocus();
        QTest::qWait(250);
        QTest::keyClick(web, Qt::Key_F12);
        QTest::qWait(500);
        result.insert("repeated_f12_same_inspector", inspector() == first);
        result.insert("repeated_f12_inspector_active", first->isActiveWindow());
        result.insert("repeated_f12_trigger_count", triggered.count());
        QTest::mouseClick(web, Qt::LeftButton, Qt::NoModifier, QPoint(100, 50));
        QTest::keyClick(web, Qt::Key_F12);
        QTest::qWait(500);
        result.insert("clicked_article_f12_inspector_active",
                      first->isActiveWindow());
        first->close();
        article.activateWindow();
        web->setFocus();
        QTest::qWait(250);
        QTest::mouseClick(web, Qt::LeftButton, Qt::NoModifier, QPoint(100, 50));
        QTest::keyClick(web, Qt::Key_F12);
        QTest::qWait(500);
        result.insert("reopened_same_inspector", inspector() == first);
        result.insert("reopened_visible", first->isVisible());
        result.insert("reopened_active", first->isActiveWindow());
        first->close();
    }
    QFile file(QDir(output).filePath("result.json"));
    if (!file.open(QIODevice::WriteOnly))
        return 6;
    const auto bytes = QJsonDocument(result).toJson();
    return file.write(bytes) == bytes.size() && menu_seen ? 0 : 7;
}
