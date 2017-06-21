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

#ifndef QANDROIDCAMERAFOCUSCONTROL_H
#define QANDROIDCAMERAFOCUSCONTROL_H

#include <qcamerafocuscontrol.h>

QT_BEGIN_NAMESPACE

class QAndroidCameraSession;

class QAndroidCameraFocusControl : public QCameraFocusControl
{
    Q_OBJECT
public:
    explicit QAndroidCameraFocusControl(QAndroidCameraSession *session);

    QCameraFocus::FocusModes focusMode() const Q_DECL_OVERRIDE;
    void setFocusMode(QCameraFocus::FocusModes mode) Q_DECL_OVERRIDE;
    bool isFocusModeSupported(QCameraFocus::FocusModes mode) const Q_DECL_OVERRIDE;
    QCameraFocus::FocusPointMode focusPointMode() const Q_DECL_OVERRIDE;
    void setFocusPointMode(QCameraFocus::FocusPointMode mode) Q_DECL_OVERRIDE;
    bool isFocusPointModeSupported(QCameraFocus::FocusPointMode mode) const Q_DECL_OVERRIDE;
    QPointF customFocusPoint() const Q_DECL_OVERRIDE;
    void setCustomFocusPoint(const QPointF &point) Q_DECL_OVERRIDE;
    QCameraFocusZoneList focusZones() const Q_DECL_OVERRIDE;

private Q_SLOTS:
    void onCameraOpened();
    void onViewportSizeChanged();
    void onCameraCaptureModeChanged();
    void onAutoFocusStarted();
    void onAutoFocusComplete(bool success);

private:
    inline void setFocusModeHelper(QCameraFocus::FocusModes mode)
    {
        if (m_focusMode != mode) {
            m_focusMode = mode;
            emit focusModeChanged(mode);
        }
    }

    inline void setFocusPointModeHelper(QCameraFocus::FocusPointMode mode)
    {
        if (m_focusPointMode != mode) {
            m_focusPointMode = mode;
            emit focusPointModeChanged(mode);
        }
    }

    void updateFocusZones(QCameraFocusZone::FocusZoneStatus status = QCameraFocusZone::Selected);
    void setCameraFocusArea();

    QAndroidCameraSession *m_session;

    QCameraFocus::FocusModes m_focusMode;
    QCameraFocus::FocusPointMode m_focusPointMode;
    QPointF m_actualFocusPoint;
    QPointF m_customFocusPoint;
    QCameraFocusZoneList m_focusZones;

    QList<QCameraFocus::FocusModes> m_supportedFocusModes;
    bool m_continuousPictureFocusSupported;
    bool m_continuousVideoFocusSupported;

    QList<QCameraFocus::FocusPointMode> m_supportedFocusPointModes;
};

QT_END_NAMESPACE

#endif // QANDROIDCAMERAFOCUSCONTROL_H
