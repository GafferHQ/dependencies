/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Mobility Components.
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

import QtQuick 2.0

ListModel {
    id: sources
    ListElement { name: "No effect"; source: "EffectPassThrough.qml" }
    ListElement { name: "Billboard"; source: "EffectBillboard.qml" }
    ListElement { name: "Black & white"; source: "EffectBlackAndWhite.qml" }
    ListElement { name: "Blur"; source: "EffectGaussianBlur.qml" }
    ListElement { name: "Edge detection"; source: "EffectSobelEdgeDetection1.qml" }
    ListElement { name: "Emboss"; source: "EffectEmboss.qml" }
    ListElement { name: "Glow"; source: "EffectGlow.qml" }
    ListElement { name: "Isolate"; source: "EffectIsolate.qml" }
    ListElement { name: "Magnify"; source: "EffectMagnify.qml" }
    ListElement { name: "Page curl"; source: "EffectPageCurl.qml" }
    ListElement { name: "Pixelate"; source: "EffectPixelate.qml" }
    ListElement { name: "Posterize"; source: "EffectPosterize.qml" }
    ListElement { name: "Ripple"; source: "EffectRipple.qml" }
    ListElement { name: "Sepia"; source: "EffectSepia.qml" }
    ListElement { name: "Sharpen"; source: "EffectSharpen.qml" }
    ListElement { name: "Shockwave"; source: "EffectShockwave.qml" }
    ListElement { name: "Tilt shift"; source: "EffectTiltShift.qml" }
    ListElement { name: "Toon"; source: "EffectToon.qml" }
    ListElement { name: "Warhol"; source: "EffectWarhol.qml" }
    ListElement { name: "Wobble"; source: "EffectWobble.qml" }
    ListElement { name: "Vignette"; source: "EffectVignette.qml" }
}
