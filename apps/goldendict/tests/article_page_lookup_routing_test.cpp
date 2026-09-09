// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QScopeGuard>
#include <QWebEngineProfile>
#include <QWebEngineUrlScheme>
#include <QWebEngineView>
#include <QtTest>

#include <memory>

#include "article_content_origin.h"
#include "article_page.h"
#include "article_scheme_handler.h"
#include "goldendict/core/application.h"

namespace {

class ArticlePageLookupRoutingTest : public QObject {
    Q_OBJECT

   private slots:
    void DispatchesExactLookupTargets_data();
    void DispatchesExactLookupTargets();
};

void ArticlePageLookupRoutingTest::DispatchesExactLookupTargets_data() {
    QTest::addColumn<QByteArray>("encoded");
    QTest::addColumn<QByteArray>("target");
    QTest::newRow("U+0001") << QByteArray("a%01b")
                            << QByteArray(
                                   "a\x01"
                                   "b");
    QTest::newRow("DEL") << QByteArray("a%7Fb")
                         << QByteArray(
                                "a\x7f"
                                "b");
    QTest::newRow("literal-percent-hash-slash-unicode")
        << QByteArray("%2501%23part%2F%E4%BD%A0%E5%A5%BD")
        << QByteArray(u8"%01#part/你好");
}

void ArticlePageLookupRoutingTest::DispatchesExactLookupTargets() {
    QFETCH(QByteArray, encoded);
    QFETCH(QByteArray, target);
    auto facade = goldendict::core::CreateDesktopFacade({});
    const auto href = QByteArray("goldendict://lookup/") + encoded;
    const QUrl url = QUrl::fromEncoded(href, QUrl::StrictMode);
    QVERIFY(url.isValid());
    QCOMPARE(url.toEncoded(), href);
    const auto resolved =
        facade->ResolveArticleUrl(url.toEncoded().toStdString());
    QVERIFY(resolved.has_value());
    QCOMPARE(resolved->lookup_text, target.toStdString());

    QWebEngineView view;
    auto* page = new ArticlePage(&view);
    page->SetFacade(facade.get());
    ArticleSchemeHandler handler;
    handler.SetFacade(facade.get());
    page->profile()->installUrlSchemeHandler(QByteArrayLiteral("goldendict"),
                                             &handler);
    const auto remove_handler = qScopeGuard([page, &handler]() {
        page->profile()->removeUrlSchemeHandler(&handler);
    });
    view.setPage(page);
    QSignalSpy requested(page, &ArticlePage::LookupRequested);
    QSignalSpy audio(page, &ArticlePage::AudioResourceRequested);
    QSignalSpy external(page, &ArticlePage::ExternalUrlRequested);
    QSignalSpy loaded(page, &QWebEnginePage::loadFinished);
    view.resize(500, 300);
    view.show();
    // Fixed boxes permit real pointer activation in both native and offscreen
    // WebEngine runs. The second href deliberately bypasses producer rejection.
    view.setHtml(
        QString::fromLatin1(
            "<html><body style='margin:0'><a style='display:block;width:200px;"
            "height:60px' href='" +
            href +
            "'>target</a><a style='display:block;width:200px;height:60px' "
            "href='goldendict://lookup/' "
            "onclick='document.body.dataset.emptyClicked=1'>"
            "empty</a></body></html>"),
        goldendict::app::ArticleContentBaseUrl());
    QVERIFY(!loaded.isEmpty() || loaded.wait(10000));
    QVERIFY(loaded.last().at(0).toBool());
    QTRY_VERIFY(view.focusProxy() != nullptr);
    QTest::qWait(150);
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(40, 25));
    QTRY_COMPARE(requested.size(), 1);
    QCOMPARE(requested.front().at(0).toString().toUtf8(), target);
    QCOMPARE(requested.front().at(1).toString().toLatin1(), href);
    QCOMPARE(qvariant_cast<ArticleLinkDisposition>(requested.front().at(2)),
             ArticleLinkDisposition::kCurrentTab);
    const QUrl before = page->url();
    QTest::mouseClick(view.focusProxy(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(40, 85));
    auto empty_clicked = std::make_shared<bool>(false);
    QElapsedTimer timeout;
    timeout.start();
    while (!*empty_clicked && timeout.elapsed() < 5000) {
        page->runJavaScript(QStringLiteral("document.body.dataset.emptyClicked"),
                            [empty_clicked](const QVariant& value) {
                                *empty_clicked = value.toString() == "1";
                            });
        QTest::qWait(20);
    }
    QVERIFY(*empty_clicked);
    QTest::qWait(100);
    QCOMPARE(requested.size(), 1);
    QCOMPARE(page->url(), before);
    QVERIFY(audio.isEmpty());
    QVERIFY(external.isEmpty());
}

}  // namespace

int main(int argc, char* argv[]) {
    QWebEngineUrlScheme scheme(QByteArrayLiteral("goldendict"));
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(0);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                    QWebEngineUrlScheme::LocalScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
    QApplication application(argc, argv);
    ArticlePageLookupRoutingTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "article_page_lookup_routing_test.moc"
