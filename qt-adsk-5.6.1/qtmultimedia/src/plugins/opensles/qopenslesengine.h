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

#ifndef QOPENSLESENGINE_H
#define QOPENSLESENGINE_H

#include <qglobal.h>
#include <qaudio.h>
#include <qlist.h>
#include <qaudioformat.h>
#include <SLES/OpenSLES.h>

QT_BEGIN_NAMESPACE

class QOpenSLESEngine
{
public:
    enum OutputValue { FramesPerBuffer, SampleRate };

    QOpenSLESEngine();
    ~QOpenSLESEngine();

    static QOpenSLESEngine *instance();

    SLEngineItf slEngine() const { return m_engine; }

    static SLDataFormat_PCM audioFormatToSLFormatPCM(const QAudioFormat &format);

    QList<QByteArray> availableDevices(QAudio::Mode mode) const;
    QList<int> supportedChannelCounts(QAudio::Mode mode) const;
    QList<int> supportedSampleRates(QAudio::Mode mode) const;

    static int getOutputValue(OutputValue type, int defaultValue = 0);
    static int getDefaultBufferSize(const QAudioFormat &format);
    static int getLowLatencyBufferSize(const QAudioFormat &format);
    static bool supportsLowLatency();
    static bool printDebugInfo();

private:
    void checkSupportedInputFormats();
    bool inputFormatIsSupported(SLDataFormat_PCM format);

    SLObjectItf m_engineObject;
    SLEngineItf m_engine;

    QList<int> m_supportedInputChannelCounts;
    QList<int> m_supportedInputSampleRates;
    bool m_checkedInputFormats;
};

QT_END_NAMESPACE

#endif // QOPENSLESENGINE_H
