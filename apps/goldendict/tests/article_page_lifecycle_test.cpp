// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QWebEngineProfile>
#include <QWebEngineUrlScheme>
#include <QtTest>
#include "legacy_configuration_location.h"
#include "webengine_storage_paths.h"

#include "article_page.h"
#include "article_view.h"
#include "goldendict/core/application.h"
#include "main_window.h"
#include "source_directories_dialog.h"

namespace core = goldendict::core;

class ArticlePageLifecycleTest : public QObject {
    Q_OBJECT

   private:
    QTemporaryDir fixture_;
    core::CoreConfiguration configuration_;
    std::shared_ptr<core::DesktopFacade> facade_;
    std::unique_ptr<MainWindow> window_;

    core::ArticleTabId TabId() const {
        return facade_->GetArticleTabsState().active_tab_id;
    }

    ArticleView* View() const { return window_->ArticleViewForTab(TabId()); }

    bool LoadDocument(ArticleView* view) {
        QSignalSpy finished(view, &ArticleView::loadFinished);
        view->page()->load(
            QUrl::fromLocalFile(fixture_.filePath("article.html")));
        return finished.wait(10000) && finished.last().at(0).toBool() &&
               !view->page()->isLoading();
    }

    std::shared_ptr<core::DesktopFacade> Replacement() {
        auto replacement = std::shared_ptr<core::DesktopFacade>(
            core::CreateDesktopFacade(configuration_));
        if (!replacement->RestoreArticleTabSession(
                facade_->ExportArticleTabSession()))
            return {};
        return replacement;
    }

    bool PublishReplacement() {
        auto replacement = Replacement();
        if (!replacement)
            return false;
        auto candidate = window_->PrepareFacadeCandidate(
            replacement, configuration_.preferences,
            configuration_.dictionary_groups);
        auto begun = window_->BeginFacadeCandidateMaintenance(candidate);
        if (begun.outcome != WidgetsCommitOutcome::kMaintainedAbortable)
            return false;
        auto published =
            window_->PublishMaintainedFacadeCommit(std::move(begun.maintained));
        const auto result =
            window_->FinishPublishedFacadeCommit(std::move(published));
        facade_ = std::move(replacement);
        return result == WidgetsCommitOutcome::kPublished;
    }

    bool InFlight(core::ArticleTabId id) const {
        const auto found = window_->article_reload_states_.find(id);
        return found != window_->article_reload_states_.end() &&
               found->second.in_flight_generation.has_value();
    }

    QVariant Evaluate(ArticleView* view, const QString& script) {
        auto result = std::make_shared<std::optional<QVariant>>();
        view->page()->runJavaScript(
            script, [result](const QVariant& value) { *result = value; });
        QElapsedTimer timeout;
        timeout.start();
        while (!result->has_value() && timeout.elapsed() < 10000)
            QTest::qWait(10);
        return result->value_or(QVariant{});
    }

   private slots:

    void init() {
        QVERIFY(fixture_.isValid());
        configuration_ = {};
        configuration_.index_directory =
            fixture_.filePath("indexes").toStdString();
        configuration_.preferences.zoom_factor = 1.25;
        configuration_.preferences.double_click_translates = false;
        configuration_.preferences.select_word_by_single_click = true;
        facade_ = std::shared_ptr<core::DesktopFacade>(
            core::CreateDesktopFacade(configuration_));
        window_ = std::make_unique<MainWindow>(fixture_.path());
        window_->SetPreferences(configuration_.preferences);
        window_->SetFacade(facade_.get());
        window_->resize(800, 600);
        window_->show();
        QFile document(fixture_.filePath("article.html"));
        QVERIFY(document.open(QIODevice::WriteOnly));
        QVERIFY(
            document.write(
                "<!doctype html><html><body><p>needle needle</p>"
                "<div style='height:4000px'>scrollable</div></body></html>") >
            0);
        document.close();
        QVERIFY(View());
        QVERIFY(LoadDocument(View()));
    }

    void cleanup() {
        window_.reset();
        facade_.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void refreshUsesRealBindings_data() {
        QTest::addColumn<int>("replacements");
        QTest::addColumn<bool>("overlap");
        QTest::newRow("normal-sequential") << 0 << false;
        QTest::newRow("normal-overlap") << 0 << true;
        QTest::newRow("prepared-sequential") << 1 << false;
        QTest::newRow("prepared-overlap") << 1 << true;
        QTest::newRow("twice-prepared-overlap") << 2 << true;
    }

    void refreshUsesRealBindings() {
        QFETCH(int, replacements);
        QFETCH(bool, overlap);
        for (int i = 0; i < replacements; ++i) {
            QVERIFY(PublishReplacement());
            QVERIFY(LoadDocument(View()));
        }
        auto* view = View();
        const auto id = TabId();
        int starts = 0;
        bool start_marked = false;
        bool one_navigation_transition = true;
        auto navigation_generation =
            window_->article_navigation_generations_[id];
        const auto connection =
            connect(view, &ArticleView::loadStarted, this, [&]() {
                ++starts;
                const auto found = window_->article_reload_states_.find(id);
                if (starts == 1) {
                    start_marked =
                        found != window_->article_reload_states_.end() &&
                        found->second.load_started;
                }
                const auto current =
                    window_->article_navigation_generations_[id];
                one_navigation_transition &=
                    current == navigation_generation + 1;
                navigation_generation = current;
                if (starts == 1 && overlap) {
                    window_->reload_action_->trigger();
                    view->page()->triggerAction(QWebEnginePage::Stop);
                }
            });
        const auto disconnect_observer =
            qScopeGuard([&]() { disconnect(connection); });
        window_->reload_action_->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(starts >= 1, 10000);
        // A real loadStarted was observed. Failure here identifies A1, not a
        // timeout.
        QVERIFY2(start_marked,
                 "Published view loadStarted did not mark its in-flight reload "
                 "started");
        if (overlap)
            QTRY_COMPARE_WITH_TIMEOUT(starts, 2, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!view->page()->isLoading() && !InFlight(id),
                                 10000);
        const int completed_starts = starts;
        window_->reload_action_->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(starts, completed_starts + 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!view->page()->isLoading() && !InFlight(id),
                                 10000);
        QVERIFY(one_navigation_transition);
        QCOMPARE(window_->article_reload_states_.at(id).generation,
                 std::uint64_t(overlap ? 3 : 2));
    }

    void initialPreferencesSurvivePageInstallation() {
        QCOMPARE(View()->zoomFactor(), 1.25);
        QVERIFY(PublishReplacement());
        QCOMPARE(View()->zoomFactor(), 1.25);
    }

    void searchAndF3Bindings_data() {
        QTest::addColumn<bool>("prepared");
        QTest::newRow("normal") << false;
        QTest::newRow("prepared") << true;
    }

    void searchAndF3Bindings() {
        QFETCH(bool, prepared);
        if (prepared) {
            QVERIFY(PublishReplacement());
            QVERIFY(LoadDocument(View()));
        }
        auto* view = View();
        const auto id = TabId();
        window_->search_in_page_action_->trigger();
        window_->article_search_->setText("needle");
        QTest::keyClick(window_->article_search_, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(window_->article_search_status_->text(),
                                  QString("1 of 2"), 10000);
        const auto search_generation =
            window_->article_search_presentations_.at(id).generation;
        int source_dialogs = 0;
        window_->source_dialog_executor_ = [&](SourceDirectoriesDialog&) {
            ++source_dialogs;
            return QDialog::Rejected;
        };
        const auto reset_executor =
            qScopeGuard([&]() { window_->source_dialog_executor_ = {}; });
        // F3 belongs to Dictionaries. It must not become a find-next binding.
        QCOMPARE(window_->dictionaries_action_->shortcut(),
                 QKeySequence(Qt::Key_F3));
        QTest::keyClick(window_->article_search_, Qt::Key_F3);
        QCOMPARE(source_dialogs, 1);
        QCOMPARE(window_->article_search_presentations_.at(id).generation,
                 search_generation);
        QSignalSpy finished(view, &ArticleView::loadFinished);
        QSignalSpy search_finished(view->page(),
                                   &QWebEnginePage::findTextFinished);
        const auto before_reload =
            window_->article_search_presentations_.at(id).generation;
        window_->reload_action_->trigger();
        QVERIFY(finished.wait(10000));
        QVERIFY(!InFlight(id));
        QCOMPARE(window_->article_search_presentations_.at(id).generation,
                 before_reload + 1);
        QTRY_VERIFY_WITH_TIMEOUT(search_finished.count() > 0, 10000);
        // WebEngine may retain the current match across reload; both positions
        // are valid, but the restored search must still report both matches.
        QVERIFY(window_->article_search_status_->text() == "1 of 2" ||
                window_->article_search_status_->text() == "2 of 2");
        emit view->loadFinished(true);
        QCOMPARE(window_->article_search_presentations_.at(id).generation,
                 before_reload + 1);
    }

    void scrollAndClickBindings_data() { searchAndF3Bindings_data(); }

    void scrollAndClickBindings() {
        QFETCH(bool, prepared);
        if (prepared) {
            QVERIFY(PublishReplacement());
            QVERIFY(LoadDocument(View()));
        }
        auto* view = View();
        QSignalSpy scrolled(view->page(),
                            &QWebEnginePage::scrollPositionChanged);
        const auto epoch = window_->presentation_mutation_epoch_;
        QVERIFY(Evaluate(view, "window.scrollTo(0, 700); true").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(scrolled.count() > 0, 10000);
        QVERIFY(window_->presentation_mutation_epoch_ > epoch);
        QVERIFY(view->page()->scrollPosition().y() > 0);
        QVERIFY(Evaluate(view,
                         "window.scrollTo(0, 0); "
                         "window.getSelection().removeAllRanges(); true")
                    .toBool());
        QTRY_COMPARE_WITH_TIMEOUT(view->page()->scrollPosition().y(), 0.0,
                                  10000);
        const auto point =
            Evaluate(view,
                     "(()=>{const "
                     "r=document.querySelector('p').getBoundingClientRect(); "
                     "return [r.x+12,r.y+r.height/2]})()")
                .toList();
        QCOMPARE(point.size(), 2);
        auto completed = std::make_shared<bool>(false);
        view->TriggerWordQueryForTest(
            QPointF(point[0].toDouble(), point[1].toDouble()), false,
            [completed]() { *completed = true; });
        QTRY_VERIFY_WITH_TIMEOUT(*completed, 10000);
        QCOMPARE(Evaluate(view, "window.getSelection().toString()").toString(),
                 QString("needle"));
        QSignalSpy lookups(view, &ArticleView::SelectionLookupRequested);
        *completed = false;
        view->TriggerWordQueryForTest(
            QPointF(point[0].toDouble(), point[1].toDouble()), true,
            [completed]() { *completed = true; });
        QVERIFY(*completed);
        QCOMPARE(lookups.count(), 0);
        emit view->SelectionToInputRequested("bound selection");
        QCOMPARE(window_->query_->text(), QString("bound selection"));
    }

    void candidateEventsStraddlingPublication() {
        auto replacement = Replacement();
        auto candidate = window_->PrepareFacadeCandidate(
            replacement, configuration_.preferences,
            configuration_.dictionary_groups);
        QVERIFY(candidate);
        auto* staged_tabs = window_->findChild<QTabWidget*>(
            "widgetsFacadeCandidateArticleTabs");
        QVERIFY(staged_tabs);
        auto* hidden = qobject_cast<ArticleView*>(staged_tabs->widget(0));
        QVERIFY(hidden);
        auto* shared_profile = QWebEngineProfile::defaultProfile();
        const auto storage_path = shared_profile->persistentStoragePath();
        QVERIFY(storage_path.endsWith("/webengine/article"));
        QCOMPARE(View()->page()->profile(), shared_profile);
        QCOMPARE(hidden->page()->profile(), shared_profile);
        QVERIFY(shared_profile->isOffTheRecord());
        const auto id = TabId();
        const auto generation = window_->article_navigation_generations_.at(id);
        emit hidden->loadStarted();
        QCOMPARE(window_->article_navigation_generations_.at(id), generation);
        auto begun = window_->BeginFacadeCandidateMaintenance(candidate);
        QCOMPARE(begun.outcome, WidgetsCommitOutcome::kMaintainedAbortable);
        auto published =
            window_->PublishMaintainedFacadeCommit(std::move(begun.maintained));
        QCOMPARE(window_->FinishPublishedFacadeCommit(std::move(published)),
                 WidgetsCommitOutcome::kPublished);
        facade_ = std::move(replacement);
        QCOMPARE(View(), hidden);
        QCOMPARE(View()->page()->profile(), shared_profile);
        QCOMPARE(shared_profile->persistentStoragePath(), storage_path);
        emit hidden->loadFinished(true);
        QVERIFY(!InFlight(id));
        QVERIFY(LoadDocument(hidden));
        QSignalSpy finished(hidden, &ArticleView::loadFinished);
        window_->reload_action_->trigger();
        QVERIFY(finished.wait(10000));
        QVERIFY(!InFlight(id));
    }

    void hiddenAndAbandonedCandidateDoNotStealActiveEvents() {
        QVERIFY(PublishReplacement());
        QVERIFY(LoadDocument(View()));
        auto* active = View();
        const auto id = TabId();
        auto candidate = window_->PrepareFacadeCandidate(
            Replacement(), configuration_.preferences,
            configuration_.dictionary_groups);
        QVERIFY(candidate);
        ArticleView* hidden = nullptr;
        for (auto* view : window_->findChildren<ArticleView*>()) {
            if (view != active &&
                view->property("articleTabId").toULongLong() == id)
                hidden = view;
        }
        QVERIFY(hidden);
        const auto generation = window_->article_navigation_generations_.at(id);
        const auto reload_count = window_->article_reload_states_.size();
        emit hidden->loadStarted();
        emit hidden->loadFinished(true);
        QCOMPARE(window_->article_navigation_generations_.at(id), generation);
        QCOMPARE(window_->article_reload_states_.size(), reload_count);
        candidate = {};
        QSignalSpy started(active, &ArticleView::loadStarted);
        window_->reload_action_->trigger();
        QVERIFY(started.wait(10000));
        QVERIFY2(window_->article_reload_states_.at(id).load_started,
                 "Preparing/abandoning a successor suppressed the "
                 "still-published page");
        QTRY_VERIFY_WITH_TIMEOUT(!InFlight(id), 10000);
    }

    void backgroundTabCompletesRefresh() {
        QVERIFY(PublishReplacement());
        QVERIFY(LoadDocument(View()));
        const auto id = TabId();
        auto* background = View();
        window_->reload_action_->trigger();
        window_->CreateEmptyArticleTab(true);
        QVERIFY(View() != background);
        QSignalSpy finished(background, &ArticleView::loadFinished);
        QVERIFY(finished.wait(10000));
        QVERIFY2(!InFlight(id),
                 "Valid background tab retained a completed refresh");
    }

    void replacementRetiresOldInFlightReload() {
        const auto id = TabId();
        QPointer<ArticleView> retired = View();
        window_->reload_action_->trigger();
        QVERIFY(InFlight(id));
        // No event-loop turn: publish while the old reload is still pending.
        QVERIFY(PublishReplacement());
        QVERIFY(View() != retired);
        auto* current = View();
        QSignalSpy started(current, &ArticleView::loadStarted);
        QSignalSpy finished(current, &ArticleView::loadFinished);
        window_->reload_action_->trigger();
        QVERIFY2(window_->article_reload_states_.at(id).in_flight_generation ==
                     window_->article_reload_states_.at(id).generation,
                 "Successor refresh inherited the retired page's in-flight "
                 "generation");
        QVERIFY(started.wait(10000));
        QVERIFY(finished.wait(10000));
        QVERIFY(!InFlight(id));
    }

    void oldViewCompletionCannotEraseSuccessorReload() {
        const auto id = TabId();
        auto* old_view = View();
        // Keep the retired QObject alive only until the next event-loop turn.
        window_->article_tabs_->removeTab(
            window_->article_tabs_->indexOf(old_view));
        window_->SyncArticleTabs();
        auto* current = View();
        QVERIFY(current != old_view);
        QVERIFY(LoadDocument(current));
        window_->reload_action_->trigger();
        QVERIFY(InFlight(id));
        const auto generation =
            window_->article_reload_states_.at(id).generation;
        emit old_view->loadStarted();
        emit old_view->loadFinished(true);
        QVERIFY2(InFlight(id),
                 "Retired view callback erased the successor's reload");
        QCOMPARE(window_->article_reload_states_.at(id).generation, generation);
        delete old_view;
        QTRY_VERIFY_WITH_TIMEOUT(!InFlight(id), 10000);
    }
};

int main(int argc, char** argv) {
    QTemporaryDir profile;
    if (!profile.isValid())
        return 2;
    for (const auto* name :
         {"HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "APPDATA",
          "LOCALAPPDATA", "GOLDENDICT_TEST_CONFIG_ROOT", "TEMP", "TMP"}) {
        const auto directory = profile.filePath(QString::fromLatin1(name));
        if (!QDir().mkpath(directory))
            return 2;
        qputenv(name, directory.toUtf8());
    }
    QWebEngineUrlScheme scheme(QByteArrayLiteral("goldendict"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(0);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                    QWebEngineUrlScheme::LocalScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
    QTemporaryDir webengine_storage;
    if (!webengine_storage.isValid())
        return 2;
    QApplication application(argc, argv);
    goldendict::app::InitializeWebEngineStorage(
        {}, webengine_storage.filePath("webengine"));
    ArticlePageLifecycleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "article_page_lifecycle_test.moc"
