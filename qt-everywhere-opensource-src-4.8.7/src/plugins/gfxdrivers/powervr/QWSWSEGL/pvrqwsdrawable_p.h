/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef PVRQWSDRAWABLE_P_H
#define PVRQWSDRAWABLE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// reasons.  This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.
//

#include <pvr2d.h>
#include "pvrqwsdrawable.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PVRQWS_MAX_VISIBLE_RECTS    32
#define PVRQWS_MAX_SCREENS          1
#define PVRQWS_MAX_BACK_BUFFERS     2
#define PVRQWS_MAX_FLIP_BUFFERS     2

typedef struct {

    PvrQwsRect          screenRect;
    int                 screenStride;
    PVR2DFORMAT         pixelFormat;
    int                 bytesPerPixel;
    PVR2DMEMINFO       *frameBuffer;
    PvrQwsDrawable     *screenDrawable;
    void               *mapped;
    int                 mappedLength;
    unsigned long       screenStart;
    int                 needsUnmap;
    int                 initialized;

} PvrQwsScreenInfo;

typedef struct {

    int                 refCount;
    PvrQwsScreenInfo    screens[PVRQWS_MAX_SCREENS];
    PVR2DCONTEXTHANDLE  context;
    int                 numDrawables;
    unsigned long       numFlipBuffers;
    PVR2DFLIPCHAINHANDLE flipChain;
    PVR2DMEMINFO       *flipBuffers[PVRQWS_MAX_FLIP_BUFFERS];
    int                 usePresentBlit;
    PvrQwsDrawable     *firstWinId;

} PvrQwsDisplay;

extern PvrQwsDisplay pvrQwsDisplay;

struct _PvrQwsDrawable
{
    PvrQwsDrawableType  type;
    long                winId;
    int                 refCount;
    PvrQwsRect          rect;
    int                 screen;
    PVR2DFORMAT         pixelFormat;
    PvrQwsRect          visibleRects[PVRQWS_MAX_VISIBLE_RECTS];
    int                 numVisibleRects;
    PVR2DMEMINFO       *backBuffers[PVRQWS_MAX_BACK_BUFFERS];
    int                 currentBackBuffer;
    int                 backBuffersValid;
    int                 usingFlipBuffers;
    int                 isFullScreen;
    int                 strideBytes;
    int                 stridePixels;
    int                 rotationAngle;
    PvrQwsSwapFunction  swapFunction;
    void               *userData;
    PvrQwsDrawable     *nextWinId;

};

/* Get the current source and render buffers for a drawable */
int pvrQwsGetBuffers
    (PvrQwsDrawable *drawable, PVR2DMEMINFO **source, PVR2DMEMINFO **render);

#ifdef __cplusplus
};
#endif

#endif
