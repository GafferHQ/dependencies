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

#ifndef QAUDIOINPUT_SYMBIAN_P_H
#define QAUDIOINPUT_SYMBIAN_P_H

#include <QtMultimedia/qaudioengine.h>
#include <QTime>
#include <QTimer>
#include "qaudio_symbian_p.h"

QT_BEGIN_NAMESPACE

class QAudioInputPrivate;

class SymbianAudioInputPrivate : public QIODevice
{
    friend class QAudioInputPrivate;
    Q_OBJECT
public:
    SymbianAudioInputPrivate(QAudioInputPrivate *audio);
    ~SymbianAudioInputPrivate();

    qint64 readData(char *data, qint64 len);
    qint64 writeData(const char *data, qint64 len);

    void dataReady();

private:
    QAudioInputPrivate *const m_audioDevice;
};

class QAudioInputPrivate
    :   public QAbstractAudioInput
{
    friend class SymbianAudioInputPrivate;
    Q_OBJECT
public:
    QAudioInputPrivate(const QByteArray &device,
                      const QAudioFormat &audioFormat);
    ~QAudioInputPrivate();

    // QAbstractAudioInput
    QIODevice* start(QIODevice *device = 0);
    void stop();
    void reset();
    void suspend();
    void resume();
    int bytesReady() const;
    int periodSize() const;
    void setBufferSize(int value);
    int bufferSize() const;
    void setNotifyInterval(int milliSeconds);
    int notifyInterval() const;
    qint64 processedUSecs() const;
    qint64 elapsedUSecs() const;
    QAudio::Error error() const;
    QAudio::State state() const;
    QAudioFormat format() const;

private slots:
    void pullData();
    void devsoundInitializeComplete(int err);
    void devsoundBufferToBeEmptied(CMMFBuffer *);
    void devsoundRecordError(int err);

private:
   void open();
   void startRecording();
   void startDataTransfer();
   CMMFDataBuffer* currentBuffer() const;
   void pushData();
   qint64 read(char *data, qint64 len);
   void bufferEmptied();
   Q_INVOKABLE void close();

   qint64 getSamplesRecorded() const;

   void setError(QAudio::Error error);
   void setState(SymbianAudio::State state);

private:
    const QByteArray m_device;
    const QAudioFormat m_format;

    int m_clientBufferSize;
    int m_notifyInterval;
    QScopedPointer<QTimer> m_notifyTimer;
    QTime m_elapsed;
    QAudio::Error m_error;

    SymbianAudio::State m_internalState;
    QAudio::State m_externalState;

    bool m_pullMode;
    QIODevice *m_sink;

    QScopedPointer<QTimer> m_pullTimer;

    SymbianAudio::DevSoundWrapper* m_devSound;

    // Latest buffer provided by DevSound, to be empied of data.
    CMMFDataBuffer *m_devSoundBuffer;

    int m_devSoundBufferSize;

    // Total amount of data in buffers provided by DevSound
    int m_totalBytesReady;

    // Queue of buffers returned after call to CMMFDevSound::Pause().
    QList<CMMFDataBuffer *> m_devSoundBufferQ;

    // Current read position within m_devSoundBuffer
    qint64 m_devSoundBufferPos;

    // Samples recorded up to the last call to suspend().  It is necessary
    // to cache this because suspend() is implemented using
    // CMMFDevSound::Stop(), which resets DevSound's SamplesRecorded() counter.
    quint32 m_totalSamplesRecorded;

};

QT_END_NAMESPACE

#endif
