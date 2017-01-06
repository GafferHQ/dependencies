/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QEVENTTRANSITION_H
#define QEVENTTRANSITION_H

#include <QtCore/qabstracttransition.h>
#include <QtCore/qcoreevent.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Core)

#ifndef QT_NO_STATEMACHINE

class QEventTransitionPrivate;
class Q_CORE_EXPORT QEventTransition : public QAbstractTransition
{
    Q_OBJECT
    Q_PROPERTY(QObject* eventSource READ eventSource WRITE setEventSource)
    Q_PROPERTY(QEvent::Type eventType READ eventType WRITE setEventType)
public:
    QEventTransition(QState *sourceState = 0);
    QEventTransition(QObject *object, QEvent::Type type, QState *sourceState = 0);
    ~QEventTransition();

    QObject *eventSource() const;
    void setEventSource(QObject *object);

    QEvent::Type eventType() const;
    void setEventType(QEvent::Type type);

protected:
    bool eventTest(QEvent *event);
    void onTransition(QEvent *event);

    bool event(QEvent *e);

protected:
    QEventTransition(QEventTransitionPrivate &dd, QState *parent);
    QEventTransition(QEventTransitionPrivate &dd, QObject *object,
                     QEvent::Type type, QState *parent);

private:
    Q_DISABLE_COPY(QEventTransition)
    Q_DECLARE_PRIVATE(QEventTransition)
};

#endif //QT_NO_STATEMACHINE

QT_END_NAMESPACE

QT_END_HEADER

#endif
