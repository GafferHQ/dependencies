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
***************************************************************************/

#ifndef QDECLARATIVEPOSITIONSOURCE_H
#define QDECLARATIVEPOSITIONSOURCE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qdeclarativeposition_p.h"

#include <QtCore/QObject>
#include <QtNetwork/QAbstractSocket>
#include <QtQml/QQmlParserStatus>
#include <QtPositioning/QGeoPositionInfoSource>

QT_BEGIN_NAMESPACE

class QFile;
class QTcpSocket;

class QDeclarativePositionSource : public QObject, public QQmlParserStatus
{
    Q_OBJECT

    Q_PROPERTY(QDeclarativePosition *position READ position NOTIFY positionChanged)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool valid READ isValid NOTIFY validityChanged)
    Q_PROPERTY(QUrl nmeaSource READ nmeaSource WRITE setNmeaSource NOTIFY nmeaSourceChanged)
    Q_PROPERTY(int updateInterval READ updateInterval WRITE setUpdateInterval NOTIFY updateIntervalChanged)
    Q_PROPERTY(PositioningMethods supportedPositioningMethods READ supportedPositioningMethods NOTIFY supportedPositioningMethodsChanged)
    Q_PROPERTY(PositioningMethods preferredPositioningMethods READ preferredPositioningMethods WRITE setPreferredPositioningMethods NOTIFY preferredPositioningMethodsChanged)
    Q_PROPERTY(SourceError sourceError READ sourceError NOTIFY sourceErrorChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_ENUMS(PositioningMethod)

    Q_INTERFACES(QQmlParserStatus)

public:
    enum PositioningMethod {
        NoPositioningMethods = QGeoPositionInfoSource::NoPositioningMethods,
        SatellitePositioningMethods = QGeoPositionInfoSource::SatellitePositioningMethods,
        NonSatellitePositioningMethods = QGeoPositionInfoSource::NonSatellitePositioningMethods,
        AllPositioningMethods = QGeoPositionInfoSource::AllPositioningMethods
    };

    Q_DECLARE_FLAGS(PositioningMethods, PositioningMethod)
    Q_FLAGS(PositioningMethods)

    enum SourceError {
        AccessError = QGeoPositionInfoSource::AccessError,
        ClosedError = QGeoPositionInfoSource::ClosedError,
        UnknownSourceError = QGeoPositionInfoSource::UnknownSourceError,
        NoError = QGeoPositionInfoSource::NoError,

        //Leave a gap for future error enum values in QGeoPositionInfoSource::Error
        SocketError = 100
    };
    Q_ENUMS(SourceError)

    QDeclarativePositionSource();
    ~QDeclarativePositionSource();
    void setNmeaSource(const QUrl &nmeaSource);
    void setUpdateInterval(int updateInterval);
    void setActive(bool active);
    void setPreferredPositioningMethods(PositioningMethods methods);

    QString name() const;
    void setName(const QString &name);

    QUrl nmeaSource() const;
    int updateInterval() const;
    bool isActive() const;
    bool isValid() const;
    QDeclarativePosition *position();
    PositioningMethods supportedPositioningMethods() const;
    PositioningMethods preferredPositioningMethods() const;
    SourceError sourceError() const;

    // Virtuals from QQmlParserStatus
    void classBegin() { }
    void componentComplete();

public Q_SLOTS:
    void update(); // TODO Qt 6 change to void update(int)
    void start();
    void stop();

Q_SIGNALS:
    void positionChanged();
    void activeChanged();
    void nmeaSourceChanged();
    void updateIntervalChanged();
    void supportedPositioningMethodsChanged();
    void preferredPositioningMethodsChanged();
    void sourceErrorChanged();
    void nameChanged();
    void validityChanged();
    void updateTimeout();

private Q_SLOTS:
    void positionUpdateReceived(const QGeoPositionInfo &update);
    void sourceErrorReceived(const QGeoPositionInfoSource::Error error);
    void socketConnected();
    void socketError(QAbstractSocket::SocketError error);
    void updateTimeoutReceived();

private:
    void setPosition(const QGeoPositionInfo &pi);

    QGeoPositionInfoSource *m_positionSource;
    QDeclarativePosition m_position;
    PositioningMethods m_preferredPositioningMethods;
    QFile *m_nmeaFile;
    QTcpSocket *m_nmeaSocket;
    QString m_nmeaFileName;
    QUrl m_nmeaSource;
    bool m_active;
    bool m_singleUpdate;
    int m_updateInterval;
    SourceError m_sourceError;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativePositionSource)

#endif
