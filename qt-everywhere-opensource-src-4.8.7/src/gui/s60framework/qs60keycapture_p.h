/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Symbian application wrapper of the Qt Toolkit.
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

#ifndef QS60KEYCAPTURE_P_H
#define QS60KEYCAPTURE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>

#ifdef Q_OS_SYMBIAN

#include <QObject>
#include <QEvent>
#include <QTimer>
#include <remconcoreapitargetobserver.h>
#include <w32std.h>

class CRemConInterfaceSelector;
class CRemConCoreApiTarget;
class CResponseHandler;
class CCoeEnv;

QT_BEGIN_NAMESPACE

class QS60KeyCapture: public QObject,
                      public MRemConCoreApiTargetObserver
{
    Q_OBJECT
public:
    QS60KeyCapture(CCoeEnv *env, QObject *parent = 0);
    ~QS60KeyCapture();

    void MrccatoCommand(TRemConCoreApiOperationId operationId,
                        TRemConCoreApiButtonAction buttonAct);

private slots:
    void volumeUpClickTimerExpired();
    void volumeDownClickTimerExpired();
    void repeatTimerExpired();

private:
    void sendKey(TRemConCoreApiOperationId operationId, QEvent::Type type);
    TKeyEvent mapToKeyEvent(TRemConCoreApiOperationId operationId);
    void initRemCon();

private:
    QS60KeyCapture();
    Q_DISABLE_COPY(QS60KeyCapture)

    CCoeEnv *coeEnv;

    CRemConInterfaceSelector *selector;
    CRemConCoreApiTarget *target;
    CResponseHandler *handler;

    QTimer volumeUpClickTimer;
    QTimer volumeDownClickTimer;

    TKeyEvent repeatKeyEvent;
    int initialRepeatTime; // time before first repeat key event
    int repeatTime; // time between subsequent repeat key events
    QTimer repeatTimer;
};

QT_END_NAMESPACE

#endif // Q_OS_SYMBIAN
#endif // QS60KEYCAPTURE_P_H
