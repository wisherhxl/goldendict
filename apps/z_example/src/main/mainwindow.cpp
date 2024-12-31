#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "proto/base.pb.h"

#include <QDir>
#include <QThread>

#include "common/constants.h"
#include "tiger/base.hpp"

MainWindow::MainWindow(QSplashScreen* splash, QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      splash_(splash),
      settings_(ti::settings::kFileName, ti::settings::kSettingType) {
    ui->setupUi(this);
    splash->close();
}

MainWindow::~MainWindow() {
    delete ui;
}
