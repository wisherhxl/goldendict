// SPDX-License-Identifier: GPL-3.0-or-later
// External frozen-Qt-5 probe; never compiled into the Qt 6 product.
#include <QContextMenuEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QTest>
#include <QTimer>
#include <QWebFrame>
#include "articleview.hh"
#include "articlewebview.hh"
#include "audioplayerfactory.hh"

static int RunArticleSelectionProbe(QApplication& app, const QString& output) {
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont("Segoe UI", 9));
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
    auto request = maker.makeEmptyPage();
    auto& bytes = request->getFullData();
    QString html = QString::fromUtf8(bytes.data(), bytes.size());
    // Preserve the actual maker's gdSelectArticle/gdMakeArticleActive code.
    // Only deterministic dictionary payload and initial article state are
    // supplied by this fixture, using frozen container/event conventions.
    html.replace(
        "</body>",
        QStringLiteral(
            "<script>var gdCurrentArticle='gdfrom-first';"
            "var gdArticleContents='first second';"
            "var articleview={onJsActiveArticleChanged:function(){}};</script>"
            "<div id='gdfrom-first' class='gdarticle gdactivearticle' "
            "onclick=\"gdMakeArticleActive('first')\" "
            "oncontextmenu=\"gdMakeArticleActive('first')\">"
            "<h2>First</h2><p>Alpha</p><p>Additional alpha</p></div>"
            "<div id='gdfrom-second' class='gdarticle' "
            "onclick=\"gdMakeArticleActive('second')\" "
            "oncontextmenu=\"gdMakeArticleActive('second')\">"
            "<h2>Second</h2><p>Beta</p></div></body>"));
    QSignalSpy loaded(web, &QWebView::loadFinished);
    article.resize(800, 600);
    article.show();
    web->setHtml(html);
    if (loaded.isEmpty() && !loaded.wait(10000))
        return 3;
    if (!loaded.last()[0].toBool())
        return 4;
    article.activateWindow();
    web->setFocus();
    QTest::qWait(200);
    QJsonObject result;
    auto selected = [&]() {
        return web->selectedText().simplified();
    };
    auto shortcut = [&]() {
        QTest::keyClick(web, Qt::Key_A,
                        Qt::ControlModifier | Qt::ShiftModifier);
        QTest::qWait(100);
    };
    shortcut();
    result.insert("default_selection", selected());
    web->page()->mainFrame()->evaluateJavaScript(
        "window.getSelection().removeAllRanges()");
    const auto point = web->page()
                           ->mainFrame()
                           ->evaluateJavaScript(
                               "var "
                               "r=document.getElementById('gdfrom-second')."
                               "getBoundingClientRect(); [r.left+12,r.top+12]")
                           .toList();
    const QPoint target(point[0].toInt(), point[1].toInt());
    QTest::mouseClick(web, Qt::LeftButton, Qt::NoModifier, target);
    shortcut();
    result.insert("click_selection", selected());
    article.jumpToDictionary("first", true);
    shortcut();
    result.insert("navigation_selection", selected());
    web->page()->mainFrame()->evaluateJavaScript(
        "window.getSelection().removeAllRanges()");
    bool seen = false, captured = false;
    QTimer driver;
    QObject::connect(&driver, &QTimer::timeout, &article, [&]() {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
            return;
        driver.stop();
        seen = true;
        captured = menu->grab().save(QDir(output).filePath("popup.png"));
        QAction* select = nullptr;
        for (auto* action : menu->actions())
            if (action->text() == "Select Current Article")
                select = action;
        result.insert("menu_action", select != nullptr);
        if (select) {
            result.insert("shortcut", select->shortcut().toString());
            menu->setActiveAction(select);
            QTest::keyClick(menu, Qt::Key_Return);
        } else
            menu->close();
    });
    driver.start(20);
    // Qt 5 QTest's widget injection needs the native context event separately.
    QTest::mouseClick(web, Qt::RightButton, Qt::NoModifier, target);
    QContextMenuEvent context(QContextMenuEvent::Mouse, target,
                              web->mapToGlobal(target));
    QApplication::sendEvent(web, &context);
    QTest::qWait(100);
    result.insert("menu_selection", selected());
    result.insert("qt_version", qVersion());
    result.insert("platform", app.platformName());
    captured =
        article.grab().save(QDir(output).filePath("selection.png")) && captured;
    const bool passed = seen && captured && result["menu_action"].toBool() &&
                        result["default_selection"].toString() ==
                            "First Alpha Additional alpha" &&
                        result["click_selection"].toString() == "Second Beta" &&
                        result["navigation_selection"].toString() ==
                            "First Alpha Additional alpha" &&
                        result["menu_selection"].toString() == "Second Beta";
    result.insert("passed", passed);
    QFile file(QDir(output).filePath("result.json"));
    if (!file.open(QIODevice::WriteOnly))
        return 5;
    file.write(QJsonDocument(result).toJson());
    return passed ? 0 : 6;
}
