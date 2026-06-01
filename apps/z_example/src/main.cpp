#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWidget>

#include "tiger/test.pb.h"
#include "tiger/ti_base.tp.h"

#include <QString>

namespace {

tiger::TestRequest makeRequest()
{
    tiger::TestRequest request;
    request.set_id("z_example");
    request.set_name("Tiger Qt Example");
    request.add_tags("module");
    request.add_tags("proto");
    return request;
}

bool hasSmokeArgument(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--smoke")) {
            return true;
        }
    }
    return false;
}

int runSmoke()
{
    const tiger::TestRequest request = makeRequest();
    if (request.GetDescriptor() == nullptr || request.GetReflection() == nullptr) {
        return 1;
    }
    return request.ShortDebugString().empty() ? 1 : 0;
}

} // namespace

int main(int argc, char* argv[])
{
    if (hasSmokeArgument(argc, argv)) {
        return runSmoke();
    }

    QApplication app(argc, argv);

    QTranslator translator;
    const QString translationDir = QCoreApplication::applicationDirPath() + QDir::separator() + "lang";
    if (translator.load("zh_cn", translationDir)) {
        app.installTranslator(&translator);
    }

    const tiger::TestRequest request = makeRequest();

    const int wrappedTagIndex = tiWrap(request.tags_size(), 0, 9);

    QWidget window;
    window.setWindowTitle(QApplication::translate("z_example", "Tiger Qt Example"));
    window.resize(360, 180);

    auto* layout = new QVBoxLayout(&window);
    auto* label = new QLabel(
        QApplication::translate("z_example", "Qt example is running with %1 and proto message %2.")
            .arg(TI_PROJECT_NAME)
            .arg(QString::fromStdString(request.ShortDebugString())),
        &window);
    auto* button = new QPushButton(&window);

    button->setText(QApplication::translate("z_example", "Close (%1)").arg(wrappedTagIndex));

    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    layout->addWidget(button);

    QObject::connect(button, &QPushButton::clicked, &window, &QWidget::close);

    window.show();
    return app.exec();
}
