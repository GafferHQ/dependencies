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

#include "qdirectfbbackingstore.h"
#include "qdirectfbintegration.h"
#include "qdirectfbblitter.h"
#include "qdirectfbconvenience.h"
#include "qdirectfbwindow.h"
#include <private/qpixmap_blitter_p.h>

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

QDirectFbBackingStore::QDirectFbBackingStore(QWindow *window)
    : QPlatformBackingStore(window), m_pixmap(0), m_pmdata(0)
{
    IDirectFBWindow *dfbWindow = static_cast<QDirectFbWindow *>(window->handle())->dfbWindow();
    dfbWindow->GetSurface(dfbWindow, m_dfbSurface.outPtr());

//WRONGSIZE
    QDirectFbBlitter *blitter = new QDirectFbBlitter(window->size(), m_dfbSurface.data());
    m_pmdata = new QDirectFbBlitterPlatformPixmap;
    m_pmdata->setBlittable(blitter);
    m_pixmap.reset(new QPixmap(m_pmdata));
}

QPaintDevice *QDirectFbBackingStore::paintDevice()
{
    return m_pixmap.data();
}

void QDirectFbBackingStore::flush(QWindow *, const QRegion &region, const QPoint &offset)
{
    m_pmdata->blittable()->unlock();

    QVector<QRect> rects = region.rects();
    for (int i = 0 ; i < rects.size(); i++) {
        const QRect rect = rects.at(i);
        DFBRegion dfbReg(rect.x() + offset.x(),rect.y() + offset.y(),rect.right() + offset.x(),rect.bottom() + offset.y());
        m_dfbSurface->Flip(m_dfbSurface.data(), &dfbReg, DFBSurfaceFlipFlags(DSFLIP_BLIT|DSFLIP_ONSYNC));
    }
}

void QDirectFbBackingStore::resize(const QSize &size, const QRegion& reg)
{
    Q_UNUSED(reg);

    if ((m_pmdata->width() == size.width()) &&
        (m_pmdata->height() == size.height()))
        return;

    QDirectFbBlitter *blitter = new QDirectFbBlitter(size, m_dfbSurface.data());
    m_pmdata->setBlittable(blitter);
}

static inline void scrollSurface(IDirectFBSurface *surface, const QRect &r, int dx, int dy)
{
    const DFBRectangle rect(r.x(), r.y(), r.width(), r.height());
    surface->Blit(surface, surface, &rect, r.x() + dx, r.y() + dy);
    const DFBRegion region(rect.x + dx, rect.y + dy, r.right() + dx, r.bottom() + dy);
    surface->Flip(surface, &region, DFBSurfaceFlipFlags(DSFLIP_BLIT));
}

bool QDirectFbBackingStore::scroll(const QRegion &area, int dx, int dy)
{
    m_pmdata->blittable()->unlock();

    if (!m_dfbSurface || area.isEmpty())
        return false;
    m_dfbSurface->SetBlittingFlags(m_dfbSurface.data(), DSBLIT_NOFX);
    if (area.rectCount() == 1) {
        scrollSurface(m_dfbSurface.data(), area.boundingRect(), dx, dy);
    } else {
        const QVector<QRect> rects = area.rects();
        const int n = rects.size();
        for (int i=0; i<n; ++i) {
            scrollSurface(m_dfbSurface.data(), rects.at(i), dx, dy);
        }
    }
    return true;
}

QT_END_NAMESPACE
