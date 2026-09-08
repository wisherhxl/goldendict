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
#include <QScreen>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QtTest>

#include "article_inspector.h"
#include "article_page.h"
#include "article_view.h"
#include "goldendict/core/application.h"

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

    void GeometryCloseSharingAndExit() {
        auto state = std::make_shared<ArticleInspectorState>();
        QWidget seed;
        seed.setGeometry(80, 90, 600, 400);
        state->SetInitialGeometry(seed.saveGeometry());
        QSignalSpy checkpoints(state.get(),
                               &ArticleInspectorState::GeometryCaptured);
        QWebEnginePage page1, page2;
        auto first = std::make_unique<ArticleInspector>(&page1, state);
        first->Inspect(false);
        QTest::qWait(100);
        QCOMPARE(first->size(), seed.size());
        first->setGeometry(110, 120, 640, 420);
        QTest::qWait(50);
        const QByteArray adjusted_first = first->saveGeometry();
        first->close();
        QCOMPARE(checkpoints.size(), 1);
        QCOMPARE(state->geometry(), adjusted_first);

        auto second = std::make_unique<ArticleInspector>(&page2, state);
        second->Inspect(false);
        QTest::qWait(100);
        QCOMPARE(second->geometry(), first->geometry());
        second->setGeometry(140, 150, 660, 440);
        QTest::qWait(50);
        const QByteArray adjusted_second = second->saveGeometry();
        // Reopening copies the retained first window, but must not become
        // the latest user adjustment. Closing the older window saves it now.
        first->Inspect(false);
        QTest::qWait(100);
        first->close();
        QCOMPARE(state->geometry(), first->saveGeometry());
        second.reset();
        first.reset();
        state->CheckpointForExit();
        QCOMPARE(state->geometry(), adjusted_second);
    }

    void GeometryRestartAndFallback() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const std::string path = directory.filePath("core.conf").toStdString();
        QWebEnginePage page;
        QByteArray saved;
        {
            auto state = std::make_shared<ArticleInspectorState>();
            ArticleInspector window(&page, state);
            window.Inspect(false);
            QTest::qWait(100);
            window.setGeometry(100, 110, 610, 410);
            QTest::qWait(50);
            saved = window.saveGeometry();
            window.close();
            goldendict::core::CoreConfiguration config;
            config.inspector_geometry = state->geometry().toStdString();
            goldendict::core::SaveConfiguration(path, config);
        }
        auto state = std::make_shared<ArticleInspectorState>();
        state->SetInitialGeometry(QByteArray::fromStdString(
            goldendict::core::LoadConfiguration(path).inspector_geometry));
        {
            ArticleInspector restarted(&page, state);
            restarted.Inspect(false);
            QTest::qWait(100);
            QCOMPARE(restarted.size(), QSize(610, 410));
            state->CheckpointForExit();
            QCOMPARE(state->geometry(), saved);
        }
        for (const QByteArray geometry :
             {QByteArray(), QByteArray("invalid")}) {
            auto invalid = std::make_shared<ArticleInspectorState>();
            invalid->SetInitialGeometry(geometry);
            ArticleInspector fallback(&page, invalid);
            const QSize initial = fallback.size();
            QCOMPARE(initial, QSize(450, 300));
            fallback.Inspect(false);
            QTest::qWait(50);
            QCOMPARE(fallback.size(), initial);
        }
        QWidget offscreen;
        offscreen.setGeometry(100000, 100000, 600, 400);
        auto recovery = std::make_shared<ArticleInspectorState>();
        recovery->SetInitialGeometry(offscreen.saveGeometry());
        ArticleInspector recovered(&page, recovery);
        recovered.Inspect(false);
        QTest::qWait(100);
        QVERIFY(recovered.screen()->availableGeometry().intersects(
            recovered.frameGeometry()));
    }

    void MaximizedGeometryAndNoAdjustmentExit() {
        QWebEnginePage page;
        QByteArray geometry;
        {
            auto state = std::make_shared<ArticleInspectorState>();
            ArticleInspector inspector(&page, state);
            inspector.Inspect(false);
            QTest::qWait(100);
            inspector.setGeometry(100, 100, 600, 400);
            inspector.showMaximized();
            QTest::qWait(100);
            QVERIFY(inspector.isMaximized());
            inspector.close();
            geometry = state->geometry();
            state->CheckpointForExit();
            QCOMPARE(state->geometry(), geometry);
        }
        auto state = std::make_shared<ArticleInspectorState>();
        state->SetInitialGeometry(geometry);
        ArticleInspector restored(&page, state);
        restored.Inspect(false);
        QTest::qWait(100);
        QVERIFY(restored.isMaximized());
        state->CheckpointForExit();
        QCOMPARE(state->geometry(), geometry);
    }

    void NativeLegacyGeometryImport() {
        const QString geometry_path =
            qEnvironmentVariable("GOLDENDICT_LEGACY_INSPECTOR_GEOMETRY");
        if (geometry_path.isEmpty())
            QSKIP(
                "Native Qt 5 geometry evidence is supplied by the paired "
                "capture run");
        QFile fixture(geometry_path);
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        const auto bytes = fixture.readAll();
        QVERIFY(!bytes.isEmpty());
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile legacy(directory.filePath("config"));
        QVERIFY(legacy.open(QIODevice::WriteOnly));
        const QByteArray xml = "<config><inspectorGeometry>" +
                               bytes.toBase64() +
                               "</inspectorGeometry></config>";
        QCOMPARE(legacy.write(xml), xml.size());
        legacy.close();
        const auto configuration = goldendict::core::LoadOrMigrateConfiguration(
            directory.filePath("core.conf").toStdString(),
            legacy.fileName().toStdString(),
            directory.filePath("indexes").toStdString());
        QCOMPARE(configuration.inspector_geometry, bytes.toStdString());
        auto state = std::make_shared<ArticleInspectorState>();
        state->SetInitialGeometry(bytes);
        ArticleView article;
        QVERIFY(LoadFixture(article));
        ArticleInspector inspector(article.page(), state);
        inspector.Inspect(false);
        QVERIFY(WaitForFrontendText(
            article.page()->devToolsPage(),
            QStringLiteral("GoldenDict inspector fixture")));
        QTest::qWait(1000);
        QCOMPARE(inspector.geometry(), QRect(100, 110, 610, 410));
        const auto output =
            qEnvironmentVariable("GOLDENDICT_INSPECTOR_CAPTURE_DIR");
        if (!output.isEmpty()) {
            QVERIFY(QDir().mkpath(output));
            QVERIFY(inspector.grab().save(
                QDir(output).filePath("legacy-geometry-restored.png")));
        }
        inspector.close();
        QCOMPARE(state->geometry(), inspector.saveGeometry());
    }

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

    void PopupCancellationAndKeyboardFocus() {
        ArticleView view;
        auto* source = new RecordingPage(&view);
        view.setPage(source);
        QVERIFY(LoadFixture(view));
        auto* web = view.findChild<QWebEngineView*>("articleWebContent");
        auto* inspect = view.findChild<QAction*>("inspectArticle");
        QVERIFY(web && web->focusProxy() && inspect);
        QSignalSpy triggered(inspect, &QAction::triggered);
        const bool native = QApplication::platformName() == "windows";
        const auto output =
            qEnvironmentVariable("GOLDENDICT_INSPECTOR_CAPTURE_DIR");
        QJsonObject result;
        bool menu_seen = false;
        bool menu_has_inspect = false;
        bool popup_still_visible = false;
        bool popup_created_inspector = false;
        int popup_trigger_count = -1;
        bool capture_saved = output.isEmpty();
        QTimer driver;
        connect(&driver, &QTimer::timeout, &view, [&]() {
            auto* menu =
                qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu)
                return;
            driver.stop();
            menu_seen = true;
            menu_has_inspect = menu->actions().contains(inspect);
            QTest::keyClick(menu, Qt::Key_F12);
            QTest::qWait(250);
            popup_trigger_count = triggered.count();
            popup_created_inspector = source->devToolsPage() != nullptr;
            popup_still_visible = menu->isVisible();
            if (!output.isEmpty())
                capture_saved =
                    QDir().mkpath(output) &&
                    menu->grab().save(QDir(output).filePath("popup.png"));
            QTest::keyClick(menu, Qt::Key_Escape);
        });
        view.activateWindow();
        view.setFocus();
        QTest::qWait(250);
        if (native)
            QTRY_VERIFY(view.isActiveWindow());
        driver.start(20);
        QTest::mouseClick(web->focusProxy(), Qt::RightButton, Qt::NoModifier,
                          QPoint(100, 50));
        QTRY_VERIFY_WITH_TIMEOUT(menu_seen, 10000);
        QVERIFY(menu_has_inspect);
        QCOMPARE(popup_trigger_count, 0);
        QVERIFY(!popup_created_inspector);
        QVERIFY(popup_still_visible);
        QVERIFY(capture_saved);
        QTRY_VERIFY(!QApplication::activePopupWidget());
        // Do not manually reactivate or refocus the article after Escape:
        // the next real shortcut must work with the restored popup focus.
        if (native)
            QTRY_VERIFY(view.isActiveWindow());
        QTest::keyClick(web->focusProxy(), Qt::Key_F12);
        QTRY_COMPARE(triggered.count(), 1);
        QTRY_VERIFY(source->devToolsPage());
        QPointer<QWidget> window = InspectorWindow(source);
        QVERIFY(window && window->isVisible());
        QCOMPARE(source->inspected_targets, 0);
        if (native)
            QTRY_VERIFY(window->isActiveWindow());
        QVERIFY(WaitForFrontendText(source->devToolsPage(),
                                    "GoldenDict inspector fixture"));
        result.insert("popup_f12_trigger_count", popup_trigger_count);
        result.insert("popup_f12_inspector_created", popup_created_inspector);
        result.insert("popup_f12_menu_still_visible", popup_still_visible);
        result.insert("article_f12_inspector_active", window->isActiveWindow());
        // Diagnostic only: legacy activation without an intervening pointer
        // event is tracked separately, not accepted as equivalent here. The
        // assertion below proves dispatch, not the unresolved focus outcome.
        view.activateWindow();
        view.setFocus();
        QTest::qWait(250);
        QTest::keyClick(web->focusProxy(), Qt::Key_F12);
        QTRY_COMPARE(triggered.count(), 2);
        QTest::qWait(500);
        result.insert("repeated_f12_same_inspector",
                      InspectorWindow(source) == window);
        result.insert("repeated_f12_inspector_active",
                      window->isActiveWindow());
        // A pointer return to the article is the explicit frozen direct-
        // invocation path; it must raise/reuse, not create another inspector.
        for (const bool close_first : {false, true}) {
            if (close_first)
                window->close();
            view.activateWindow();
            view.setFocus();
            QTest::qWait(250);
            QTest::mouseClick(web->focusProxy(), Qt::LeftButton, Qt::NoModifier,
                              QPoint(100, 50));
            QTest::keyClick(web->focusProxy(), Qt::Key_F12);
            QTRY_VERIFY(window->isVisible());
            QCOMPARE(InspectorWindow(source), window.data());
            if (native)
                QTRY_VERIFY(window->isActiveWindow());
        }
        QCOMPARE(triggered.count(), 4);
        QCOMPARE(source->inspected_targets, 0);
        if (!output.isEmpty()) {
            window->resize(1000, 700);
            QVERIFY(
                WaitForFrontendText(source->devToolsPage(), "element.style"));
            QTest::qWait(500);
            QVERIFY(
                window->grab().save(QDir(output).filePath("inspector.png")));
            result.insert("qt_version", qVersion());
            result.insert("platform", QApplication::platformName());
            result.insert("style", window->style()->objectName());
            result.insert("font", window->font().toString());
            result.insert("device_pixel_ratio", window->devicePixelRatioF());
            result.insert("clicked_article_f12_inspector_active",
                          window->isActiveWindow());
            result.insert("reopened_same_inspector",
                          InspectorWindow(source) == window);
            result.insert("reopened_visible", window->isVisible());
            QFile file(QDir(output).filePath("result.json"));
            QVERIFY(file.open(QIODevice::WriteOnly));
            const auto bytes = QJsonDocument(result).toJson();
            QCOMPARE(file.write(bytes), qint64(bytes.size()));
        }
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
        QVERIFY(WaitForFrontendText(frontend, "GoldenDict inspector fixture"));
        QVERIFY(Evaluate(frontend, "document.body.childElementCount").toInt() >
                0);
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
