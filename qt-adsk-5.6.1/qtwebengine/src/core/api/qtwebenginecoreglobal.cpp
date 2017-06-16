/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtwebenginecoreglobal_p.h"

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QThread>

QT_BEGIN_NAMESPACE
Q_GUI_EXPORT void qt_gl_set_global_share_context(QOpenGLContext *context);
Q_GUI_EXPORT QOpenGLContext *qt_gl_global_share_context();
QT_END_NAMESPACE

namespace QtWebEngineCore {

static QOpenGLContext *shareContext;

static void deleteShareContext()
{
    delete shareContext;
    shareContext = 0;
}

// ### Qt 6: unify this logic and Qt::AA_ShareOpenGLContexts.
// QtWebEngine::initialize was introduced first and meant to be called
// after the QGuiApplication creation, when AA_ShareOpenGLContexts fills
// the same need but the flag has to be set earlier.

QWEBENGINE_PRIVATE_EXPORT void initialize()
{
#ifdef Q_OS_WIN32
    qputenv("QT_D3DCREATE_MULTITHREADED", "1");
#endif

    // No need to override the shared context if QApplication already set one (e.g with Qt::AA_ShareOpenGLContexts).
    if (qt_gl_global_share_context())
        return;

    QCoreApplication *app = QCoreApplication::instance();
    if (!app) {
        qFatal("QtWebEngine::initialize() must be called after the construction of the application object.");
        return;
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 1))
    // Bail out silently if the user did not construct a QGuiApplication.
    if (!qobject_cast<QGuiApplication *>(app))
        return;
#endif

    if (app->thread() != QThread::currentThread()) {
        qFatal("QtWebEngine::initialize() must be called from the Qt gui thread.");
        return;
    }

    if (shareContext)
        return;

    shareContext = new QOpenGLContext;
    shareContext->create();
    qAddPostRoutine(deleteShareContext);
    qt_gl_set_global_share_context(shareContext);

    // Classes like QOpenGLWidget check for the attribute
    app->setAttribute(Qt::AA_ShareOpenGLContexts);
}
} // namespace QtWebEngineCore
