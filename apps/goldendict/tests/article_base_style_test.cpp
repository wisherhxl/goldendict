// SPDX-License-Identifier: GPL-3.0-or-later
#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QScreen>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QWebEngineSettings>
#include <QWebEngineUrlScheme>
#include <QWebEngineView>
#include <QtTest>

#include "article_view.h"
#include "goldendict/core/application.h"
#include "main_window.h"

namespace {
QByteArray Read(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool Write(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::NewOnly) &&
           file.write(bytes) == bytes.size();
}

QByteArray Hash(const QByteArray& bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
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

void Append32(quint32 value, QByteArray* bytes) {
    for (int shift = 24; shift >= 0; shift -= 8)
        bytes->append(char(value >> shift));
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

QJsonObject Files(const QString& directory) {
    QJsonObject files;
    for (const auto& name : QDir(directory).entryList(QDir::Files))
        files[name] =
            QString::fromLatin1(Hash(Read(QDir(directory).filePath(name))));
    return files;
}

QString Rule(QString css, const QString& selector) {
    css.remove(QRegularExpression(
        "/\\*.*?\\*/", QRegularExpression::DotMatchesEverythingOption));
    const auto match = QRegularExpression("(?:^|})\\s*" +
                                          QRegularExpression::escape(selector) +
                                          "\\s*\\{([^}]*)}")
                           .match(css);
    QString rule = match.captured(1);
    rule.remove(QRegularExpression("\\s+"));
    if (rule.endsWith(';'))
        rule.chop(1);
    return rule;
}

const char kMeasure[] = R"JS(
(() => {
 const measure = e => {
   const s=getComputedStyle(e), r=e.getBoundingClientRect(), range=document.createRange();
   range.selectNodeContents(e);
   const lines=Array.from(range.getClientRects()).filter(r=>r.width>0).map(r=>({x:r.x,y:r.y,width:r.width,height:r.height}));
   return {text:e.textContent,font:s.fontFamily,size:s.fontSize,line:s.lineHeight,color:s.color,
     background:s.backgroundColor,margin:[s.marginTop,s.marginRight,s.marginBottom,s.marginLeft],
     padding:[s.paddingTop,s.paddingRight,s.paddingBottom,s.paddingLeft],max:s.maxWidth,box:s.boxSizing,
     whitespace:s.whiteSpace,wrap:s.overflowWrap,overflow:s.overflowX,x:r.x,y:r.y,width:r.width,height:r.height,
     lineCount:new Set(lines.map(r=>r.y)).size,lines,weight:s.fontWeight,fontStyle:s.fontStyle,
     border:[s.borderTopWidth,s.borderTopStyle,s.borderTopColor],display:s.display};
 };
 const article=document.querySelector('.sdct_h');
 return JSON.stringify({body:measure(document.body),plain:measure(article.querySelector('p')),
   token:measure(article.querySelector('div')),pre:measure(article.querySelector('pre')),
   dictd:measure(document.querySelector('.dictd_article')),phonetic:measure(document.querySelector('.dictd_phonetic')),
   pseudo:getComputedStyle(document.querySelector('.dictd_phonetic'),'::before').content,
   links:Array.from(document.querySelectorAll('.dictd_article a')).map(e=>({href:e.getAttribute('href'),color:getComputedStyle(e).color,decoration:getComputedStyle(e).textDecorationLine})),
   dpr:devicePixelRatio,viewport:innerWidth,documentWidth:document.documentElement.scrollWidth,
   headings:Array.from(document.querySelectorAll('body > section.gd-dictionary-result > h2')).map(measure),
   results:Array.from(document.querySelectorAll('body > section.gd-dictionary-result')).map(measure),
   dark:matchMedia('(prefers-color-scheme:dark)').matches,html:document.documentElement.outerHTML});
})()
)JS";
}  // namespace

class ArticleBaseStyleTest : public QObject {
    Q_OBJECT
   private slots:
    void PublishesColdAndWarmBaseStyle();
    void HeadingStates();
};

void ArticleBaseStyleTest::PublishesColdAndWarmBaseStyle() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fixture = directory.filePath("sources");
    const QString indexes = directory.filePath("indexes");
    QVERIFY(QDir().mkpath(fixture));
    const QByteArray token(240, 'W');
    const QByteArray html =
        "<p>Alpha beta gamma delta epsilon zeta eta theta iota kappa lambda "
        "mu.</p><div>" +
        token + "</div><pre>first  column\tsecond\n" + token +
        "\nlast line</pre>";
    QByteArray index("canvas\0", 7);
    Append32(0, &index);
    Append32(html.size(), &index);
    QVERIFY(Write(QDir(fixture).filePath("fixture.ifo"),
                  "StarDict's dict ifo file\nversion=2.4.2\nbookname=Base "
                  "fixture\nwordcount=1\nidxfilesize=" +
                      QByteArray::number(index.size()) +
                      "\nsametypesequence=h\n"));
    QVERIFY(Write(QDir(fixture).filePath("fixture.idx"), index));
    QVERIFY(Write(QDir(fixture).filePath("fixture.dict"), html));
    const QByteArray dictd("ordinary \\phonetic\\ {word} {.}");
    QVERIFY(Write(QDir(fixture).filePath("inline.index"),
                  "canvas\tA\t" + Base64Integer(dictd.size()).toUtf8() + "\n"));
    QVERIFY(Write(QDir(fixture).filePath("inline.dict"), dictd));
    const auto source_hashes = Files(fixture);
    const QString capture =
        qEnvironmentVariable("GOLDENDICT_BASE_STYLE_CAPTURE_DIR");
    if (!capture.isEmpty()) {
        QVERIFY(QDir().mkpath(capture));
        QVERIFY(QDir().mkpath(QDir(capture).filePath("sources")));
        for (const auto& name : QDir(fixture).entryList(QDir::Files))
            QVERIFY(Write(QDir(capture).filePath("sources/" + name),
                          Read(QDir(fixture).filePath(name))));
    }
    goldendict::core::CoreConfiguration config;
    config.dictionary_paths = {fixture.toStdString()};
    config.index_directory = indexes.toStdString();
    config.preferences.collapse_large_articles = false;
    QJsonArray observations;
    QJsonObject cold_cache;
    QString cold_html;
    const bool expect_dark =
        qEnvironmentVariableIsSet("GOLDENDICT_BASE_STYLE_EXPECT_DARK");
    for (int restart = 0; restart < 2; ++restart) {
        auto facade = goldendict::core::CreateDesktopFacade(config);
        MainWindow window(directory.path());
        window.SetFacade(facade.get());
        window.resize(1900, 800);
        window.show();
        auto* tabs = window.findChild<QTabWidget*>("articleTabs");
        QVERIFY(tabs);
        auto* view = qobject_cast<ArticleView*>(tabs->currentWidget());
        QVERIFY(view);
        auto* web = view->findChild<QWebEngineView*>("articleWebContent");
        QVERIFY(web);
        const QJsonObject defaults{
            {"standard",
             web->settings()->fontFamily(QWebEngineSettings::StandardFont)},
            {"fixed",
             web->settings()->fontFamily(QWebEngineSettings::FixedFont)},
            {"default",
             web->settings()->fontSize(QWebEngineSettings::DefaultFontSize)},
            {"defaultFixed", web->settings()->fontSize(
                                 QWebEngineSettings::DefaultFixedFontSize)}};
        window.SubmitInitialLookup("canvas");
        QTRY_VERIFY_WITH_TIMEOUT(
            Evaluate(view->page(),
                     "!!document.querySelector('.dictd_phonetic') && "
                     "!!document.querySelector('pre')")
                .toBool(),
            15000);
        QVERIFY(!view->page()->isLoading());
        const QString document =
            Evaluate(view->page(), "document.documentElement.outerHTML")
                .toString();
        QVERIFY(document.contains("Content-Security-Policy"));
        QVERIFY(document.contains("script-src 'none'"));
        const QString css =
            Evaluate(view->page(),
                     "document.querySelector('style').textContent")
                .toString();
        const QString resource = QString::fromUtf8(Read(":/article-style.css"));
        QVERIFY(!resource.isEmpty());
        QString body_rule = Rule(css, "body");
        QVERIFY(body_rule.contains("color:#000;"));
        body_rule.remove("color:#000;");
        QCOMPARE(body_rule, Rule(resource, "body"));
        QCOMPARE(Rule(css, "pre"), Rule(resource, "pre"));
        QString heading_rule = Rule(css,
                                    "body>section.gd-dictionary-result>h2,"
                                    "body>section.gd-dictionary-result>details."
                                    "gd-collapsed-article>summary");
        heading_rule.remove(";margin-left:0;margin-right:0");
        QCOMPARE(heading_rule, Rule(resource, ".gddictname"));
        QCOMPARE(Rule(css, "body>section.gd-dictionary-result"),
                 Rule(resource, ".gdarticle"));
        QCOMPARE(Rule(css, "body>section.gd-dictionary-result:after"),
                 Rule(resource, ".gdarticle:after"));
        QVERIFY(!css.contains("color-scheme:light dark"));
        if (!restart) {
            cold_html = document;
            cold_cache = Files(indexes);
        } else {
            QCOMPARE(document, cold_html);
            QCOMPARE(Files(indexes), cold_cache);
        }
        QVERIFY(cold_cache.size() >=
                3);  // StarDict records and two FTS caches.
        QCOMPARE(Files(fixture), source_hashes);
        for (int width : {320, 800, 1600}) {
            web->setFixedSize(width, 600);
            QTRY_COMPARE_WITH_TIMEOUT(
                Evaluate(view->page(), "innerWidth").toInt(), width, 5000);
            QTest::qWait(80);
            const auto measured =
                QJsonDocument::fromJson(
                    Evaluate(view->page(), kMeasure).toString().toUtf8())
                    .object();
            QVERIFY(!measured.isEmpty());
            const auto body = measured["body"].toObject();
            const auto pre = measured["pre"].toObject();
            const auto normal = measured["plain"].toObject();
            QCOMPARE(measured["viewport"].toInt(), width);
            QCOMPARE(measured["dark"].toBool(), expect_dark);
            QCOMPARE(body["background"].toString(), "rgb(254, 253, 235)");
            QCOMPARE(body["color"].toString(), "rgb(0, 0, 0)");
            QCOMPARE(body["size"].toString(), "13px");
            QVERIFY(body["font"].toString().startsWith("Tahoma, Verdana,"));
            QCOMPARE(body["margin"].toArray(),
                     (QJsonArray{"8px", "8px", "8px", "8px"}));
            QCOMPARE(body["padding"].toArray(),
                     (QJsonArray{"0px", "0px", "0px", "0px"}));
            QCOMPARE(body["max"].toString(), "none");
            QCOMPARE(body["box"].toString(), "content-box");
            QCOMPARE(body["wrap"].toString(), "normal");
            QCOMPARE(body["x"].toDouble(), 8.0);
            QCOMPARE(body["width"].toDouble(), width - 16.0);
            QCOMPARE(pre["size"].toString(), "12px");
            QCOMPARE(pre["whitespace"].toString(), "pre");
            QCOMPARE(pre["overflow"].toString(), "visible");
            QCOMPARE(pre["lineCount"].toInt(), 3);
            QCOMPARE(measured["token"].toObject()["lineCount"].toInt(), 1);
            if (width == 320)
                QVERIFY(normal["lineCount"].toInt() > 1);
            else
                QCOMPARE(normal["lineCount"].toInt(), 1);
            QVERIFY(measured["documentWidth"].toDouble() > width);
            QCOMPARE(measured["dictd"].toObject()["font"], body["font"]);
            QCOMPARE(measured["dictd"].toObject()["size"], body["size"]);
            QCOMPARE(measured["dictd"].toObject()["line"], body["line"]);
            QCOMPARE(normal["size"], body["size"]);
            QCOMPARE(normal["color"], body["color"]);
            QCOMPARE(pre["color"], body["color"]);
            const auto headings = measured["headings"].toArray();
            const auto results = measured["results"].toArray();
            QCOMPARE(headings.size(), 2);
            for (int i = 0; i < headings.size(); ++i) {
                const auto heading = headings[i].toObject();
                QCOMPARE(heading["size"].toString(), "14px");
                QCOMPARE(heading["weight"].toString(), "700");
                QCOMPARE(heading["border"].toArray(),
                         (QJsonArray{"1px", "dotted", "rgb(0, 0, 0)"}));
                QCOMPARE(heading["x"].toDouble(), 8.0);
                QCOMPARE(heading["width"].toDouble(), width - 16.0);
                QCOMPARE(results[i].toObject()["margin"].toArray(),
                         (QJsonArray{"-8px", "0px", "8px", "0px"}));
                QCOMPARE(results[i].toObject()["padding"].toArray(),
                         (QJsonArray{"1px", "0px", "0px", "0px"}));
            }
            QCOMPARE(measured["dictd"].toObject()["color"], body["color"]);
            QCOMPARE(measured["phonetic"].toObject()["color"].toString(),
                     "rgb(0, 153, 0)");
            QVERIFY(measured["pseudo"].toString().contains('\\'));
            const auto links = measured["links"].toArray();
            QCOMPARE(links.size(), 2);
            QVERIFY(!links[0].toObject()["href"].isNull());
            QVERIFY(links[1].toObject()["href"].isNull());
            for (const auto& link : links) {
                QCOMPARE(link.toObject()["color"].toString(), "rgb(0, 0, 255)");
                QCOMPARE(link.toObject()["decoration"].toString(), "underline");
            }
            if (!capture.isEmpty()) {
                QCOMPARE(measured["dpr"].toDouble(), 1.0);
                QCOMPARE(web->devicePixelRatioF(), 1.0);
                const QString name =
                    QString("qt6-%1-%2.png").arg(restart).arg(width);
                const QImage raster = web->grab().toImage();
                QVERIFY(raster.save(QDir(capture).filePath(name)));
                QCOMPARE(raster.pixelColor(20, 500), QColor(254, 253, 235));
                int black_pixels = 0;
                const QRect paragraph =
                    QRectF(normal["x"].toDouble(), normal["y"].toDouble(),
                           normal["width"].toDouble(),
                           normal["height"].toDouble())
                        .toAlignedRect()
                        .intersected(raster.rect());
                for (int y = paragraph.top(); y <= paragraph.bottom(); ++y)
                    for (int x = paragraph.left(); x <= paragraph.right(); ++x)
                        black_pixels +=
                            raster.pixelColor(x, y) == QColor(Qt::black);
                QVERIFY(black_pixels > 0);
                observations.append(QJsonObject{
                    {"restart", restart},
                    {"width", width},
                    {"measurement", measured},
                    {"defaults", defaults},
                    {"widgetDpr", web->devicePixelRatioF()},
                    {"zoom", view->zoomFactor()},
                    {"canvasPixel", raster.pixelColor(20, 500).name()},
                    {"paragraphBlackPixels", black_pixels},
                    {"screenshot", name},
                    {"sha256", QString::fromLatin1(
                                   Hash(Read(QDir(capture).filePath(name))))}});
            }
        }
    }
    if (!capture.isEmpty()) {
        QVERIFY(Write(QDir(capture).filePath("observations.json"),
                      QJsonDocument(observations).toJson()));
        QVERIFY(
            Write(QDir(capture).filePath("environment.json"),
                  QJsonDocument(
                      QJsonObject{
                          {"qt", qVersion()},
                          {"platform", QGuiApplication::platformName()},
                          {"locale", QLocale().name()},
                          {"appFont", QApplication::font().toString()},
                          {"screenDpi",
                           QApplication::primaryScreen()->logicalDotsPerInch()},
                          {"sourceHashes", source_hashes},
                          {"cacheHashes", cold_cache}})
                      .toJson()));
    }
}

void ArticleBaseStyleTest::HeadingStates() {
    using namespace goldendict::core;
    const QString capture =
        qEnvironmentVariable("GOLDENDICT_HEADING_CAPTURE_DIR");
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QByteArray names[] = {
        "First <dictionary> & \"name\"",
        QByteArray(12, 'W') + " Long dictionary " + QByteArray(35, 'W')};
    for (int dictionary = 0; dictionary < 2; ++dictionary) {
        QByteArray index, body;
        const int count = dictionary == 0 ? 2 : 1;
        for (int i = 0; i < count; ++i) {
            const QByteArray text = "<p>Article body " +
                                    QByteArray::number(dictionary * 2 + i) +
                                    "</p>";
            index.append("canvas\0", 7);
            Append32(body.size(), &index);
            Append32(text.size(), &index);
            body += text;
        }
        const QString stem = dictionary == 0 ? "fixture" : "second";
        QVERIFY(Write(fixture.filePath(stem + ".ifo"),
                      "StarDict's dict ifo file\nversion=2.4.2\nbookname=" +
                          names[dictionary] +
                          "\nwordcount=" + QByteArray::number(count) +
                          "\nidxfilesize=" + QByteArray::number(index.size()) +
                          "\nsametypesequence=h\n"));
        QVERIFY(Write(fixture.filePath(stem + ".idx"), index));
        QVERIFY(Write(fixture.filePath(stem + ".dict"), body));
    }
    if (!capture.isEmpty()) {
        QVERIFY(QDir().mkpath(capture + "/sources"));
        for (const auto& name : QDir(fixture.path()).entryList(QDir::Files))
            QVERIFY(Write(capture + "/sources/" + name,
                          Read(fixture.filePath(name))));
    }
    std::string plain_text;
    for (bool collapse : {true, false}) {
        CoreConfiguration config;
        config.dictionary_paths = {fixture.path().toStdString()};
        config.index_directory = fixture.filePath("indexes").toStdString();
        config.preferences.collapse_large_articles = collapse;
        config.preferences.article_size_limit = 1;
        auto facade = CreateDesktopFacade(config);
        LookupQuery query;
        query.text = "canvas";
        const auto response = facade->GetDictionaryService().Lookup(query);
        QCOMPARE(response.entries.size(), std::size_t{3});
        QVERIFY(response.errors.empty());
        QCOMPARE(response.entries[0].dictionary.name, names[0].toStdString());
        QCOMPARE(response.entries[1].dictionary.id,
                 response.entries[0].dictionary.id);
        QCOMPARE(response.entries[2].dictionary.name, names[1].toStdString());
        const auto page = facade->ComposeLookupPage(response);
        if (plain_text.empty())
            plain_text = page.plain_text;
        QCOMPARE(page.plain_text, plain_text);
        ArticleView view;
        view.resize(1800, 700);
        view.show();
        auto* web = view.findChild<QWebEngineView*>("articleWebContent");
        QVERIFY(web);
        QSignalSpy loaded(&view, &ArticleView::loadFinished);
        view.setHtml(QString::fromStdString(*page.sanitized_html));
        QVERIFY(!loaded.isEmpty() || loaded.wait(10000));
        QVERIFY(loaded.last()[0].toBool());
        for (int width : {320, 800, 1600}) {
            web->setFixedSize(width, 600);
            QTRY_COMPARE(Evaluate(view.page(), "innerWidth").toInt(), width);
            const auto measure = [&] {
                auto result = QJsonDocument::fromJson(
                                  Evaluate(view.page(), QStringLiteral(R"JS(
                (() => {
                  const sections=[...document.querySelectorAll('body > section')];
                  const rows=sections.map(e=>{
                    const h=e.querySelector(':scope > details > summary, :scope > h2');
                    const s=getComputedStyle(h),r=h.getBoundingClientRect();
                    const p=getComputedStyle(e,':after'), title=h.querySelector('h2')||h;
                    const t=title.getBoundingClientRect(), ts=getComputedStyle(title);
                    return {text:h.textContent,x:r.x,y:r.y,width:r.width,height:r.height,
                      border:[s.borderTopWidth,s.borderTopStyle,s.borderTopColor],
                      margin:[s.marginTop,s.marginRight,s.marginBottom,s.marginLeft],
                      padding:[s.paddingTop,s.paddingRight,s.paddingBottom,s.paddingLeft],
                      size:s.fontSize,weight:s.fontWeight,line:s.lineHeight,marker:s.listStylePosition,
                      markerSize:getComputedStyle(h,'::marker').fontSize,
                      title:[t.x,t.y,t.width,t.height,ts.borderTopStyle,ts.marginTop,ts.fontSize],
                      clear:[p.content,p.display,p.height,p.clear],
                      hits:[0,2,4,7,8,12].map(x=>({x,tag:document.elementFromPoint(x,r.y+r.height/2)?.tagName}))};
                  });
                  return JSON.stringify({rows,viewport:innerWidth,dpr:devicePixelRatio,dark:matchMedia('(prefers-color-scheme:dark)').matches,
                    documentWidth:document.documentElement.scrollWidth});
                })()
                )JS"))
                                      .toString()
                                      .toUtf8())
                                  .object();
                result["widgetDpr"] = web->devicePixelRatioF();
                result["zoom"] = view.zoomFactor();
                return result;
            };
            const auto measured = measure();
            QVERIFY(!measured.isEmpty());
            QCOMPARE(
                measured["dark"].toBool(),
                qEnvironmentVariableIsSet("GOLDENDICT_BASE_STYLE_EXPECT_DARK"));
            if (!capture.isEmpty()) {
                QCOMPARE(measured["dpr"].toDouble(), 1.0);
                QCOMPARE(measured["widgetDpr"].toDouble(), 1.0);
                QCOMPARE(measured["zoom"].toDouble(), 1.0);
                QVERIFY(QDir().mkpath(capture));
                const QString name =
                    QString("headings-%1-%2.png").arg(collapse).arg(width);
                QVERIFY(web->grab().save(QDir(capture).filePath(name)));
                QVERIFY(
                    Write(QDir(capture).filePath(QString("headings-%1-%2.json")
                                                     .arg(collapse)
                                                     .arg(width)),
                          QJsonDocument(measured).toJson()));
            }
            const auto rows = measured["rows"].toArray();
            QCOMPARE(rows.size(), 3);
            for (int i = 0; i < rows.size(); ++i) {
                const auto row = rows[i].toObject();
                QCOMPARE(row["text"].toString(),
                         QString::fromStdString(
                             response.entries[i].dictionary.name));
                QCOMPARE(row["x"].toDouble(), 8.0);
                QCOMPARE(row["width"].toDouble(), width - 16.0);
                QCOMPARE(row["border"].toArray(),
                         (QJsonArray{"1px", "dotted", "rgb(0, 0, 0)"}));
                QCOMPARE(row["size"].toString(), "14px");
                QCOMPARE(row["weight"].toString(), "700");
                QCOMPARE(row["line"].toString(), "normal");
                QCOMPARE(row["clear"].toArray(),
                         (QJsonArray{"\"\"", "block", "0px", "both"}));
            }
            if (collapse) {
                const auto row = rows[0].toObject();
                QCOMPARE(row["marker"].toString(), "outside");
                const auto title = row["title"].toArray();
                QCOMPARE(title[4].toString(), "none");
                QCOMPARE(title[5].toString(), "0px");
                QCOMPARE(title[6].toString(), "14px");
                // The outside marker was already clipped before this change.
                // Preserve its measured off-box hit region, without a fake
                // click.
                for (int i = 0; i < 4; ++i)
                    QCOMPARE(
                        row["hits"].toArray()[i].toObject()["tag"].toString(),
                        "HTML");
                const QPoint padding(10, int(row["y"].toDouble() + 2));
                QTest::mouseClick(web->focusProxy(), Qt::LeftButton,
                                  Qt::NoModifier, padding);
                QTRY_VERIFY(Evaluate(view.page(),
                                     "document.querySelector('details').open")
                                .toBool());
                if (!capture.isEmpty()) {
                    const QString stem =
                        capture + QString("/reopened-%1").arg(width);
                    QVERIFY(web->grab().save(stem + ".png"));
                    QVERIFY(Write(stem + ".json",
                                  QJsonDocument(measure()).toJson()));
                }
                QTest::mouseClick(web->focusProxy(), Qt::LeftButton,
                                  Qt::NoModifier, padding);
                QTRY_VERIFY(!Evaluate(view.page(),
                                      "document.querySelector('details').open")
                                 .toBool());
                const QPoint text(
                    int(title[0].toDouble() + 2),
                    int(title[1].toDouble() + title[3].toDouble() / 2));
                QTest::mouseClick(web->focusProxy(), Qt::LeftButton,
                                  Qt::NoModifier, text);
                QTRY_VERIFY(Evaluate(view.page(),
                                     "document.querySelector('details').open")
                                .toBool());
                QTest::mouseClick(web->focusProxy(), Qt::LeftButton,
                                  Qt::NoModifier, text);
                QTRY_VERIFY(!Evaluate(view.page(),
                                      "document.querySelector('details').open")
                                 .toBool());
            }
            for (int index : {0, 1, 2}) {
                view.NavigateToResult(index);
                view.findChild<QAction*>("selectCurrentArticle")->trigger();
                const QString expected = QString::fromStdString(
                    response.entries[index].dictionary.name);
                QTRY_VERIFY(
                    Evaluate(view.page(), "window.getSelection().toString()")
                        .toString()
                        .contains(expected));
            }
            Evaluate(
                view.page(),
                "window.getSelection().removeAllRanges();window.scrollTo(0,0)");
        }
        // A payload heading is outside the trusted shell selectors.
        Evaluate(view.page(),
                 "const "
                 "h=document.createElement('h2');h.id='payload-heading';h."
                 "textContent='Body "
                 "heading';document.querySelector('.gd-entry-body').append(h)");
        QVERIFY(Evaluate(view.page(),
                         "getComputedStyle(document.getElementById('payload-"
                         "heading')).borderTopStyle === 'none'")
                    .toBool());
        Evaluate(
            view.page(),
            "const first=document.querySelector('body>section');const "
            "f=document.createElement('div');f.id='float-probe';f.style."
            "cssText='float:left;width:10px;height:200px';first.append(f)");
        QVERIFY(Evaluate(view.page(),
                         "document.querySelector('body>section')."
                         "getBoundingClientRect().bottom >= "
                         "document.getElementById('float-probe')."
                         "getBoundingClientRect().bottom")
                    .toBool());
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
    ArticleBaseStyleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "article_base_style_test.moc"
