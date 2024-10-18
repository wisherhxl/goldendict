#include "mainwindow.h"
#include "ui_mainwindow.h"

// #include "proto/config.pb.h"

#include <QDir>
#include <QThread>


#include "tiger/base.hpp"
#include "common/constants.h"

MainWindow::MainWindow(QSplashScreen* splash, QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    splash_(splash),
    settings_(ti::settings::kFileName, ti::settings::kSettingType)
{
    ui->setupUi(this);
    QThread::msleep(2000);
    splash->close();

    /*ti::CameraInfo cam;
	cam.set_device_("test");
	cam.set_manufacturer_("test manufacturer");
	cam.set_exposure_(10);
	cam.set_sn_("0x0123456789x9");
	cam.set_type_(ti::DAHUA);
	const auto result = ti::ProtobufUtil::writeMessageToJson(cam, QDir::currentPath().append("/test/cam_test.json"));
    */
}

MainWindow::~MainWindow()
{
    delete ui;
}
