/*
* Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     wuzhigang <wuzhigang@uniontech.com>
* Maintainer: wuzhigang <wuzhigang@uniontech.com>
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

#ifndef WINDOWSTATETHREAD_H
#define WINDOWSTATETHREAD_H

#include <DMainWindow>
#include <DForeignWindow>

#include <QThread>
#include <QMutex>

class windowStateThread : public QThread
{
    Q_OBJECT

public:
    explicit windowStateThread(QObject *parent = nullptr);
    QList<DForeignWindow *> workspaceWindows() const;
    void run();

signals:
    void someWindowFullScreen();

private:

};

#endif  // WINDOWSTATETHREAD_H
