/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtMultimedia module of the Qt Toolkit.
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

#ifndef QAUDIOOUTPUT_MAC_P_H
#define QAUDIOOUTPUT_MAC_P_H

#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include <QtCore/qobject.h>
#include <QtCore/qmutex.h>
#include <QtCore/qwaitcondition.h>
#include <QtCore/qtimer.h>
#include <QtCore/qatomic.h>

#include <QtMultimedia/qaudio.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qaudioengine.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

class QIODevice;
class QAbstractAudioDeviceInfo;

namespace QtMultimediaInternal
{
class QAudioOutputBuffer;
}

class QAudioOutputPrivate : public QAbstractAudioOutput
{
    Q_OBJECT

public:
    bool            isOpen;
    int             internalBufferSize;
    int             periodSizeBytes;
    qint64          totalFrames;
    QAudioFormat    audioFormat;
    QIODevice*      audioIO;
    AudioDeviceID   audioDeviceId;
    AudioUnit       audioUnit;
    Float64         clockFrequency;
    UInt64          startTime;
    AudioStreamBasicDescription deviceFormat;
    AudioStreamBasicDescription streamFormat;
    QtMultimediaInternal::QAudioOutputBuffer*   audioBuffer;
    QAtomicInt      audioThreadState;
    QWaitCondition  threadFinished;
    QMutex          mutex;
    QTimer*         intervalTimer;
    QAbstractAudioDeviceInfo *audioDeviceInfo;

    QAudio::Error    errorCode;
    QAudio::State    stateCode;

    QAudioOutputPrivate(const QByteArray& device, const QAudioFormat& format);
    ~QAudioOutputPrivate();

    bool open();
    void close();

    QAudioFormat format() const;

    QIODevice* start(QIODevice* device);
    void stop();
    void reset();
    void suspend();
    void resume();

    int bytesFree() const;
    int periodSize() const;

    void setBufferSize(int value);
    int bufferSize() const;

    void setNotifyInterval(int milliSeconds);
    int notifyInterval() const;

    qint64 processedUSecs() const;
    qint64 elapsedUSecs() const;

    QAudio::Error error() const;
    QAudio::State state() const;

    void audioThreadStart();
    void audioThreadStop();
    void audioThreadDrain();

    void audioDeviceStop();
    void audioDeviceIdle();
    void audioDeviceError();

    void startTimers();
    void stopTimers();

private slots:
    void deviceStopped();
    void inputReady();

private:
    enum { Running, Draining, Stopped };

    static OSStatus renderCallback(void* inRefCon,
                                    AudioUnitRenderActionFlags* ioActionFlags,
                                    const AudioTimeStamp* inTimeStamp,
                                    UInt32 inBusNumber,
                                    UInt32 inNumberFrames,
                                    AudioBufferList* ioData);
};

QT_END_NAMESPACE

QT_END_HEADER

#endif
