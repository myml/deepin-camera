/*
* Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     shicetu <shicetu@uniontech.com>
*
* Maintainer: shicetu <shicetu@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mainwindow.h"
//#include <DApplication>
#include "capplication.h"
#include <DMainWindow>
#include <DWidgetUtil>
#include <QSharedMemory>
#include <DLog>
#include <DApplicationSettings>

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    CApplication::loadDXcbPlugin();
    CApplication a(argc, argv);
    //加载翻译
    a.loadTranslator(QList<QLocale>() << QLocale::system());

    QTranslator *translator = new QTranslator;

    bool bLoaded = translator->load("deepin-camera.qm", ":/translations");
    if (!bLoaded) {
        qDebug() << "load transfile error";
    }

    a.installTranslator(translator);

    a.setAttribute(Qt::AA_UseHighDpiPixmaps);

    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();
    qDebug() << "LogFile:" << DLogManager::getlogFilePath();

    a.setOrganizationName("deepin");
    a.setApplicationName("deepin-camera");
    a.setApplicationDisplayName("Camera");
    a.setApplicationVersion("1.0");
    a.setWindowIcon(QIcon(":/images/logo/deepin-camera-96px.svg"));
    //a.setProductIcon(QIcon::fromTheme("deepin-camera"));
    a.setProductIcon(QIcon(":/images/logo/deepin-camera-96px.svg")); //用于显示关于窗口的应用图标
    a.setProductName("Camera");
    a.setApplicationDescription("This is camera.");

    DApplicationSettings saveTheme;

    //仅允许打开一个相机
    QSharedMemory shared_memory("deepincamera");

    if (shared_memory.attach()) {
        shared_memory.detach();
    }

    if (!shared_memory.create(1)) {
        qDebug() << "another deepin camera instance has started";
        exit(0);
    }

    CMainWindow w;

    w.setMinimumSize(MinWindowWidth, MinWindowHeight);
    w.show();

    Dtk::Widget::moveToCenter(&w);

    return a.exec();
}
