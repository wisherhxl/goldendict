#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Rago Qt Example");
    window.resize(360, 180);

    auto* layout = new QVBoxLayout(&window);
    auto* label = new QLabel("Qt example is running.", &window);
    auto* button = new QPushButton("Close", &window);

    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    layout->addWidget(button);

    QObject::connect(button, &QPushButton::clicked, &window, &QWidget::close);

    window.show();
    return app.exec();
}
