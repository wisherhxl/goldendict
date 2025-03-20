/*
 * Copyright (c) 2021 Huang Xiling
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the Tiger Template.
 * Distributed under the MIT License. See LICENSE file for details.
 *
 * Created Date: 2021-03-19
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QSplashScreen>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(QSplashScreen* splash, QWidget* parent = nullptr);
    ~MainWindow();

   private:
    Ui::MainWindow* ui;
    QSplashScreen* splash_;
    QSettings settings_;
};

#endif  // MAINWINDOW_H
