/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdesktopwidget.h"
#include "private/qapplication_p.h"
#include "private/qgraphicssystem_p.h"
#include <QWidget>
#include "private/qwidget_p.h"
#include "private/qdesktopwidget_qpa_p.h"
QT_BEGIN_NAMESPACE

QT_USE_NAMESPACE

void QDesktopWidgetPrivate::updateScreenList()
{
    Q_Q(QDesktopWidget);

    QList<QPlatformScreen *> screenList = QApplicationPrivate::platformIntegration()->screens();
    int targetLength = screenList.length();
    int currentLength = screens.length();

    // Add or remove screen widgets as necessary
    if(currentLength > targetLength) {
        QDesktopScreenWidget *screen;
        while (currentLength-- > targetLength) {
            screen = screens.takeLast();
            delete screen;
        }
    }
    else if (currentLength < targetLength) {
        QDesktopScreenWidget *screen;
        while (currentLength < targetLength) {
            screen = new QDesktopScreenWidget(currentLength++);
            screens.append(screen);
        }
    }

    QRegion virtualGeometry;

    // update the geometry of each screen widget
    for (int i = 0; i < screens.length(); i++) {
        QRect screenGeometry = screenList.at(i)->geometry();
        screens.at(i)->setGeometry(screenGeometry);
        virtualGeometry += screenGeometry;
    }

    q->setGeometry(virtualGeometry.boundingRect());
}

QDesktopWidget::QDesktopWidget()
    : QWidget(*new QDesktopWidgetPrivate, 0, Qt::Desktop)
{
    Q_D(QDesktopWidget);
    setObjectName(QLatin1String("desktop"));
    d->updateScreenList();
}

QDesktopWidget::~QDesktopWidget()
{
}

bool QDesktopWidget::isVirtualDesktop() const
{
    return QApplicationPrivate::platformIntegration()->isVirtualDesktop();
}

int QDesktopWidget::primaryScreen() const
{
    return 0;
}

int QDesktopWidget::numScreens() const
{
    QPlatformIntegration *pi = QApplicationPrivate::platformIntegration();
    return qMax(pi->screens().size(), 1);
}

QWidget *QDesktopWidget::screen(int screen)
{
    Q_D(QDesktopWidget);
    if (screen < 0 || screen >= d->screens.length())
        return d->screens.at(0);
    return d->screens.at(screen);
}

const QRect QDesktopWidget::availableGeometry(int screenNo) const
{
    QPlatformIntegration *pi = QApplicationPrivate::platformIntegration();
    QList<QPlatformScreen *> screens = pi->screens();
    if (screenNo == -1)
        screenNo = 0;
    if (screenNo < 0 || screenNo >= screens.size())
        return QRect();
    else
        return screens[screenNo]->availableGeometry();
}

const QRect QDesktopWidget::screenGeometry(int screenNo) const
{
    QPlatformIntegration *pi = QApplicationPrivate::platformIntegration();
    QList<QPlatformScreen *> screens = pi->screens();
    if (screenNo == -1)
        screenNo = 0;
    if (screenNo < 0 || screenNo >= screens.size())
        return QRect();
    else
        return screens[screenNo]->geometry();
}

int QDesktopWidget::screenNumber(const QWidget *w) const
{
    if (!w)
        return 0;

    QRect frame = w->frameGeometry();
    if (!w->isWindow())
        frame.moveTopLeft(w->mapToGlobal(QPoint(0, 0)));
    const QPoint midpoint = (frame.topLeft() + frame.bottomRight()) / 2;
    return screenNumber(midpoint);
}

int QDesktopWidget::screenNumber(const QPoint &p) const
{
    QPlatformIntegration *pi = QApplicationPrivate::platformIntegration();
    QList<QPlatformScreen *> screens = pi->screens();

    for (int i = 0; i < screens.size(); ++i)
        if (screens[i]->geometry().contains(p))
            return i;

    return primaryScreen(); //even better would be closest screen
}

void QDesktopWidget::resizeEvent(QResizeEvent *)
{
}

QT_END_NAMESPACE
