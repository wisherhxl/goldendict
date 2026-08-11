// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QMainWindow>
#include <QString>
#include <QWebEngineView>

namespace {

bool HasSmokeArgument(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--smoke")) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (HasSmokeArgument(argc, argv)) {
        return 0;
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("GoldenDict"));
    QApplication::setOrganizationName(QStringLiteral("GoldenDict"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("GoldenDict"));
    window.resize(960, 640);

    auto* article_view = new QWebEngineView(&window);
    article_view->setHtml(
        QStringLiteral("<h1>GoldenDict</h1><p>Qt 6 migration shell</p>"));
    window.setCentralWidget(article_view);
    window.show();

    return app.exec();
}
