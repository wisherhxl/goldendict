// SPDX-License-Identifier: GPL-3.0-or-later
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
     lineCount:new Set(lines.map(r=>r.y)).size,lines};
 };
 const article=document.querySelector('.sdct_h');
 return JSON.stringify({body:measure(document.body),plain:measure(article.querySelector('p')),
   token:measure(article.querySelector('div')),pre:measure(article.querySelector('pre')),
   dictd:measure(document.querySelector('.dictd_article')),phonetic:measure(document.querySelector('.dictd_phonetic')),
   pseudo:getComputedStyle(document.querySelector('.dictd_phonetic'),'::before').content,
   links:Array.from(document.querySelectorAll('.dictd_article a')).map(e=>({href:e.getAttribute('href'),color:getComputedStyle(e).color,decoration:getComputedStyle(e).textDecorationLine})),
   dpr:devicePixelRatio,viewport:innerWidth,documentWidth:document.documentElement.scrollWidth,
   dark:matchMedia('(prefers-color-scheme:dark)').matches,html:document.documentElement.outerHTML});
})()
)JS";
}  // namespace

class ArticleBaseStyleTest : public QObject {
    Q_OBJECT
   private slots:
    void PublishesColdAndWarmBaseStyle();
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
