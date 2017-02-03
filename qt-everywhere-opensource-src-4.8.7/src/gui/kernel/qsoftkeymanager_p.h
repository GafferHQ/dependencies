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

#ifndef QSOFTKEYMANAGER_P_H
#define QSOFTKEYMANAGER_P_H

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

#include <QtCore/qobject.h>
#include "QtGui/qaction.h"

QT_BEGIN_HEADER

#ifndef QT_NO_SOFTKEYMANAGER
QT_BEGIN_NAMESPACE

class QSoftKeyManagerPrivate;

class Q_AUTOTEST_EXPORT QSoftKeyManager : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QSoftKeyManager)

public:

    enum StandardSoftKey {
        OkSoftKey,
        SelectSoftKey,
        DoneSoftKey,
        MenuSoftKey,
        CancelSoftKey
    };

    static void updateSoftKeys();
#ifdef Q_WS_S60
    static bool handleCommand(int);
#endif

    static QAction *createAction(StandardSoftKey standardKey, QWidget *actionWidget);
    static QAction *createKeyedAction(StandardSoftKey standardKey, Qt::Key key, QWidget *actionWidget);
    static QString standardSoftKeyText(StandardSoftKey standardKey);
    static void setForceEnabledInSoftkeys(QAction *action);
    static bool isForceEnabledInSofkeys(QAction *action);

protected:
    bool event(QEvent *e);

private:
    QSoftKeyManager();
    static QSoftKeyManager *instance();
    bool appendSoftkeys(const QWidget &source, int level);
    QWidget *softkeySource(QWidget *previousSource, bool& recursiveMerging);
    bool handleUpdateSoftKeys();

private Q_SLOTS:
    void cleanupHash(QObject* obj);
    void sendKeyEvent();

private:
    Q_DISABLE_COPY(QSoftKeyManager)
};

QT_END_NAMESPACE
#endif //QT_NO_SOFTKEYMANAGER

QT_END_HEADER

#endif //QSOFTKEYMANAGER_P_H
