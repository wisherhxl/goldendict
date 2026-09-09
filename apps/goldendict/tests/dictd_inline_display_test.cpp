// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QScreen>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineUrlScheme>
#include <QWebEngineView>
#include <QtTest>

#include "article_content_origin.h"
#include "article_page.h"
#include "article_scheme_handler.h"
#include "article_view.h"
#include "audio_playback_service.h"
#include "goldendict/core/application.h"
#include "main_window.h"

namespace {
const QList<QByteArray> kSupplementBodies = {"{\n    }",
                                             QByteArray(u8"{\u00a0}"),
                                             "{.}",
                                             "{..}",
                                             "{../word}",
                                             "{/word}",
                                             "{...}",
                                             "{%2e}",
                                             "{&#46;}",
                                             "{\t . }",
                                             "{ ..\v }",
                                             "{.\n    }",
                                             QByteArray(u8"{你好%/#}")};

QList<QByteArray> ControlBodies() {
    QList<QByteArray> bodies;
    for (int c = 1; c <= 32; ++c) {
        const QByteArray body =
            "a" + QByteArray(1, char(c == 32 ? 127 : c)) + "b";
        bodies << body << "{" + body + "}";
    }
    bodies << "ab" << "{ab}" << "a b" << "{a b}" << "{\t\v\f }";
    return bodies;
}

QByteArray Read(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

bool Write(const QString& path, const QByteArray& data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QString Base64Integer(quint64 value) {
    const QString alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    QString result;
    do {
        result.prepend(alphabet.at(value % 64));
        value /= 64;
    } while (value);
    return result;
}

QVariant Evaluate(QWebEnginePage* page, const QString& script) {
    auto result = std::make_shared<std::optional<QVariant>>();
    page->runJavaScript(script,
                        [result](const QVariant& value) { *result = value; });
    QElapsedTimer timer;
    timer.start();
    while (!result->has_value() && timer.elapsed() < 10000)
        QTest::qWait(10);
    return result->value_or(QVariant());
}

QString WithoutActions(QString html) {
    html.replace(QRegularExpression("<a[^>]*>"), "<a>");
    html.replace(
        QRegularExpression("<span class=\"dictd_control\">([^<]*)</span>"),
        "\\1");
    return html;
}

struct PlaybackLease {
    QByteArray bytes;
    QString type;
    int starts = 0;
    int ticks = 0;
    bool active = false;
};

QJsonArray LinkStyles(QWebEnginePage* page) {
    return QJsonDocument::fromJson(
               Evaluate(page,
                        "JSON.stringify(Array.from(document.querySelectorAll('."
                        "dictd_article "
                        "a'),a=>({href:a.getAttribute('href'),color:"
                        "getComputedStyle(a).color,line:getComputedStyle(a)."
                        "textDecorationLine,thickness:getComputedStyle(a)."
                        "textDecorationThickness})))")
                   .toString()
                   .toUtf8())
        .array();
}
}  // namespace

class DictdInlineDisplayTest : public QObject {
    Q_OBJECT
   private slots:
    void initTestCase();
    void PreservesFrozenDisplay();
    void KeepsInertApplicationState();
    void PreservesSupplementalReferences();
    void PreservesControlGlyphs();

   private:
    QTemporaryDir directory_;
    QJsonArray rows_;
    QJsonArray contexts_;
    std::unique_ptr<goldendict::core::DesktopFacade> facade_;
    QString capture_ = qEnvironmentVariable("GOLDENDICT_DICTD_CAPTURE_DIR");
    QString Article(int index);
    void Load(MainWindow& window, int index);
};

void DictdInlineDisplayTest::initTestCase() {
    QVERIFY(directory_.isValid());
    rows_ =
        QJsonDocument::fromJson(Read(QStringLiteral(GOLDENDICT_DICTD_ORACLE)))
            .array();
    QCOMPARE(rows_.size(), 48);
    contexts_ = QJsonDocument::fromJson(
                    Read(QStringLiteral(GOLDENDICT_DICTD_CONTROL_CONTEXTS)))
                    .array();
    QCOMPARE(contexts_.size(), 7);
    QByteArray index, data;
    for (int i = 0; i < rows_.size(); ++i) {
        const QByteArray body = rows_[i].toObject()["body"].toString().toUtf8();
        index +=
            QString("case%1\t%2\t%3\n")
                .arg(i, 2, 10, QChar('0'))
                .arg(Base64Integer(data.size()), Base64Integer(body.size()))
                .toUtf8();
        data += body;
    }
    for (int j = 0; j < kSupplementBodies.size(); ++j) {
        const auto& body = kSupplementBodies[j];
        index +=
            QString("case%1\t%2\t%3\n")
                .arg(48 + j)
                .arg(Base64Integer(data.size()), Base64Integer(body.size()))
                .toUtf8();
        data += body;
    }
    const auto control_bodies = ControlBodies();
    for (int j = 0; j < control_bodies.size(); ++j) {
        const auto& body = control_bodies[j];
        index +=
            QString("case%1\t%2\t%3\n")
                .arg(200 + j)
                .arg(Base64Integer(data.size()), Base64Integer(body.size()))
                .toUtf8();
        data += body;
    }
    for (int j = 0; j < contexts_.size(); ++j) {
        const auto body = contexts_[j].toObject()["body"].toString().toUtf8();
        index +=
            QString("case%1\t%2\t%3\n")
                .arg(300 + j)
                .arg(Base64Integer(data.size()), Base64Integer(body.size()))
                .toUtf8();
        data += body;
    }
    QVERIFY(Write(directory_.filePath("fixture.index"), index));
    QVERIFY(Write(directory_.filePath("fixture.dict"), data));
    QVERIFY(QDir(directory_.path()).mkdir("audio"));
    QVERIFY(Write(directory_.filePath("audio/clip.wav"), "RIFFfixtureWAVE"));
    goldendict::core::CoreConfiguration configuration;
    configuration.dictionary_paths = {directory_.path().toStdString()};
    configuration.index_directory =
        directory_.filePath("indexes").toStdString();
    configuration.sound_directories = {
        {directory_.filePath("audio").toStdString(), "Audio fixture"}};
    facade_ = goldendict::core::CreateDesktopFacade(configuration);
    QVERIFY(facade_ != nullptr);
    if (!capture_.isEmpty())
        QVERIFY(QDir().mkpath(capture_));
}

void DictdInlineDisplayTest::PreservesSupplementalReferences() {
    QWebEngineView view;
    view.settings()->setFontFamily(QWebEngineSettings::StandardFont, "Arial");
    view.settings()->setFontFamily(QWebEngineSettings::FixedFont,
                                   "Courier New");
    view.settings()->setFontSize(QWebEngineSettings::DefaultFontSize, 16);
    view.settings()->setFontSize(QWebEngineSettings::DefaultFixedFontSize, 13);
    view.settings()->setFontSize(QWebEngineSettings::MinimumFontSize, 0);
    view.settings()->setFontSize(QWebEngineSettings::MinimumLogicalFontSize, 0);
    view.resize(800, 600);
    view.show();
    const QString css =
        QString::fromUtf8(Read(QStringLiteral(GOLDENDICT_DICTD_CSS)));
    const QString measurement = QString::fromUtf8(
        Read(qEnvironmentVariable("GOLDENDICT_DICTD_MEASUREMENT")));
    if (!capture_.isEmpty())
        QVERIFY(!measurement.isEmpty());
    QJsonArray observations;
    for (int i = 48; i < 48 + kSupplementBodies.size(); ++i) {
        const auto html = Article(i);
        QVERIFY(!html.isEmpty());
        QSignalSpy loaded(view.page(), &QWebEnginePage::loadFinished);
        view.setHtml(
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>" + css +
                "</style></head><body>" + html + "</body></html>",
            goldendict::app::ArticleContentBaseUrl());
        QVERIFY(!loaded.isEmpty() || loaded.wait(10000));
        QVERIFY(loaded.last()[0].toBool());
        QTest::qWait(50);
        const auto anchors =
            QJsonDocument::fromJson(
                Evaluate(
                    view.page(),
                    "JSON.stringify(Array.from(document.querySelectorAll('."
                    "dictd_article "
                    "a'),a=>({href:a.getAttribute('href'),text:a.textContent,"
                    "color:getComputedStyle(a).color,decoration:"
                    "getComputedStyle(a).textDecorationLine})))")
                    .toString()
                    .toUtf8())
                .array();
        QCOMPARE(anchors.size(), i == 48 || i == 59 ? 2 : 1);
        for (const auto& value : anchors) {
            const auto a = value.toObject();
            QCOMPARE(a["color"].toString(), QString("rgb(0, 0, 255)"));
            QCOMPARE(a["decoration"].toString(), QString("underline"));
            if (i < 52 || (i >= 57 && i <= 59))
                QVERIFY(a["href"].isNull());
            else {
                const QStringList targets = {"..%2Fword", "%2Fword", "...",
                                             "%252e", "%26amp%3B%2346%3B"};
                const auto target = i == 60
                                        ? QString("%E4%BD%A0%E5%A5%BD%25%2F%23")
                                        : targets[i - 52];
                QCOMPARE(a["href"].toString(), "goldendict://lookup/" + target);
            }
        }
        QJsonObject object{
            {"index", i},
            {"body", QString::fromUtf8(kSupplementBodies[i - 48])},
            {"html", html},
            {"anchors", anchors}};
        if (!measurement.isEmpty())
            object["measurement"] =
                QJsonDocument::fromJson(
                    Evaluate(view.page(), measurement).toString().toUtf8())
                    .object();
        observations.append(object);
        if (!capture_.isEmpty())
            QVERIFY(view.grab().save(
                QDir(capture_).filePath(QString("supplement-%1.png").arg(i))));
    }
    if (!capture_.isEmpty())
        QVERIFY(Write(QDir(capture_).filePath("supplemental-references.json"),
                      QJsonDocument(observations).toJson()));
}

void DictdInlineDisplayTest::PreservesControlGlyphs() {
    QWebEngineView view;
    view.resize(800, 600);
    view.settings()->setFontFamily(QWebEngineSettings::StandardFont, "Arial");
    view.settings()->setFontFamily(QWebEngineSettings::FixedFont,
                                   "Courier New");
    view.settings()->setFontSize(QWebEngineSettings::DefaultFontSize, 16);
    view.settings()->setFontSize(QWebEngineSettings::DefaultFixedFontSize, 13);
    view.settings()->setFontSize(QWebEngineSettings::MinimumFontSize, 0);
    view.settings()->setFontSize(QWebEngineSettings::MinimumLogicalFontSize, 0);
    view.show();
    const QString css =
        QString::fromUtf8(Read(QStringLiteral(GOLDENDICT_DICTD_CSS)));
    const QString measurement = QString::fromUtf8(
        Read(qEnvironmentVariable("GOLDENDICT_DICTD_MEASUREMENT")));
    if (!capture_.isEmpty())
        QVERIFY(!measurement.isEmpty());
    QJsonArray observations, context_observations, glyphs;
    auto bodies = ControlBodies();
    for (const auto& context : contexts_)
        bodies.append(context.toObject()["body"].toString().toUtf8());
    for (int i = 0; i < bodies.size(); ++i) {
        const auto html = Article(i < 69 ? 200 + i : 300 + i - 69);
        QVERIFY(!html.isEmpty());
        QSignalSpy loaded(view.page(), &QWebEnginePage::loadFinished);
        view.setHtml(
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>" + css +
                "</style></head><body>" + html + "</body></html>",
            goldendict::app::ArticleContentBaseUrl());
        QVERIFY(!loaded.isEmpty() || loaded.wait(10000));
        QVERIFY(loaded.last()[0].toBool());
        QTest::qWait(30);
        const auto geometry =
            QJsonDocument::fromJson(
                Evaluate(
                    view.page(),
                    "JSON.stringify((()=>{const "
                    "root=document.querySelector('.dictd_article'),w=document."
                    "createTreeWalker(root,NodeFilter.SHOW_TEXT),letters=[],"
                    "controls=[],spaces=[];for(let "
                    "n=w.nextNode();n;n=w.nextNode())for(let "
                    "i=0;i<n.length;i++){const "
                    "r=document.createRange();r.setStart(n,i);r.setEnd(n,i+1);"
                    "const "
                    "b=r.getBoundingClientRect(),v={x:b.x,y:b.y,width:b.width,"
                    "height:b.height};if(/[ab]/"
                    ".test(n.data[i]))letters.push(v);if(n.data[i]===' "
                    "'||n.data[i]==='\\t')spaces.push(v);if(n.parentElement."
                    "matches('.dictd_control'))controls.push(v)}return "
                    "{letters,controls,spaces,text:root.textContent,height:"
                    "root.getBoundingClientRect().height,anchors:Array.from("
                    "root.querySelectorAll('a'),a=>({href:a.getAttribute('href'"
                    "),width:a.getBoundingClientRect().width}))}})())")
                    .toString()
                    .toUtf8())
                .object();
        for (const auto& r : geometry["controls"].toArray())
            QCOMPARE(r.toObject()["width"].toDouble(), 0.0);
        if (i < 64) {
            const int c = i / 2 == 31 ? 127 : i / 2 + 1;
            const QString text =
                c == 10 || c == 13 ? "ab" : "a" + QString(QChar(c)) + "b";
            QCOMPARE(geometry["text"].toString(), text);
            if (i % 2) {
                const QString target = c == 13             ? "ab"
                                       : c >= 9 && c <= 12 ? "a b"
                                                           : text;
                const auto anchors = geometry["anchors"].toArray();
                QCOMPARE(anchors.size(), c == 10 ? 2 : 1);
                for (const auto& a : anchors)
                    QCOMPARE(a.toObject()["href"].toString(),
                             "goldendict://lookup/" +
                                 QString::fromLatin1(
                                     QUrl::toPercentEncoding(target)));
            }
        }
        if (i >= 69) {
            const auto original = contexts_[i - 69].toObject();
            const auto dom =
                Evaluate(view.page(),
                         "document.querySelector('.dictd_article').outerHTML")
                    .toString();
            QCOMPARE(WithoutActions(dom),
                     WithoutActions(original["html"].toString()));
            QCOMPARE(geometry["text"], original["text"]);
            const auto anchors = geometry["anchors"].toArray();
            QCOMPARE(anchors.size(), original["anchors"].toArray().size());
            for (const auto& a : anchors) {
                if (i == 72)
                    QCOMPARE(a.toObject()["href"].toString(),
                             QString("goldendict://lookup/a%01%20b"));
                else
                    QVERIFY(a.toObject()["href"].isNull());
            }
        }
        glyphs.append(geometry);
        QJsonObject row{{"index", i},
                        {"body", QString::fromUtf8(bodies[i])},
                        {"html", html},
                        {"glyphs", geometry}};
        if (!measurement.isEmpty())
            row["measurement"] =
                QJsonDocument::fromJson(
                    Evaluate(view.page(), measurement).toString().toUtf8())
                    .object();
        if (i < 69)
            observations.append(row);
        else {
            row["index"] = i - 69;
            context_observations.append(row);
        }
        if (!capture_.isEmpty()) {
            const auto name =
                i < 69
                    ? QString("control-%1.png").arg(i + 1, 2, 10, QChar('0'))
                    : QString("context-%1.png").arg(i - 68, 2, 10, QChar('0'));
            QVERIFY(view.grab().save(QDir(capture_).filePath(name)));
        }
    }
    // Retain all raw observations even when a later geometry assertion fails.
    if (!capture_.isEmpty()) {
        QVERIFY(Write(QDir(capture_).filePath("control-observations.json"),
                      QJsonDocument(observations).toJson()));
        QVERIFY(Write(QDir(capture_).filePath("context-observations.json"),
                      QJsonDocument(context_observations).toJson()));
    }
    for (int i = 0; i < 64; ++i) {
        const int c = i / 2 == 31 ? 127 : i / 2 + 1;
        if (c == 10)
            continue;
        const auto baseline = glyphs[(c == 9 ? 66 : 64) + i % 2].toObject();
        const auto actual_letters = glyphs[i].toObject()["letters"].toArray();
        const auto expected_letters = baseline["letters"].toArray();
        QCOMPARE(actual_letters.size(), expected_letters.size());
        for (int c = 0; c < actual_letters.size(); ++c) {
            for (const auto* key : {"y", "height"})
                QCOMPARE(actual_letters[c].toObject()[key],
                         expected_letters[c].toObject()[key]);
            // Blink's Range edges quantize at 1/64 CSS px when a text run is
            // split around the zero-width wrapper. This bound is measured
            // across the complete matrix; wrapper advance itself stays zero.
            for (const auto* key : {"x", "width"})
                QVERIFY(qAbs(actual_letters[c].toObject()[key].toDouble() -
                             expected_letters[c].toObject()[key].toDouble()) <=
                        1.0 / 64.0);
        }
        QCOMPARE(glyphs[i].toObject()["height"], baseline["height"]);
    }
    for (const int i : {69, 70}) {
        const auto spaces = glyphs[i].toObject()["spaces"].toArray();
        QCOMPARE(spaces.size(), 2);
        const auto width = glyphs[66]
                               .toObject()["spaces"]
                               .toArray()[0]
                               .toObject()["width"]
                               .toDouble();
        for (const auto& space : spaces) {
            QVERIFY(space.toObject()["width"].toDouble() > 0);
            QVERIFY(qAbs(space.toObject()["width"].toDouble() - width) <=
                    1.0 / 64.0);
        }
        QCOMPARE(glyphs[i].toObject()["height"],
                 glyphs[64].toObject()["height"]);
    }
    QCOMPARE(glyphs[68].toObject()["text"].toString(), QString("\t\v\f "));
    const auto empty_anchor =
        glyphs[68].toObject()["anchors"].toArray()[0].toObject();
    QVERIFY(empty_anchor["href"].isNull());
    QCOMPARE(empty_anchor["width"].toDouble(), 0.0);
}

QString DictdInlineDisplayTest::Article(int index) {
    goldendict::core::LookupQuery query;
    query.text = QString("case%1").arg(index, 2, 10, QChar('0')).toStdString();
    const auto response =
        facade_->GetDictionaryService().StartLookup(query)->Await();
    if (response.entries.size() != 1 || !response.errors.empty())
        return {};
    const QString html = QString::fromStdString(
        response.entries.front().article.sanitized_html.value_or(""));
    const auto begin = html.indexOf("<div class=\"dictd_article\"");
    const auto end = html.lastIndexOf("</section>");
    return begin < 0 || end < begin ? QString() : html.mid(begin, end - begin);
}

void DictdInlineDisplayTest::PreservesFrozenDisplay() {
    QWebEngineView view;
    view.resize(800, 600);
    view.settings()->setFontFamily(QWebEngineSettings::StandardFont, "Arial");
    view.settings()->setFontFamily(QWebEngineSettings::FixedFont,
                                   "Courier New");
    view.settings()->setFontSize(QWebEngineSettings::DefaultFontSize, 16);
    view.settings()->setFontSize(QWebEngineSettings::DefaultFixedFontSize, 13);
    view.settings()->setFontSize(QWebEngineSettings::MinimumFontSize, 0);
    view.settings()->setFontSize(QWebEngineSettings::MinimumLogicalFontSize, 0);
    view.show();
    const QString css =
        QString::fromUtf8(Read(QStringLiteral(GOLDENDICT_DICTD_CSS)));
    QVERIFY(!css.isEmpty());
    const QString measurement = QString::fromUtf8(
        Read(qEnvironmentVariable("GOLDENDICT_DICTD_MEASUREMENT")));
    if (!capture_.isEmpty())
        QVERIFY2(!measurement.isEmpty(),
                 "Native acceptance requires the frozen measurement script");
    QJsonArray observations;
    for (int i = 0; i < rows_.size(); ++i) {
        const auto row = rows_[i].toObject();
        const QString article = Article(i);
        QVERIFY2(!article.isEmpty(), qPrintable(QString::number(i)));
        QSignalSpy loaded(view.page(), &QWebEnginePage::loadFinished);
        view.setHtml(
            "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>" + css +
                "</style></head><body>" + article + "</body></html>",
            goldendict::app::ArticleContentBaseUrl());
        QVERIFY(!loaded.isEmpty() || loaded.wait(10000));
        QVERIFY(loaded.last()[0].toBool());
        QTest::qWait(50);
        const QString actual =
            Evaluate(view.page(),
                     "document.querySelector('.dictd_article').outerHTML")
                .toString();
        QCOMPARE(WithoutActions(actual),
                 WithoutActions(row["html"].toString()));
        const auto anchors = QJsonDocument::fromJson(
                                 Evaluate(view.page(),
                                          "JSON.stringify(Array.from(document."
                                          "querySelectorAll('.dictd_article "
                                          "a'),a=>({href:a.getAttribute('href')"
                                          ",text:a.textContent})))")
                                     .toString()
                                     .toUtf8())
                                 .array();
        const auto expected = row["anchors"].toArray();
        QCOMPARE(anchors.size(), expected.size());
        for (int a = 0; a < anchors.size(); ++a) {
            const QString original = expected[a].toObject()["raw"].toString();
            const auto origin = expected[a].toObject();
            const bool inert = origin["completeOpeningRewrite"] == "failed" ||
                               origin["normalizedEmpty"].toBool();
            const auto observed = anchors[a].toObject();
            QCOMPARE(observed["text"], expected[a].toObject()["text"]);
            if (inert)
                QVERIFY(observed["href"].isNull());
            else
                QCOMPARE(observed["href"].toString(),
                         "goldendict://lookup/" + original.mid(21));
        }
        QJsonObject observation{{"index", i},
                                {"body", row["body"]},
                                {"html", actual},
                                {"anchors", anchors}};
        const auto link_styles = LinkStyles(view.page());
        for (const auto& style : link_styles)
            QCOMPARE(style.toObject()["thickness"].toString(), QString("auto"));
        observation["linkStyles"] = link_styles;
        if (!measurement.isEmpty()) {
            observation["measurement"] =
                QJsonDocument::fromJson(
                    Evaluate(view.page(), measurement).toString().toUtf8())
                    .object();
        }
        observations.append(observation);
        if (!capture_.isEmpty()) {
            QVERIFY(view.grab().save(QDir(capture_).filePath(
                QString("case-%1.png").arg(i + 1, 2, 10, QChar('0')))));
        }
    }
    if (!capture_.isEmpty()) {
        QVERIFY(Write(QDir(capture_).filePath("observations.json"),
                      QJsonDocument(observations).toJson()));
        const QJsonObject environment{
            {"qt", qVersion()},
            {"viewportWidth", view.width()},
            {"viewportHeight", view.height()},
            {"viewDpr", view.devicePixelRatioF()},
            {"screenDpi", view.screen()->logicalDotsPerInch()},
            {"locale", QLocale().name()},
            {"font", QApplication::font().toString()},
            {"style", QApplication::style()->objectName()}};
        QVERIFY(Write(QDir(capture_).filePath("environment.json"),
                      QJsonDocument(environment).toJson()));
    }
}

void DictdInlineDisplayTest::Load(MainWindow& window, int index) {
    window.SubmitInitialLookup(QString("case%1").arg(index, 2, 10, QChar('0')));
    QTRY_VERIFY_WITH_TIMEOUT(window.requests_.empty(), 15000);
    QTRY_VERIFY_WITH_TIMEOUT(!window.article_page_->isLoading(), 15000);
    QTRY_VERIFY_WITH_TIMEOUT(
        Evaluate(window.article_page_,
                 "!!document.querySelector('.dictd_article')")
            .toBool(),
        15000);
    QTest::qWait(150);
}

void DictdInlineDisplayTest::KeepsInertApplicationState() {
    MainWindow window(directory_.path());
    window.SetFacade(facade_.get());
    window.SetHistoryItems({{"prior lookup", 0U}, {"older lookup", 1U}});
    // Give the actual search widget its own toolbar row in this harness.
    // Otherwise the many preceding text actions move it into overflow.
    auto* article_toolbar = window.findChild<QToolBar*>("articleToolbar");
    QVERIFY(article_toolbar);
    article_toolbar->toggleViewAction()->trigger();
    window.insertToolBarBreak(article_toolbar);
    window.resize(qMax(1800, article_toolbar->sizeHint().width() + 100), 800);
    window.show();
    window.CreateEmptyArticleTab(true);
    Load(window, 0);
    window.CreateEmptyArticleTab(true);
    Load(window, 1);
    QVERIFY(window.article_tabs_->count() >= 3);

    auto lease = std::make_shared<PlaybackLease>();
    QPointer<QTimer> player;
    window.audio_playback_service_ = std::make_unique<AudioPlaybackService>(
        nullptr, [lease, &player, &window](const QByteArray& bytes,
                                           const QString& type) {
            lease->bytes = bytes;
            lease->type = type;
            ++lease->starts;
            lease->active = true;
            auto* timer = new QTimer(window.audio_playback_service_.get());
            player = timer;
            QObject::connect(timer, &QTimer::timeout, timer,
                             [lease] { ++lease->ticks; });
            QObject::connect(timer, &QObject::destroyed,
                             [lease] { lease->active = false; });
            timer->start(10);
            return true;
        });
    goldendict::core::LookupQuery audio_query;
    audio_query.text = "clip";
    const auto audio =
        facade_->GetDictionaryService().StartLookup(audio_query)->Await();
    QVERIFY(!audio.entries.empty());
    QVERIFY(!audio.entries.front().resources.empty());
    const auto& resource = audio.entries.front().resources.front();
    const auto audio_url = QUrl::fromEncoded(
        "goldendict://resource/" +
        QUrl::toPercentEncoding(
            QString::fromStdString(resource.dictionary_id)) +
        "/" +
        QUrl::toPercentEncoding(QString::fromStdString(resource.resource_id)));
    QCOMPARE(window.audio_playback_service_->Play(*facade_, audio_url),
             AudioPlaybackService::Result::kStarted);
    QCOMPARE(lease->bytes, QByteArray("RIFFfixtureWAVE"));
    QCOMPARE(lease->type, QString("audio/wav"));
    QTRY_VERIFY(lease->active && lease->ticks > 1);
    auto* audio_owner = window.audio_playback_service_.get();
    const auto* player_identity = player.data();
    QCOMPARE(player->parent(), audio_owner);
    const auto tab_snapshot = [&window]() {
        QJsonArray tabs;
        for (int t = 0; t < window.article_tabs_->count(); ++t) {
            auto* widget = window.article_tabs_->widget(t);
            auto* view = qobject_cast<ArticleView*>(widget);
            auto* page = view ? view->page() : nullptr;
            auto* browser = view ? view->findChild<QWebEngineView*>() : nullptr;
            QJsonArray history;
            if (page)
                for (const auto& item : page->history()->items()) {
                    history.append(QJsonObject{
                        {"url", item.url().toString()},
                        {"originalUrl", item.originalUrl().toString()},
                        {"title", item.title()}});
                }
            const auto id = window.TabIdAt(t);
            QJsonObject search;
            const auto found = window.article_search_presentations_.find(id);
            if (found != window.article_search_presentations_.end()) {
                const auto& s = found->second;
                search = {
                    {"query", s.query},
                    {"status", s.status},
                    {"generation", QString::number(s.generation)},
                    {"accepted", QString::number(s.accepted_query_generation)},
                    {"mode", int(s.mode)},
                    {"matchCase", s.match_case},
                    {"ignoreOrder", s.ignore_word_order},
                    {"ignoreDiacritics", s.ignore_diacritics},
                    {"distance", s.maximum_word_distance
                                     ? QString::number(*s.maximum_word_distance)
                                     : QString()}};
            }
            tabs.append(
                QJsonObject{{"id", QString::number(id)},
                            {"widget", QString::number(quintptr(widget))},
                            {"view", QString::number(quintptr(view))},
                            {"page", QString::number(quintptr(page))},
                            {"browser", QString::number(quintptr(browser))},
                            {"tabTitle", window.article_tabs_->tabText(t)},
                            {"tabTooltip", window.article_tabs_->tabToolTip(t)},
                            {"pageTitle", page ? page->title() : QString()},
                            {"url", page ? page->url().toString() : QString()},
                            {"loading", page && page->isLoading()},
                            {"history", history},
                            {"historyIndex",
                             page ? page->history()->currentItemIndex() : -1},
                            {"zoom", view ? view->zoomFactor() : 0.0},
                            {"search", search}});
        }
        return QJsonObject{
            {"current", window.article_tabs_->currentIndex()},
            {"tabs", tabs},
            {"status", window.status_->text()},
            {"group", int(window.selected_group_id_)},
            {"searchVisible", window.article_search_->isVisible()}};
    };
    QJsonArray attempts;
    QJsonArray actual_styles;
    QList<int> inert_cases;
    for (int i = 0; i < rows_.size() + kSupplementBodies.size(); ++i)
        inert_cases.append(i);
    inert_cases << 304 << 306;
    for (const int i : inert_cases) {
        Load(window, i);
        const auto link_styles = LinkStyles(window.article_page_);
        for (const auto& style : link_styles)
            QCOMPARE(style.toObject()["thickness"].toString(), QString("auto"));
        actual_styles.append(
            QJsonObject{{"case", i}, {"linkStyles", link_styles}});
        QVERIFY(Evaluate(
                    window.article_page_,
                    "Array.from(document.querySelectorAll('.dictd_article "
                    "a.dictd_inert_reference')).every(a=>getComputedStyle(a)."
                    "color==='rgb(0, 0, "
                    "255)'&&getComputedStyle(a).textDecorationLine==='"
                    "underline')&&Array.from(document.querySelectorAll('.dictd_"
                    "control')).every(e=>getComputedStyle(e).fontSize==='0px'&&"
                    "e.getBoundingClientRect().width===0)&&Array.from(document."
                    "querySelectorAll('.dictd_article,.dictd_article "
                    "div')).every(e=>getComputedStyle(e).unicodeBidi===(e."
                    "hasAttribute('dir')?'embed':'normal'))&&Array.from("
                    "document.querySelectorAll('.dictd_phonetic')).every(e=>"
                    "getComputedStyle(e).color==='rgb(0, 153, "
                    "0)'&&getComputedStyle(e).fontStyle==='italic'&&['::before'"
                    ",'::after'].every(p=>getComputedStyle(e,p).fontStyle==='"
                    "normal'&&getComputedStyle(e,p).content.includes('\\\\')))")
                    .toBool());
        if (i == 0) {
            auto resource_css =
                QString::fromUtf8(Read(QStringLiteral(GOLDENDICT_DICTD_CSS)));
            resource_css.remove(QRegularExpression("\\s+"));
            auto actual_css = Evaluate(window.article_page_,
                                       "Array.from(document.querySelectorAll('"
                                       "style'),e=>e.textContent).join('')")
                                  .toString();
            actual_css.remove(QRegularExpression("\\s+"));
            for (const auto* selector :
                 {".dictd_article,.dictd_articlediv{",
                  ".dictd_article[dir],.dictd_articlediv[dir]{",
                  ".dictd_articlespan.dictd_control{",
                  ".dictd_phonetic:before,.dictd_phonetic:after{",
                  ".dictd_phonetic:after{", ".dictd_phonetic{",
                  ".dictd_articlea:link{", ".dictd_articlea{",
                  ".dictd_articlea.dictd_inert_reference{"}) {
                const int begin =
                    resource_css.indexOf(QString::fromLatin1(selector));
                QVERIFY(begin >= 0);
                const int end = resource_css.indexOf('}', begin);
                auto rule = resource_css.mid(begin, end - begin + 1);
                rule.remove(QRegularExpression("\\s+"));
                QVERIFY2(actual_css.contains(rule), qPrintable(rule));
            }
        }
        window.search_in_page_action_->trigger();
        window.article_search_->setText("kept search");
        QTest::qWait(250);
        QVERIFY(window.article_search_->isVisible());
        auto* page = window.article_page_;
        auto* view = window.article_view_->findChild<QWebEngineView*>();
        QVERIFY(view && view->focusProxy());
        // Search stays open; settle the ordinary pointer-focus transfer before
        // observing activation, since focus itself advances the UI epoch.
        view->setFocus(Qt::OtherFocusReason);
        QTest::qWait(100);
        Evaluate(page,
                 "window.dictdHits=[];document.addEventListener('mousedown',e=>"
                 "{const "
                 "a=e.target.closest('a');if(a)dictdHits.push({index:Array."
                 "from(document.querySelectorAll('.dictd_article "
                 "a')).indexOf(a),button:e.button,ctrl:e.ctrlKey})},true)");
        const auto anchors =
            QJsonDocument::fromJson(
                Evaluate(
                    page,
                    "JSON.stringify(Array.from(document.querySelectorAll('."
                    "dictd_article "
                    "a'),(a,index)=>({index,href:a.getAttribute('href')})))")
                    .toString()
                    .toUtf8())
                .array();
        for (const auto& anchor_value : anchors) {
            const auto anchor = anchor_value.toObject();
            if (!anchor["href"].isNull())
                continue;
            const int a = anchor["index"].toInt();
            for (const auto button : {Qt::LeftButton, Qt::MiddleButton}) {
                const QString hit_script =
                    QString(
                        "(()=>{const "
                        "a=document.querySelectorAll('.dictd_article "
                        "a')[%1];a.scrollIntoView({block:'center'});const "
                        "r=Array.from(a.getClientRects()).find(r=>r.width>0&&r."
                        "height>0);if(!r)return null;const "
                        "x=r.left+r.width/2,y=r.top+r.height/2;return "
                        "JSON.stringify({x,y,hit:a.contains(document."
                        "elementFromPoint(x,y))})})()")
                        .arg(a);
                const auto hit =
                    QJsonDocument::fromJson(
                        Evaluate(page, hit_script).toString().toUtf8())
                        .object();
                const bool expected_zero =
                    (i == 12 || i == 23 || i == 48 || i == 306) && a == 0;
                QCOMPARE(hit.isEmpty(), expected_zero);
                if (hit.isEmpty()) {
                    // Only these exact fragments were observed to have no
                    // hit area in the immutable Qt5 evidence.
                    attempts.append(QJsonObject{{"case", i},
                                                {"anchor", a},
                                                {"zeroRenderedArea", true}});
                    break;
                }
                QVERIFY2(
                    hit["hit"].toBool(),
                    qPrintable(QString("case %1 anchor %2").arg(i).arg(a)));
                QTest::qWait(80);
                const auto session = facade_->ExportArticleTabSession();
                const auto all_tabs = tab_snapshot();
                const auto generations = window.article_navigation_generations_;
                const auto mru = window.mru_tab_ids_;
                const auto query = window.query_->text();
                const auto search = window.article_search_->text();
                const auto search_status =
                    window.article_search_status_->text();
                const auto history = window.history_items_;
                const auto url = page->url();
                const auto browser_history = page->history()->count();
                const auto pending = window.requests_.size();
                const auto mutation = window.presentation_mutation_epoch_;
                const int ticks = lease->ticks;
                const int hits = Evaluate(page, "dictdHits.length").toInt();
                QTest::qWait(80);
                QCOMPARE(tab_snapshot(), all_tabs);
                std::vector<std::unique_ptr<QSignalSpy>> other_loads;
                for (int t = 0; t < window.article_tabs_->count(); ++t) {
                    auto* tab_view = qobject_cast<ArticleView*>(
                        window.article_tabs_->widget(t));
                    QVERIFY(tab_view && tab_view->page());
                    other_loads.push_back(std::make_unique<QSignalSpy>(
                        tab_view->page(), &QWebEnginePage::loadStarted));
                }
                QSignalSpy lookup(&window, &MainWindow::LookupSubmitted);
                QSignalSpy session_signal(
                    &window, &MainWindow::ArticleTabSessionMutated);
                QSignalSpy navigation(page, &ArticlePage::LookupRequested);
                QSignalSpy loaded(page, &QWebEnginePage::loadStarted);
                QTest::mouseClick(view->focusProxy(), button, Qt::NoModifier,
                                  QPoint(qRound(hit["x"].toDouble()),
                                         qRound(hit["y"].toDouble())));
                QTRY_COMPARE(Evaluate(page, "dictdHits.length").toInt(),
                             hits + 1);
                QCOMPARE(Evaluate(page, "dictdHits[dictdHits.length-1].index")
                             .toInt(),
                         a);
                QCOMPARE(Evaluate(page, "dictdHits[dictdHits.length-1].button")
                             .toInt(),
                         button == Qt::LeftButton ? 0 : 1);
                QTest::qWait(150);
                QVERIFY(lookup.isEmpty() && session_signal.isEmpty() &&
                        navigation.isEmpty() && loaded.isEmpty());
                QVERIFY(facade_->ExportArticleTabSession() == session);
                QCOMPARE(tab_snapshot(), all_tabs);
                for (const auto& spy : other_loads)
                    QVERIFY(spy->isEmpty());
                QCOMPARE(window.article_navigation_generations_, generations);
                QCOMPARE(window.mru_tab_ids_, mru);
                QCOMPARE(window.query_->text(), query);
                QCOMPARE(window.article_search_->text(), search);
                QCOMPARE(window.article_search_status_->text(), search_status);
                QVERIFY(window.article_search_->isVisible());
                QCOMPARE(window.history_items_.size(), history.size());
                for (std::size_t h = 0; h < history.size(); ++h) {
                    QCOMPARE(window.history_items_[h].word, history[h].word);
                    QCOMPARE(window.history_items_[h].group_id,
                             history[h].group_id);
                }
                QCOMPARE(window.article_page_, page);
                QCOMPARE(page->url(), url);
                QCOMPARE(page->history()->count(), browser_history);
                QCOMPARE(window.requests_.size(), pending);
                QCOMPARE(window.presentation_mutation_epoch_, mutation);
                QCOMPARE(window.audio_playback_service_.get(), audio_owner);
                QVERIFY(player && player->isActive() && lease->active &&
                        lease->ticks > ticks);
                QCOMPARE(player.data(), player_identity);
                QCOMPARE(player->parent(), audio_owner);
                QCOMPARE(lease->bytes, QByteArray("RIFFfixtureWAVE"));
                QCOMPARE(lease->type, QString("audio/wav"));
                QCOMPARE(lease->starts, 1);
                attempts.append(QJsonObject{
                    {"case", i},
                    {"anchor", a},
                    {"button", button == Qt::LeftButton ? "left" : "middle"},
                    {"hit", hit},
                    {"stateBefore", all_tabs},
                    {"stateAfter", tab_snapshot()},
                    {"tabs", int(session.tabs.size())},
                    {"audioTicksBefore", ticks},
                    {"audioTicksAfter", lease->ticks}});
            }
        }
    }
    // Every successful original anchor, including each reconstructed clone,
    // and the narrow N4 controls traverse both real dispatch paths.
    QJsonArray active_attempts;
    for (int i : {2,  4,  5,  7,  8,  9,  15, 16, 17, 18,
                  20, 21, 22, 52, 53, 54, 55, 56, 60, 303}) {
        const auto expected = i < 48 ? rows_[i].toObject()["anchors"].toArray()
                              : i == 303
                                  ? contexts_[3].toObject()["anchors"].toArray()
                                  : QJsonArray{QJsonObject()};
        for (int a = 0; a < expected.size(); ++a) {
            for (const auto button : {Qt::LeftButton, Qt::MiddleButton}) {
                Load(window, i);
                auto* page = window.article_page_;
                QSignalSpy navigation(page, &ArticlePage::LookupRequested);
                QSignalSpy lookup(&window, &MainWindow::LookupSubmitted);
                const int tabs = window.article_tabs_->count();
                const QStringList supplemental_targets = {
                    "../word", "/word", "...", "%2e", "&amp;#46;"};
                const auto target =
                    i < 48 || i == 303
                        ? QUrl::fromPercentEncoding(expected[a]
                                                        .toObject()["raw"]
                                                        .toString()
                                                        .mid(21)
                                                        .toUtf8())
                    : i == 60 ? QString::fromUtf8(u8"你好%/#")
                              : supplemental_targets[i - 52];
                const auto point =
                    QJsonDocument::fromJson(
                        Evaluate(
                            page,
                            QString(
                                "JSON.stringify((()=>{window.dictdHits=[];"
                                "document.addEventListener('mousedown',e=>{"
                                "const "
                                "a=e.target.closest('a');if(a)dictdHits.push({"
                                "index:Array.from(document.querySelectorAll('."
                                "dictd_article "
                                "a')).indexOf(a),button:e.button})},true);"
                                "const "
                                "a=document.querySelectorAll('.dictd_article "
                                "a')[%1];a.scrollIntoView({block:'center'});"
                                "const "
                                "r=Array.from(a.getClientRects()).find(r=>r."
                                "width>0&&r.height>0);if(!r)return null;const "
                                "x=r.left+r.width/2,y=r.top+r.height/2;return "
                                "{x,y,hit:a.contains(document.elementFromPoint("
                                "x,y))}})())")
                                .arg(a))
                            .toString()
                            .toUtf8())
                        .object();
                QVERIFY(point["hit"].toBool());
                auto* view = window.article_view_->findChild<QWebEngineView*>();
                const QPoint position(qRound(point["x"].toDouble()),
                                      qRound(point["y"].toDouble()));
                QTest::mousePress(view->focusProxy(), button, Qt::NoModifier,
                                  position);
                QTRY_COMPARE(Evaluate(page, "dictdHits.length").toInt(), 1);
                QCOMPARE(Evaluate(page, "dictdHits[0].index").toInt(), a);
                QCOMPARE(Evaluate(page, "dictdHits[0].button").toInt(),
                         button == Qt::LeftButton ? 0 : 1);
                QTest::mouseRelease(view->focusProxy(), button, Qt::NoModifier,
                                    position);
                QTRY_COMPARE(navigation.size(), 1);
                QCOMPARE(navigation.front()[0].toString(), QString(target));
                QTRY_COMPARE(lookup.size(), 1);
                QCOMPARE(lookup.front()[0].toString(), QString(target));
                QTRY_COMPARE(window.article_tabs_->count(),
                             tabs + (button == Qt::MiddleButton ? 1 : 0));
                QTRY_VERIFY(window.requests_.empty());
                active_attempts.append(QJsonObject{
                    {"case", i},
                    {"anchor", a},
                    {"target", target},
                    {"button", button == Qt::LeftButton ? "left" : "middle"},
                    {"hit", point}});
                if (button == Qt::MiddleButton)
                    window.CloseArticleTab(
                        window.article_tabs_->currentIndex());
            }
        }
    }
    window.audio_playback_service_.reset();
    QVERIFY(!player && !lease->active);
    if (!capture_.isEmpty()) {
        QVERIFY(Write(QDir(capture_).filePath("activation.json"),
                      QJsonDocument(attempts).toJson()));
        QVERIFY(Write(QDir(capture_).filePath("active-controls.json"),
                      QJsonDocument(active_attempts).toJson()));
        QVERIFY(Write(QDir(capture_).filePath("actual-document-styles.json"),
                      QJsonDocument(actual_styles).toJson()));
    }
}

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_Use96Dpi);
    QLocale::setDefault(QLocale("en_US"));
    QWebEngineUrlScheme scheme("goldendict");
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::HostAndPort);
    scheme.setDefaultPort(0);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme |
                    QWebEngineUrlScheme::LocalScheme |
                    QWebEngineUrlScheme::LocalAccessAllowed);
    QWebEngineUrlScheme::registerScheme(scheme);
    QApplication application(argc, argv);
    QApplication::setStyle("Fusion");
    QApplication::setFont(QFont("Arial", 12));
    DictdInlineDisplayTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "dictd_inline_display_test.moc"
