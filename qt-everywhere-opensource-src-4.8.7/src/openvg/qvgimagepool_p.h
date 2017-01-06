/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtOpenVG module of the Qt Toolkit.
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

#ifndef QVGIMAGEPOOL_P_H
#define QVGIMAGEPOOL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qvg.h"
#include <QtCore/qscopedpointer.h>

QT_BEGIN_NAMESPACE

class QVGPixmapData;
class QVGImagePoolPrivate;

class Q_OPENVG_EXPORT QVGImagePool
{
public:
    QVGImagePool();
    virtual ~QVGImagePool();

    static QVGImagePool *instance();

    // This function can be used from system-specific graphics system
    // plugins to alter the image allocation strategy.
    static void setImagePool(QVGImagePool *pool);

    // Create a new VGImage from the pool with the specified parameters
    // that is not associated with a pixmap.  The VGImage is returned to
    // the pool when releaseImage() is called.
    //
    // This function will call reclaimSpace() when vgCreateImage() fails.
    //
    // This function is typically called when allocating temporary
    // VGImage's for pixmap filters.  The "keepData" object will not
    // be reclaimed if reclaimSpace() needs to be called.
    virtual VGImage createTemporaryImage(VGImageFormat format,
                                         VGint width, VGint height,
                                         VGbitfield allowedQuality,
                                         QVGPixmapData *keepData = 0);

    // Create a new VGImage with the specified parameters and associate
    // it with "data".  The QVGPixmapData will be notified when the
    // VGImage needs to be reclaimed by the pool.
    //
    // This function will call reclaimSpace() when vgCreateImage() fails.
    virtual VGImage createImageForPixmap(VGImageFormat format,
                                         VGint width, VGint height,
                                         VGbitfield allowedQuality,
                                         QVGPixmapData *data);

    // Create a permanent VGImage with the specified parameters.
    // If there is insufficient space for the vgCreateImage call,
    // then this function will call reclaimSpace() and try again.
    //
    // The caller is responsible for calling vgDestroyImage()
    // when it no longer needs the VGImage, as the image is not
    // recorded in the image pool.
    //
    // This function is typically used for pattern brushes where
    // the OpenVG engine is responsible for managing the lifetime
    // of the VGImage, destroying it automatically when the brush
    // is no longer in use.
    virtual VGImage createPermanentImage(VGImageFormat format,
                                         VGint width, VGint height,
                                         VGbitfield allowedQuality);

    // Release a VGImage that is no longer required.
    virtual void releaseImage(QVGPixmapData *data, VGImage image);

    // Notify the pool that a QVGPixmapData object is using
    // an image again.  This allows the pool to move the image
    // within a least-recently-used list of QVGPixmapData objects.
    virtual void useImage(QVGPixmapData *data);

    // Notify the pool that the VGImage's associated with a
    // QVGPixmapData are being detached from the pool.  The caller
    // will become responsible for calling vgDestroyImage().
    virtual void detachImage(QVGPixmapData *data);

    // Reclaim space for an image allocation with the specified parameters.
    // Returns true if space was reclaimed, or false if there is no
    // further space that can be reclaimed.  The "data" parameter
    // indicates the pixmap that is trying to obtain space which should
    // not itself be reclaimed.
    virtual bool reclaimSpace(VGImageFormat format,
                              VGint width, VGint height,
                              QVGPixmapData *data);

    // Hibernate the image pool because the context is about to be
    // destroyed.  All VGImage's left in the pool should be released.
    virtual void hibernate();

protected:
    // Helper functions for managing the LRU list of QVGPixmapData objects.
    void moveToHeadOfLRU(QVGPixmapData *data);
    void removeFromLRU(QVGPixmapData *data);
    QVGPixmapData *pixmapLRU();

private:
    QScopedPointer<QVGImagePoolPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QVGImagePool)
    Q_DISABLE_COPY(QVGImagePool)
};

QT_END_NAMESPACE

#endif
