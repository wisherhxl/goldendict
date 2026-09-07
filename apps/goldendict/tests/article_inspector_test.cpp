// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPointer>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QtTest>

#include "article_page.h"
#include "article_view.h"

namespace {

const QString kFixture = QStringLiteral(
    "<!doctype html><html><head><title>Inspector fixture</title></head>"
    "<body style='margin:0'><h1 id='inspection-target' "
    "style='margin:0;padding:30px'>GoldenDict inspector fixture</h1>"
    "<p>Local deterministic article content.</p></body></html>");

// DevTools uses shadow roots. Wait for observable frontend content, not just
// loadFinished, which precedes the connection to the inspected document.
const QString kFrontendText = QStringLiteral(R"JS(
    (() => {
        function text(node) {
            if (node.nodeType === Node.TEXT_NODE) return node.textContent;
            return [...node.childNodes].map(text).join(' ') +
                (node.shadowRoot ? text(node.shadowRoot) : '');
        }
        return text(document.body);
    })()
)JS");

QVariant Evaluate(QWebEnginePage* page, const QString& script) {
    struct Result {
        bool complete = false;
        QVariant value;
    };

    auto result = std::make_shared<Result>();
    page->runJavaScript(script, [result](const QVariant& value) {
        result->value = value;
        result->complete = true;
    });
    QElapsedTimer timeout;
    timeout.start();
    while (!result->complete && timeout.elapsed() < 5000)
        QTest::qWait(10);
    return result->value;
}

bool LoadFixture(ArticleView& view) {
    QSignalSpy loaded(&view, &ArticleView::loadFinished);
    view.resize(800, 600);
    view.show();
    view.setHtml(kFixture);
    return (!loaded.isEmpty() || loaded.wait(10000)) &&
           loaded.last().at(0).toBool();
}

bool WaitForFrontendText(QWebEnginePage* frontend, const QString& expected) {
    QElapsedTimer timeout;
    timeout.start();
    while (timeout.elapsed() < 30000) {
        // Evaluate processes the event loop. Do not put it inside QTRY's
        // repeatedly evaluated condition/timeout diagnostic expressions.
        if (Evaluate(frontend, kFrontendText).toString().contains(expected))
            return true;
        QTest::qWait(50);
    }
    return false;
}

QWidget* InspectorWindow(QWebEnginePage* inspected) {
    auto* frontend = inspected->devToolsPage();
    if (!frontend)
        return nullptr;
    auto* view = QWebEngineView::forPage(frontend);
    return view ? view->window() : nullptr;
}

bool EnterConsoleExpression(QWebEnginePage* frontend,
                            const QString& expression) {
    auto* view = QWebEngineView::forPage(frontend);
    if (!view || !view->focusProxy())
        return false;
    view->window()->activateWindow();
    view->window()->resize(1000, 700);
    view->setFocus();
    QTest::qWait(100);
    // The built-in Escape shortcut opens the Console drawer even when the
    // Console tab is in the narrow-window overflow menu.
    QTest::keyClick(view->focusProxy(), Qt::Key_Escape);
    QTest::qWait(200);
    const bool focused = Evaluate(frontend, QStringLiteral(R"JS(
        (() => {
            function focus(root) {
                for (const node of root.querySelectorAll('*')) {
                    if (node.getAttribute('contenteditable') === 'true' &&
                        node.getBoundingClientRect().width > 0) {
                        node.focus(); return true;
                    }
                    if (node.shadowRoot && focus(node.shadowRoot)) return true;
                }
                return false;
            }
            return focus(document);
        })()
    )JS"))
                             .toBool();
    if (!focused)
        return false;
    QTest::keyClicks(view->focusProxy(), expression);
    QTest::keyClick(view->focusProxy(), Qt::Key_Return);
    return true;
}

class RecordingPage final : public QWebEnginePage {
   public:
    explicit RecordingPage(QObject* parent) : QWebEnginePage(parent) {}

    int inspected_targets = 0;

    void triggerAction(WebAction action, bool checked = false) override {
        if (action == InspectElement)
            ++inspected_targets;
        QWebEnginePage::triggerAction(action, checked);
    }
};

class ArticleInspectorTest final : public QObject {
    Q_OBJECT

   private slots:

    void LazyKeyboardEntryAndReuse() {
        ArticleView view;
        auto* source = new RecordingPage(&view);
        view.setPage(source);
        QVERIFY(LoadFixture(view));
        QVERIFY(!source->devToolsPage());
        auto* action = view.findChild<QAction*>("inspectArticle");
        QVERIFY(action);
        QCOMPARE(action->text(), QString("Inspect"));
        QCOMPARE(action->shortcut(), QKeySequence(Qt::Key_F12));
        QCOMPARE(action->shortcutContext(), Qt::WindowShortcut);
        QVERIFY(view.AvailableContextActions({}).last() ==
                ArticleContextAction::kInspect);
        view.activateWindow();
        view.setFocus();
        QTest::qWait(100);
        QTest::keyClick(view.focusProxy(), Qt::Key_F12);
        QTRY_VERIFY(source->devToolsPage());
        QPointer<QWebEnginePage> frontend = source->devToolsPage();
        QPointer<QWidget> window = InspectorWindow(source);
        QVERIFY(window);
        QVERIFY(window->isWindow());
        QVERIFY(!window->isModal());
        QVERIFY(window->isVisible());
        QCOMPARE(source->inspected_targets, 0);
        QCOMPARE(frontend->inspectedPage(), source);
        QVERIFY(frontend->profile()->isOffTheRecord());
        QVERIFY(frontend->profile() != source->profile());
        QCOMPARE(frontend->profile()->httpCacheType(),
                 QWebEngineProfile::MemoryHttpCache);
        QCOMPARE(frontend->profile()->persistentCookiesPolicy(),
                 QWebEngineProfile::NoPersistentCookies);
        const bool javascript = source->settings()->testAttribute(
            QWebEngineSettings::JavascriptEnabled);
        window->close();
        QVERIFY(!window->isVisible());
        QVERIFY(frontend);
        action->trigger();
        QCOMPARE(source->devToolsPage(), frontend.data());
        QCOMPARE(InspectorWindow(source), window.data());
        QVERIFY(window->isVisible());
        QCOMPARE(source->settings()->testAttribute(
                     QWebEngineSettings::JavascriptEnabled),
                 javascript);
    }

    void IndependentViewsAndReplacement() {
        auto first = std::make_unique<ArticleView>();
        ArticleView second;
        QWebEnginePage old_source;
        QWebEnginePage new_source;
        first->setPage(&old_source);
        first->findChild<QAction*>("inspectArticle")->trigger();
        second.findChild<QAction*>("inspectArticle")->trigger();
        QPointer<QWebEnginePage> first_frontend = old_source.devToolsPage();
        QPointer<QWebEnginePage> second_frontend =
            second.page()->devToolsPage();
        QPointer<QWidget> first_window = InspectorWindow(&old_source);
        QVERIFY(first_frontend && second_frontend && first_window);
        QPointer<QWebEngineProfile> first_profile = first_frontend->profile();
        QVERIFY(first_frontend != second_frontend);
        QVERIFY(first_frontend->profile() != second_frontend->profile());
        first->setPage(&old_source);
        QCOMPARE(old_source.devToolsPage(), first_frontend.data());
        first->setPage(&new_source);
        QVERIFY(first_frontend.isNull());
        QVERIFY(first_window.isNull());
        QVERIFY(first_profile.isNull());
        QVERIFY(!old_source.devToolsPage());
        QVERIFY(!new_source.devToolsPage());
        QVERIFY(second_frontend);
        first->findChild<QAction*>("inspectArticle")->trigger();
        QPointer<QWebEnginePage> replacement = new_source.devToolsPage();
        QPointer<QWidget> replacement_window = InspectorWindow(&new_source);
        QVERIFY(replacement && replacement_window);
        first.reset();
        QVERIFY(replacement.isNull());
        QVERIFY(replacement_window.isNull());
        QVERIFY(!new_source.devToolsPage());
        QVERIFY(second_frontend);
    }

    void InspectedPageDestruction() {
        ArticleView view;
        auto source = std::make_unique<QWebEnginePage>();
        view.setPage(source.get());
        view.findChild<QAction*>("inspectArticle")->trigger();
        QPointer<QWidget> window = InspectorWindow(source.get());
        QVERIFY(window && window->isVisible());
        source.reset();
        QVERIFY(!window || !window->isVisible());
        auto* replacement = new QWebEnginePage(&view);
        view.setPage(replacement);
        view.findChild<QAction*>("inspectArticle")->trigger();
        QVERIFY(replacement->devToolsPage());
        QCOMPARE(replacement->devToolsPage()->inspectedPage(), replacement);
    }

    void F12TargetsOnlyTheVisibleTab() {
        QTabWidget tabs;
        auto* first = new ArticleView(&tabs);
        auto* second = new ArticleView(&tabs);
        tabs.addTab(first, "First");
        tabs.addTab(second, "Second");
        tabs.resize(800, 600);
        tabs.show();
        tabs.activateWindow();
        first->setFocus();
        QTest::qWait(100);
        QTest::keyClick(first->focusProxy(), Qt::Key_F12);
        QTRY_VERIFY(first->page()->devToolsPage());
        QVERIFY(!second->page()->devToolsPage());
        auto* first_window = InspectorWindow(first->page());
        tabs.setCurrentIndex(1);
        QVERIFY(first_window->isVisible());
        tabs.activateWindow();
        second->setFocus();
        QTest::qWait(100);
        QTest::keyClick(second->focusProxy(), Qt::Key_F12);
        QTRY_VERIFY(second->page()->devToolsPage());
        QCOMPARE(InspectorWindow(first->page()), first_window);
        QVERIFY(first_window != InspectorWindow(second->page()));
    }

    void ContextMenuTargetAndTailOrder_data() {
        QTest::addColumn<int>("change");
        QTest::newRow("current-target") << 0;
        QTest::newRow("replaced-page") << 1;
        QTest::newRow("navigated-document") << 2;
    }

    void ContextMenuTargetAndTailOrder() {
        QFETCH(int, change);
        ArticleView view;
        auto* source = new RecordingPage(&view);
        view.setPage(source);
        QVERIFY(LoadFixture(view));
        view.SetDictionaryContextEntries(
            {{"fixture-id", "Fixture dictionary", 0}}, true, 1);
        QSignalSpy activated(view.findChild<QAction*>("inspectArticle"),
                             &QAction::triggered);
        bool menu_seen = false;
        bool order_matches = false;
        QTimer menu_driver;
        connect(&menu_driver, &QTimer::timeout, &view, [&]() {
            auto* menu =
                qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu)
                return;
            menu_seen = true;
            const auto actions = menu->actions();
            auto* inspect = actions.isEmpty() ? nullptr : actions.last();
            order_matches =
                actions.size() >= 4 && inspect &&
                inspect->text() == "Inspect" &&
                inspect->shortcut() == QKeySequence(Qt::Key_F12) &&
                actions[actions.size() - 2]->isSeparator() &&
                actions[actions.size() - 3]->text() == "........." &&
                actions[actions.size() - 4]->text() == "Fixture dictionary";
            menu_driver.stop();
            if (inspect) {
                if (change == 1) {
                    view.setPage(new QWebEnginePage(&view));
                } else if (change == 2) {
                    view.setHtml("<p>Replacement document</p>");
                }
                menu->setActiveAction(inspect);
                QTimer::singleShot(1000, menu, &QMenu::close);
                QTest::keyClick(menu, Qt::Key_Return);
            } else {
                menu->close();
            }
        });
        menu_driver.start(20);
        auto* web = view.findChild<QWebEngineView*>("articleWebContent");
        QVERIFY(web && web->focusProxy());
        view.activateWindow();
        view.setFocus();
        QTest::qWait(100);
        QTest::mouseClick(web->focusProxy(), Qt::RightButton, Qt::NoModifier,
                          QPoint(100, 50));
        QTRY_VERIFY_WITH_TIMEOUT(menu_seen, 10000);
        QVERIFY(order_matches);
        if (change == 1 || change == 2) {
            QCOMPARE(source->inspected_targets, 0);
            QVERIFY(!source->devToolsPage());
            QVERIFY(!view.page()->devToolsPage());
            return;
        }
        QCOMPARE(activated.count(), 1);
        QTRY_VERIFY(source->devToolsPage());
        QCOMPARE(source->inspected_targets, 1);
        QVERIFY(InspectorWindow(source)->isVisible());
        auto* frontend = source->devToolsPage();
        QVERIFY(WaitForFrontendText(frontend, "GoldenDict inspector fixture"));
        QVERIFY(EnterConsoleExpression(frontend, "'selected:' + $0.id"));
        QVERIFY(WaitForFrontendText(frontend, "selected:inspection-target"));
    }

    void FrontendAndOptionalCapture() {
        ArticleView view;
        view.setPage(new ArticlePage(&view));
        QVERIFY(LoadFixture(view));
        view.findChild<QAction*>("inspectArticle")->trigger();
        auto* frontend = view.page()->devToolsPage();
        QVERIFY(frontend);
        QTRY_COMPARE_WITH_TIMEOUT(frontend->url().scheme(), QString("devtools"),
                                  15000);
        QTRY_COMPARE_WITH_TIMEOUT(
            Evaluate(frontend, "document.readyState").toString(),
            QString("complete"), 15000);
        QVERIFY(Evaluate(frontend, "document.body.childElementCount").toInt() >
                0);
        QVERIFY(WaitForFrontendText(frontend, "GoldenDict inspector fixture"));
        QCOMPARE(
            Evaluate(view.page(),
                     "document.getElementById('inspection-target').textContent")
                .toString(),
            QString("GoldenDict inspector fixture"));
        auto* window = InspectorWindow(view.page());
        QVERIFY(window && window->isVisible());
        const auto output =
            qEnvironmentVariable("GOLDENDICT_INSPECTOR_CAPTURE_DIR");
        if (!output.isEmpty()) {
            QVERIFY(QDir().mkpath(output));
            window->resize(1000, 700);
            QVERIFY(WaitForFrontendText(frontend, "element.style"));
            QTest::qWait(2000);
            QVERIFY(
                window->grab().save(QDir(output).filePath("inspector.png")));
            QVERIFY(view.grab().save(QDir(output).filePath("article.png")));
            const QJsonObject metadata{
                {"qt_version", qVersion()},
                {"platform", QApplication::platformName()},
                {"style", window->style()->objectName()},
                {"font", window->font().toString()},
                {"device_pixel_ratio", window->devicePixelRatioF()},
                {"width", window->width()},
                {"height", window->height()},
                {"frontend_url", frontend->url().toString()},
                {"frontend_title", frontend->title()}};
            QFile file(QDir(output).filePath("metadata.json"));
            QVERIFY(file.open(QIODevice::WriteOnly));
            const auto bytes = QJsonDocument(metadata).toJson();
            QCOMPARE(file.write(bytes), qint64(bytes.size()));
        }
        QVERIFY(EnterConsoleExpression(
            frontend,
            "'console-check:' + "
            "document.getElementById('inspection-target').textContent"));
        QVERIFY(WaitForFrontendText(
            frontend, "console-check:GoldenDict inspector fixture"));
        if (!output.isEmpty())
            QVERIFY(window->grab().save(QDir(output).filePath("console.png")));
    }
};

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    if (!qEnvironmentVariableIsEmpty("GOLDENDICT_INSPECTOR_CAPTURE_DIR"))
        app.setFont(QFont("Segoe UI", 9));
    ArticleInspectorTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "article_inspector_test.moc"
