// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QMenu>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebEngineScript>
#include <QWebEngineView>
#include <QtTest>

#include "article_view.h"
#include "goldendict/core/application.h"

namespace {

QVariant Evaluate(QWebEnginePage* page, const QString& script,
                  quint32 world = QWebEngineScript::MainWorld) {
    struct Result {
        bool done = false;
        QVariant value;
    };

    auto result = std::make_shared<Result>();
    page->runJavaScript(script, world, [result](const QVariant& value) {
        result->value = value;
        result->done = true;
    });
    QElapsedTimer timer;
    timer.start();
    while (!result->done && timer.elapsed() < 5000)
        QTest::qWait(10);
    return result->value;
}

QString Fixture() {
    goldendict::core::LookupResponse response;
    for (const auto& text : {"Alpha", "Additional alpha", "Beta"}) {
        goldendict::core::DictionaryEntry entry;
        entry.dictionary.id = std::string(text) == "Beta" ? "second" : "first";
        entry.dictionary.name =
            entry.dictionary.id == "first" ? "First" : "Second";
        entry.article.plain_text = text;
        response.entries.push_back(std::move(entry));
    }
    auto facade = goldendict::core::CreateDesktopFacade({});
    return QString::fromStdString(
        *facade->ComposeLookupPage(response).sanitized_html);
}

bool Load(ArticleView& view, const QString& html = Fixture()) {
    QSignalSpy loaded(&view, &ArticleView::loadFinished);
    view.setHtml(html);
    return (!loaded.isEmpty() || loaded.wait(10000)) &&
           loaded.last()[0].toBool();
}

QString Selected(ArticleView& view) {
    return Evaluate(view.page(), "window.getSelection().toString()")
        .toString()
        .simplified();
}

bool WaitSelection(ArticleView& view, const QString& expected) {
    QElapsedTimer timer;
    timer.start();
    do {
        if (Selected(view) == expected)
            return true;
        QTest::qWait(20);
    } while (timer.elapsed() < 5000);
    return false;
}

QPoint EntryPoint(ArticleView& view, int index) {
    const auto point =
        Evaluate(view.page(),
                 QStringLiteral(
                     "(() => { const r=document.querySelectorAll('body > "
                     "section')[%1]"
                     ".getBoundingClientRect(); return [r.x+12,r.y+12]; })()")
                     .arg(index))
            .toList();
    return point.size() == 2 ? QPoint(point[0].toInt(), point[1].toInt())
                             : QPoint();
}

void Shortcut(ArticleView& view) {
    auto* web = view.findChild<QWebEngineView*>("articleWebContent");
    QTest::keyClick(web->focusProxy(), Qt::Key_A,
                    Qt::ControlModifier | Qt::ShiftModifier);
}

class ArticleSelectionTest final : public QObject {
    Q_OBJECT
   private slots:

    void CorpusSelection_data() {
        QTest::addColumn<QString>("word");
        QTest::newRow("synthetic") << QString();
        if (!qEnvironmentVariableIsEmpty("GOLDENDICT_SELECTION_CORPUS")) {
            QTest::newRow("real-apple") << QStringLiteral("apple");
            QTest::newRow("real-book") << QStringLiteral("book");
        }
    }

    void CorpusSelection() {
        QFETCH(QString, word);
        QString html = Fixture();
        QTemporaryDir indexes;
        QVERIFY(indexes.isValid());
        if (!word.isEmpty()) {
            goldendict::core::CoreConfiguration config;
            config.dictionary_paths = {
                qEnvironmentVariable("GOLDENDICT_SELECTION_CORPUS")
                    .toUtf8()
                    .toStdString()};
            config.index_directory = indexes.path().toUtf8().toStdString();
            auto facade = goldendict::core::CreateDesktopFacade(config);
            goldendict::core::LookupQuery query;
            query.text = word.toStdString();
            const auto response = facade->GetDictionaryService().Lookup(query);
            QVERIFY(!response.entries.empty());
            QVERIFY(response.errors.empty());
            html = QString::fromStdString(
                *facade->ComposeLookupPage(response).sanitized_html);
        }
        ArticleView view;
        view.resize(800, 600);
        view.show();
        QVERIFY(Load(view, html));
        view.activateWindow();
        view.setFocus();
        QTest::qWait(150);
        Shortcut(view);
        QTRY_VERIFY(view.page()->hasSelection());
        const auto selected = Selected(view);
        QVERIFY(!selected.isEmpty());
        // Inspect actual DOM range boundaries, without storing corpus text.
        QVERIFY(Evaluate(view.page(), QStringLiteral(R"JS(
            (() => {
              const s=window.getSelection();
              const first=document.querySelector('body > section');
              return s.anchorNode === first && s.anchorOffset === 0 &&
                s.focusNode.getAttribute('data-gd-dictionary-id') ===
                  first.getAttribute('data-gd-dictionary-id') &&
                s.focusOffset === s.focusNode.childNodes.length;
            })()
        )JS"))
                    .toBool());
    }

    void ShortcutPointerNavigationAndReplacement() {
        ArticleView view;
        view.resize(800, 600);
        view.show();
        QVERIFY(Load(view));
        view.activateWindow();
        view.setFocus();
        QTest::qWait(150);
        auto* action = view.findChild<QAction*>("selectCurrentArticle");
        QVERIFY(action);
        QCOMPARE(action->shortcut(), QKeySequence("Ctrl+Shift+A"));
        QSignalSpy triggered(action, &QAction::triggered);
        Shortcut(view);
        QTRY_COMPARE(triggered.size(), 1);
        QVERIFY(WaitSelection(view, "First Alpha First Additional alpha"));
        // CSP remains closed to content scripts; state exists only in the
        // trusted isolated world, and real Core output supplies the grouping.
        QCOMPARE(Evaluate(view.page(), "typeof globalThis.gdArticleSelection")
                     .toString(),
                 "undefined");
        auto* web = view.findChild<QWebEngineView*>("articleWebContent");
        QTest::mouseClick(web->focusProxy(), Qt::LeftButton, Qt::NoModifier,
                          EntryPoint(view, 2));
        QTest::qWait(100);
        Shortcut(view);
        QVERIFY(WaitSelection(view, "Second Beta"));
        view.NavigateToResult(1);
        Shortcut(view);
        QVERIFY(WaitSelection(view, "First Alpha First Additional alpha"));
        view.NavigateToResult(-1);
        view.NavigateToResult(99);
        Shortcut(view);
        QVERIFY(WaitSelection(view, "First Alpha First Additional alpha"));
        view.setPage(new QWebEnginePage(&view));
        QVERIFY(Load(view));
        view.setFocus();
        Shortcut(view);
        QVERIFY(WaitSelection(view, "First Alpha First Additional alpha"));
        QVERIFY(Load(view, "<p>No result</p>"));
        view.setFocus();
        Shortcut(view);
        QVERIFY(WaitSelection(view, ""));
        Evaluate(view.page(),
                 "const r=document.createRange();"
                 "r.selectNodeContents(document.querySelector('p'));"
                 "window.getSelection().addRange(r)");
        Shortcut(view);
        QVERIFY(WaitSelection(view, "No result"));
        QVERIFY(Load(view));
        view.setFocus();
        Shortcut(view);
        QVERIFY(WaitSelection(view, "First Alpha First Additional alpha"));
    }

    void ContextMenu_data() {
        QTest::addColumn<int>("change");
        QTest::newRow("activate") << 0;
        QTest::newRow("cancel") << 1;
        QTest::newRow("replace-document") << 2;
        QTest::newRow("replace-page") << 3;
        QTest::newRow("existing-selection") << 4;
    }

    void ContextMenu() {
        QFETCH(int, change);
        ArticleView view;
        view.resize(800, 600);
        view.show();
        QVERIFY(Load(view));
        view.activateWindow();
        view.setFocus();
        QTest::qWait(150);
        if (change == 4) {
            view.NavigateToResult(2);
            Shortcut(view);
            QVERIFY(WaitSelection(view, "Second Beta"));
        }
        const QPoint target = EntryPoint(view, 2);
        bool seen = false, correct = false, captured = true;
        QTimer driver;
        connect(&driver, &QTimer::timeout, &view, [&]() {
            auto* menu =
                qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu)
                return;
            driver.stop();
            seen = true;
            auto* action = view.findChild<QAction*>("selectCurrentArticle");
            const auto actions = menu->actions();
            correct = change == 4
                          ? !actions.contains(action)
                          : actions.size() >= 4 && actions[0] == action &&
                                actions[1]->text() == "Select All" &&
                                actions.last()->text() == "Inspect";
            const QString output =
                qEnvironmentVariable("GOLDENDICT_SELECTION_CAPTURE_DIR");
            if (!output.isEmpty() && change == 0)
                captured =
                    QDir().mkpath(output) &&
                    menu->grab().save(QDir(output).filePath("popup.png"));
            if (change == 1 || change == 4) {
                menu->close();
                return;
            }
            if (change == 2)
                view.setHtml("<p>Replacement</p>");
            if (change == 3)
                view.setPage(new QWebEnginePage(&view));
            menu->setActiveAction(action);
            QTimer::singleShot(1000, menu, &QMenu::close);
            QTest::keyClick(menu, Qt::Key_Return);
        });
        driver.start(20);
        auto* web = view.findChild<QWebEngineView*>("articleWebContent");
        QTest::mouseClick(web->focusProxy(), Qt::RightButton, Qt::NoModifier,
                          target);
        QTRY_VERIFY_WITH_TIMEOUT(seen, 10000);
        QVERIFY(correct);
        QVERIFY(captured);
        if (change == 0 || change == 4) {
            QVERIFY(WaitSelection(view, "Second Beta"));
            const QString output =
                qEnvironmentVariable("GOLDENDICT_SELECTION_CAPTURE_DIR");
            if (!output.isEmpty() && change == 0)
                QVERIFY(
                    view.grab().save(QDir(output).filePath("selection.png")));
        } else {
            QTest::qWait(250);
            QVERIFY(WaitSelection(view, ""));
        }
        if (change == 1) {
            Shortcut(view);
            QVERIFY(WaitSelection(view, "Second Beta"));
        }
    }

    void HiddenTabShortcutIsolation() {
        QTabWidget tabs;
        ArticleView first, second;
        tabs.addTab(&first, "First");
        tabs.addTab(&second, "Second");
        tabs.resize(800, 600);
        tabs.show();
        QVERIFY(Load(first));
        QVERIFY(Load(second));
        tabs.activateWindow();
        first.setFocus();
        QTest::qWait(150);
        Shortcut(first);
        QVERIFY(WaitSelection(first, "First Alpha First Additional alpha"));
        QVERIFY(WaitSelection(second, ""));
        tabs.setCurrentWidget(&second);
        second.setFocus();
        QTest::qWait(100);
        second.NavigateToResult(2);
        Shortcut(second);
        QVERIFY(WaitSelection(second, "Second Beta"));
        QVERIFY(WaitSelection(first, "First Alpha First Additional alpha"));
    }
};
}  // namespace

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    if (!qEnvironmentVariableIsEmpty("GOLDENDICT_SELECTION_CAPTURE_DIR"))
        app.setFont(QFont("Segoe UI", 9));
    ArticleSelectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "article_selection_test.moc"
