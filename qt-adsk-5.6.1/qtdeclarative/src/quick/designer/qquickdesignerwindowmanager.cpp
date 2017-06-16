/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickdesignerwindowmanager_p.h"
#include "private/qquickwindow_p.h"
#include <QtGui/QOpenGLContext>

#include <QtQuick/QQuickWindow>

QT_BEGIN_NAMESPACE

QQuickDesignerWindowManager::QQuickDesignerWindowManager()
    : m_sgContext(QSGContext::createDefaultContext())
{
    m_renderContext.reset(new QSGRenderContext(m_sgContext.data()));
}

void QQuickDesignerWindowManager::show(QQuickWindow *window)
{
    makeOpenGLContext(window);
}

void QQuickDesignerWindowManager::hide(QQuickWindow *)
{
}

void QQuickDesignerWindowManager::windowDestroyed(QQuickWindow *)
{
}

void QQuickDesignerWindowManager::makeOpenGLContext(QQuickWindow *window)
{
    if (!m_openGlContext) {
        m_openGlContext.reset(new QOpenGLContext());
        m_openGlContext->setFormat(window->requestedFormat());
        m_openGlContext->create();
        if (!m_openGlContext->makeCurrent(window))
            qWarning("QQuickWindow: makeCurrent() failed...");
        m_renderContext->initialize(m_openGlContext.data());
    } else {
        m_openGlContext->makeCurrent(window);
    }
}

void QQuickDesignerWindowManager::exposureChanged(QQuickWindow *)
{
}

QImage QQuickDesignerWindowManager::grab(QQuickWindow *)
{
    return QImage();
}

void QQuickDesignerWindowManager::maybeUpdate(QQuickWindow *)
{
}

QSGContext *QQuickDesignerWindowManager::sceneGraphContext() const
{
    return m_sgContext.data();
}

void QQuickDesignerWindowManager::createOpenGLContext(QQuickWindow *window)
{
    window->create();
    window->update();
}

void QQuickDesignerWindowManager::update(QQuickWindow *window)
{
    makeOpenGLContext(window);
}

QT_END_NAMESPACE


