/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#ifndef VMR9VIDEOWINDOWCONTROL_H
#define VMR9VIDEOWINDOWCONTROL_H

#include "qvideowindowcontrol.h"

#include <dshow.h>
#include <d3d9.h>
#include <vmr9.h>

QT_USE_NAMESPACE

class Vmr9VideoWindowControl : public QVideoWindowControl
{
    Q_OBJECT
public:
    Vmr9VideoWindowControl(QObject *parent = 0);
    ~Vmr9VideoWindowControl();

    IBaseFilter *filter() const { return m_filter; }

    WId winId() const;
    void setWinId(WId id);

    QRect displayRect() const;
    void setDisplayRect(const QRect &rect);

    bool isFullScreen() const;
    void setFullScreen(bool fullScreen);

    void repaint();

    QSize nativeSize() const;

    Qt::AspectRatioMode aspectRatioMode() const;
    void setAspectRatioMode(Qt::AspectRatioMode mode);

    int brightness() const;
    void setBrightness(int brightness);

    int contrast() const;
    void setContrast(int contrast);

    int hue() const;
    void setHue(int hue);

    int saturation() const;
    void setSaturation(int saturation);

private:
    void setProcAmpValues();
    float scaleProcAmpValue(
            IVMRMixerControl9 *control, VMR9ProcAmpControlFlags property, int value) const;

    IBaseFilter *m_filter;
    WId m_windowId;
    COLORREF m_windowColor;
    DWORD m_dirtyValues;
    Qt::AspectRatioMode m_aspectRatioMode;
    QRect m_displayRect;
    int m_brightness;
    int m_contrast;
    int m_hue;
    int m_saturation;
    bool m_fullScreen;
};

#endif
