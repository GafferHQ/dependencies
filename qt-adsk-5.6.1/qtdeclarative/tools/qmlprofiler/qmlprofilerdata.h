/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QMLPROFILERDATA_H
#define QMLPROFILERDATA_H

#include "qmlprofilereventlocation.h"

#include <QtQml/private/qqmlprofilerdefinitions_p.h>
#include <QObject>

class QmlProfilerDataPrivate;
class QmlProfilerData : public QObject
{
    Q_OBJECT
public:
    enum State {
        Empty,
        AcquiringData,
        ProcessingData,
        Done
    };

    explicit QmlProfilerData(QObject *parent = 0);
    ~QmlProfilerData();

    static QString getHashStringForQmlEvent(const QmlEventLocation &location, int eventType);
    static QString getHashStringForV8Event(const QString &displayName, const QString &function);
    static QString qmlRangeTypeAsString(QQmlProfilerDefinitions::RangeType type);
    static QString qmlMessageAsString(QQmlProfilerDefinitions::Message type);
    static QString rootEventName();
    static QString rootEventDescription();

    qint64 traceStartTime() const;
    qint64 traceEndTime() const;

    bool isEmpty() const;

signals:
    void error(QString);
    void stateChanged();
    void dataReady();

public slots:
    void clear();
    void setTraceEndTime(qint64 time);
    void setTraceStartTime(qint64 time);
    void addQmlEvent(QQmlProfilerDefinitions::RangeType type,
                     QQmlProfilerDefinitions::BindingType bindingType,
                     qint64 startTime, qint64 duration, const QStringList &data,
                     const QmlEventLocation &location);
    void addV8Event(int depth, const QString &function, const QString &filename,
                    int lineNumber, double totalTime, double selfTime);
    void addFrameEvent(qint64 time, int framerate, int animationcount, int threadId);
    void addSceneGraphFrameEvent(QQmlProfilerDefinitions::SceneGraphFrameType type, qint64 time,
                                 qint64 numericData1, qint64 numericData2, qint64 numericData3,
                                 qint64 numericData4, qint64 numericData5);
    void addPixmapCacheEvent(QQmlProfilerDefinitions::PixmapEventType type, qint64 time,
                             const QmlEventLocation &location, int width, int height, int refcount);
    void addMemoryEvent(QQmlProfilerDefinitions::MemoryType type, qint64 time, qint64 size);
    void addInputEvent(QQmlProfilerDefinitions::EventType type, qint64 time);

    void complete();
    bool save(const QString &filename);

private:
    void sortStartTimes();
    int v8EventIndex(const QString &hashStr);
    void computeQmlTime();
    void setState(QmlProfilerData::State state);

private:
    QmlProfilerDataPrivate *d;
};

#endif // QMLPROFILERDATA_H
