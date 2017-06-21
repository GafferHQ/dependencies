/***************************************************************************
**
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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

#ifndef QQNXRASTERWINDOW_H
#define QQNXRASTERWINDOW_H

#include "qqnxwindow.h"
#include "qqnxbuffer.h"

QT_BEGIN_NAMESPACE

class QQnxRasterWindow : public QQnxWindow
{
public:
    QQnxRasterWindow(QWindow *window, screen_context_t context, bool needRootWindow);

    void post(const QRegion &dirty);

    void scroll(const QRegion &region, int dx, int dy, bool flush=false);

    QQnxBuffer &renderBuffer();

    bool hasBuffers() const { return !bufferSize().isEmpty(); }

    void setParent(const QPlatformWindow *window);

    void adjustBufferSize();

protected:
    int pixelFormat() const;
    void resetBuffers();

    // Copies content from the previous buffer (back buffer) to the current buffer (front buffer)
    void blitPreviousToCurrent(const QRegion &region, int dx, int dy, bool flush=false);

    void blitHelper(QQnxBuffer &source, QQnxBuffer &target, const QPoint &sourceOffset,
                    const QPoint &targetOffset, const QRegion &region, bool flush = false);

private:
    QRegion m_previousDirty;
    QRegion m_scrolled;
    int m_currentBufferIndex;
    int m_previousBufferIndex;
    QQnxBuffer m_buffers[MAX_BUFFER_COUNT];
};

QT_END_NAMESPACE

#endif // QQNXRASTERWINDOW_H
