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

#include "qqmlprofileradapter.h"
#include <private/qqmldebugserviceinterfaces_p.h>

QT_BEGIN_NAMESPACE

QQmlProfilerAdapter::QQmlProfilerAdapter(QQmlProfilerService *service, QQmlEnginePrivate *engine) :
    QQmlAbstractProfilerAdapter(service), next(0)
{
    engine->enableProfiler();
    connect(this, SIGNAL(profilingEnabled(quint64)), engine->profiler, SLOT(startProfiling(quint64)));
    connect(this, SIGNAL(profilingEnabledWhileWaiting(quint64)),
            engine->profiler, SLOT(startProfiling(quint64)), Qt::DirectConnection);
    connect(this, SIGNAL(profilingDisabled()), engine->profiler, SLOT(stopProfiling()));
    connect(this, SIGNAL(profilingDisabledWhileWaiting()),
            engine->profiler, SLOT(stopProfiling()), Qt::DirectConnection);
    connect(this, SIGNAL(dataRequested()), engine->profiler, SLOT(reportData()));
    connect(this, SIGNAL(referenceTimeKnown(QElapsedTimer)),
            engine->profiler, SLOT(setTimer(QElapsedTimer)));
    connect(engine->profiler, SIGNAL(dataReady(QVector<QQmlProfilerData>)),
            this, SLOT(receiveData(QVector<QQmlProfilerData>)));
}

// convert to QByteArrays that can be sent to the debug client
// use of QDataStream can skew results
//     (see tst_qqmldebugtrace::trace() benchmark)
static void qQmlProfilerDataToByteArrays(const QQmlProfilerData *d, QList<QByteArray> &messages)
{
    QByteArray data;
    Q_ASSERT_X(((d->messageType | d->detailType) & (1 << 31)) == 0, Q_FUNC_INFO,
               "You can use at most 31 message types and 31 detail types.");
    for (uint decodedMessageType = 0; (d->messageType >> decodedMessageType) != 0;
         ++decodedMessageType) {
        if ((d->messageType & (1 << decodedMessageType)) == 0)
            continue;

        for (uint decodedDetailType = 0; (d->detailType >> decodedDetailType) != 0;
             ++decodedDetailType) {
            if ((d->detailType & (1 << decodedDetailType)) == 0)
                continue;

            //### using QDataStream is relatively expensive
            QQmlDebugStream ds(&data, QIODevice::WriteOnly);
            ds << d->time << decodedMessageType << decodedDetailType;

            switch (decodedMessageType) {
            case QQmlProfilerDefinitions::RangeStart:
                if (decodedDetailType == (int)QQmlProfilerDefinitions::Binding)
                    ds << QQmlProfilerDefinitions::QmlBinding;
                break;
            case QQmlProfilerDefinitions::RangeData:
                ds << (d->detailString.isEmpty() ? d->detailUrl.toString() : d->detailString);
                break;
            case QQmlProfilerDefinitions::RangeLocation:
                ds << (d->detailUrl.isEmpty() ? d->detailString : d->detailUrl.toString()) << d->x
                   << d->y;
                break;
            case QQmlProfilerDefinitions::RangeEnd: break;
            default:
                Q_ASSERT_X(false, Q_FUNC_INFO, "Invalid message type.");
                break;
            }
            messages << data;
            data.clear();
        }
    }
}

qint64 QQmlProfilerAdapter::sendMessages(qint64 until, QList<QByteArray> &messages)
{
    while (next != data.length()) {
        if (data[next].time > until)
            return data[next].time;
        qQmlProfilerDataToByteArrays(&(data[next++]), messages);
    }

    next = 0;
    data.clear();
    return -1;
}

void QQmlProfilerAdapter::receiveData(const QVector<QQmlProfilerData> &new_data)
{
    if (data.isEmpty())
        data = new_data;
    else
        data.append(new_data);
    service->dataReady(this);
}

QT_END_NAMESPACE
