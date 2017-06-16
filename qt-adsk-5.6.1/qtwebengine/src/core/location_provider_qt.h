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

#ifndef LOCATION_PROVIDER_QT_H
#define LOCATION_PROVIDER_QT_H

#include <QtCore/qcompilerdetection.h>

#include "content/browser/geolocation/location_provider_base.h"
#include "content/public/common/geoposition.h"

QT_FORWARD_DECLARE_CLASS(QThread)

namespace base {
class MessageLoop;
}

namespace QtWebEngineCore {
class QtPositioningHelper;

class LocationProviderQt : public content::LocationProviderBase
{
public:
    LocationProviderQt();
    virtual ~LocationProviderQt();

    // LocationProviderBase
    virtual bool StartProvider(bool highAccuracy) Q_DECL_OVERRIDE;
    virtual void StopProvider() Q_DECL_OVERRIDE;
    virtual void GetPosition(content::Geoposition *position) Q_DECL_OVERRIDE { *position = m_lastKnownPosition; }
    virtual void RequestRefresh() Q_DECL_OVERRIDE;
    virtual void OnPermissionGranted() Q_DECL_OVERRIDE;

private:
    friend class QtPositioningHelper;

    void updatePosition(const content::Geoposition &);

    content::Geoposition m_lastKnownPosition;
    QtPositioningHelper *m_positioningHelper;
};
//#define QT_USE_POSITIONING 1

} // namespace QtWebEngineCore

#endif // LOCATION_PROVIDER_QT_H
