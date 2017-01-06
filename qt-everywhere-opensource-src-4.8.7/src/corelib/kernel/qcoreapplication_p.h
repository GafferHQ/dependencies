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

#ifndef QCOREAPPLICATION_P_H
#define QCOREAPPLICATION_P_H

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

#include "QtCore/qcoreapplication.h"
#include "QtCore/qtranslator.h"
#include "QtCore/qsettings.h"
#include "private/qobject_p.h"

#ifdef Q_OS_SYMBIAN
#include <f32file.h>
#endif

QT_BEGIN_NAMESPACE

typedef QList<QTranslator*> QTranslatorList;

#if defined(Q_OS_SYMBIAN)
#  if !defined(QT_NO_SYSTEMLOCALE)
class QEnvironmentChangeNotifier;
#  endif
class CApaCommandLine;
#endif
class QAbstractEventDispatcher;

class Q_CORE_EXPORT QCoreApplicationPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QCoreApplication)

public:
    QCoreApplicationPrivate(int &aargc,  char **aargv, uint flags);
    ~QCoreApplicationPrivate();

    bool sendThroughApplicationEventFilters(QObject *, QEvent *);
    bool sendThroughObjectEventFilters(QObject *, QEvent *);
    bool notify_helper(QObject *, QEvent *);

    virtual QString appName() const;
    mutable QString applicationName;

    virtual void createEventDispatcher();
    static void removePostedEvent(QEvent *);
#ifdef Q_OS_WIN
    static void removePostedTimerEvent(QObject *object, int timerId);
#endif

#ifdef Q_OS_MAC
    static QString macMenuBarName();
#endif

    static QThread *theMainThread;
    static QThread *mainThread();
    static bool checkInstance(const char *method);
    static void sendPostedEvents(QObject *receiver, int event_type, QThreadData *data);

#if !defined (QT_NO_DEBUG) || defined (QT_MAC_FRAMEWORK_BUILD) || defined (Q_OS_SYMBIAN)
    void checkReceiverThread(QObject *receiver);
#endif
    int &argc;
    char **argv;
    void appendApplicationPathToLibraryPaths(void);
    void processCommandLineArguments();

    static QString qmljsDebugArguments(); // access arguments from other libraries

#ifndef QT_NO_TRANSLATION
    QTranslatorList translators;
#endif
    uint application_type;

    QCoreApplication::EventFilter eventFilter;

    bool in_exec;
    bool aboutToQuitEmitted;
    QString cachedApplicationDirPath;
    QString cachedApplicationFilePath;
#if defined(Q_OS_SYMBIAN)
#  if !defined(QT_NO_SYSTEMLOCALE)
    QScopedPointer<QEnvironmentChangeNotifier> environmentChangeNotifier;
    void symbianInit();
#  endif
    static CApaCommandLine* symbianCommandLine();
#ifndef QT_NO_LIBRARY
    static void rebuildInstallLibraryPaths();
#endif
#endif

    static bool isTranslatorInstalled(QTranslator *translator);

    static QAbstractEventDispatcher *eventDispatcher;
    static bool is_app_running;
    static bool is_app_closing;

    static uint attribs;
    static inline bool testAttribute(uint flag) { return attribs & (1 << flag); }
    static int app_compile_version;
#if defined(QT3_SUPPORT)
    static bool useQt3Support;
#endif
#ifndef QT_NO_SETTINGS
    static QSettings *trolltechConf();
#endif
};

QT_END_NAMESPACE

#endif // QCOREAPPLICATION_P_H
