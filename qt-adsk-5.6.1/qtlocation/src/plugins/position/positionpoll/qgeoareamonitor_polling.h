/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#ifndef QGEOAREAMONITORPOLLING_H
#define QGEOAREAMONITORPOLLING_H

#include <QtPositioning/qgeoareamonitorsource.h>
#include <QtPositioning/qgeopositioninfosource.h>


/**
 *  QGeoAreaMonitorPolling
 *
 */

class QGeoAreaMonitorPollingPrivate;
class QGeoAreaMonitorPolling : public QGeoAreaMonitorSource
{
    Q_OBJECT
public :
    explicit QGeoAreaMonitorPolling(QObject *parent = 0);
    ~QGeoAreaMonitorPolling();

    void setPositionInfoSource(QGeoPositionInfoSource *source) Q_DECL_OVERRIDE;
    QGeoPositionInfoSource* positionInfoSource() const Q_DECL_OVERRIDE;

    Error error() const Q_DECL_OVERRIDE;

    bool startMonitoring(const QGeoAreaMonitorInfo &monitor) Q_DECL_OVERRIDE;
    bool requestUpdate(const QGeoAreaMonitorInfo &monitor,
                       const char *signal) Q_DECL_OVERRIDE;
    bool stopMonitoring(const QGeoAreaMonitorInfo &monitor) Q_DECL_OVERRIDE;

    QList<QGeoAreaMonitorInfo> activeMonitors() const Q_DECL_OVERRIDE;
    QList<QGeoAreaMonitorInfo> activeMonitors(const QGeoShape &region) const Q_DECL_OVERRIDE;

    QGeoAreaMonitorSource::AreaMonitorFeatures supportedAreaMonitorFeatures() const Q_DECL_OVERRIDE;

    inline bool isValid() { return positionInfoSource(); }

    bool signalsAreConnected;

private Q_SLOTS:
    void positionError(QGeoPositionInfoSource::Error error);
    void timeout(const QGeoAreaMonitorInfo &monitor);
    void processAreaEvent(const QGeoAreaMonitorInfo &minfo, const QGeoPositionInfo &pinfo, bool isEnteredEvent);

private:
    QGeoAreaMonitorPollingPrivate* d;
    QGeoAreaMonitorSource::Error lastError;

    void connectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE;
    void disconnectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE;

    int idForSignal(const char *signal);
};

#endif // QGEOAREAMONITORPOLLING_H
