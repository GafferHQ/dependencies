/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

/****************************************************************************
**
** Implementation of QMultiInputContext class
**
** Copyright (C) 2004 immodule for Qt Project.  All rights reserved.
**
** This file is written to contribute to Nokia Corporation and/or its subsidiary(-ies) under their own
** license. You may use this file under your Qt license. Following
** description is copied from their original file headers. Contact
** immodule-qt@freedesktop.org if any conditions of this licensing are
** not clear to you.
**
****************************************************************************/

#ifndef QT_NO_IM
#include "qmultiinputcontext.h"
#include <qinputcontextfactory.h>
#include <qstringlist.h>
#include <qaction.h>
#include <qsettings.h>
#include <qmenu.h>

#include <stdlib.h>

QT_BEGIN_NAMESPACE

QMultiInputContext::QMultiInputContext()
    : QInputContext(), current(-1)
{
    keys = QInputContextFactory::keys();
    for (int i = keys.size()-1; i >= 0; --i)
        if (keys.at(i).contains(QLatin1String("imsw")))
            keys.removeAt(i);

    QString def = QLatin1String(getenv("QT4_IM_MODULE"));
    if (def.isEmpty())
        def = QLatin1String(getenv("QT_IM_MODULE"));
    if (def.isEmpty()) {
        QSettings settings(QSettings::UserScope, QLatin1String("Trolltech"));
        settings.beginGroup(QLatin1String("Qt"));
        def = settings.value(QLatin1String("DefaultInputMethod"), QLatin1String("xim")).toString();
    }
    current = keys.indexOf(def);
    if (current < 0)
        current = 0;

    menu = new QMenu(tr("Select IM"));
    separator = new QAction(this);
    separator->setSeparator(true);

    QActionGroup *group = new QActionGroup(this);
    for (int i = 0; i < keys.size(); ++i) {
        slaves.append(0);
        const QString key = keys.at(i);
        QAction *a = menu->addAction(QInputContextFactory::displayName(key));
        a->setData(key);
        a->setCheckable(true);
        group->addAction(a);
        if (i == current) {
            slaves.replace(current, QInputContextFactory::create(key, this));
            a->setChecked(true);
        }
    }
    connect(group, SIGNAL(triggered(QAction*)), this, SLOT(changeSlave(QAction*)));
}

QMultiInputContext::~QMultiInputContext()
{
    delete menu;
}


QString QMultiInputContext::identifierName()
{
    return (slave()) ? slave()->identifierName() : QLatin1String("");
}

QString QMultiInputContext::language()
{
    return (slave()) ? slave()->language() : QLatin1String("");
}


#if defined(Q_WS_X11)
bool QMultiInputContext::x11FilterEvent(QWidget *keywidget, XEvent *event)
{
    return (slave()) ? slave()->x11FilterEvent(keywidget, event) : false;
}
#endif // Q_WS_X11


bool QMultiInputContext::filterEvent(const QEvent *event)
{
    return (slave()) ? slave()->filterEvent(event) : false;
}

void QMultiInputContext::reset()
{
    if (slave())
	slave()->reset();
}

void QMultiInputContext::update()
{
    if (slave())
	slave()->update();
}

void QMultiInputContext::mouseHandler(int x, QMouseEvent *event)
{
    if (slave())
	slave()->mouseHandler(x, event);
}

QFont QMultiInputContext::font() const
{
    return (slave()) ? slave()->font() : QInputContext::font();
}

void QMultiInputContext::setFocusWidget(QWidget *w)
{
    QInputContext::setFocusWidget(w);
    if (slave())
	slave()->setFocusWidget(w);
}

QWidget *QMultiInputContext::focusWidget() const
{
    return QInputContext::focusWidget();
}

void QMultiInputContext::widgetDestroyed(QWidget *w)
{
    if (slave())
	slave()->widgetDestroyed(w);
}

bool QMultiInputContext::isComposing() const
{
    return (slave()) ? slave()->isComposing() : false;
}

QList<QAction *> QMultiInputContext::actions()
{
    QList<QAction *> a = slave()->actions();
    a.append(separator);
    a.append(menu->menuAction());
    return a;
}

void QMultiInputContext::changeSlave(QAction *a)
{
    for (int i = 0; i < slaves.size(); ++i) {
        if (keys.at(i) == a->data().toString()) {
            if (slaves.at(i) == 0)
                slaves.replace(i, QInputContextFactory::create(keys.at(i), this));
            QInputContext *qic = slaves.at(current);
            QWidget *oldWidget = qic->focusWidget();
            qic->reset();
            qic->setFocusWidget(0);
            current = i;
            qic = slaves.at(current);
            qic->setFocusWidget(oldWidget);
            return;
        }
    }
}

QT_END_NAMESPACE

#endif // QT_NO_IM
