/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qminimaleglbackingstore.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLPaintDevice>

QT_BEGIN_NAMESPACE

QMinimalEglBackingStore::QMinimalEglBackingStore(QWindow *window)
    : QPlatformBackingStore(window)
    , m_context(new QOpenGLContext)
    , m_device(0)
{
    m_context->setFormat(window->requestedFormat());
    m_context->setScreen(window->screen());
    m_context->create();
}

QMinimalEglBackingStore::~QMinimalEglBackingStore()
{
    delete m_context;
}

QPaintDevice *QMinimalEglBackingStore::paintDevice()
{
    return m_device;
}

void QMinimalEglBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(region);
    Q_UNUSED(offset);

#ifdef QEGL_EXTRA_DEBUG
    qWarning("QEglBackingStore::flush %p", window);
#endif

    m_context->swapBuffers(window);
}

void QMinimalEglBackingStore::beginPaint(const QRegion &)
{
    m_context->makeCurrent(window());
    m_device = new QOpenGLPaintDevice(window()->size());
}

void QMinimalEglBackingStore::endPaint()
{
    delete m_device;
}

void QMinimalEglBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(size);
    Q_UNUSED(staticContents);
}

QT_END_NAMESPACE
