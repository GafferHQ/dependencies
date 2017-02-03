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


#ifndef QAUDIO_MAC_P_H
#define QAUDIO_MAC_P_H

#include <CoreAudio/CoreAudio.h>

#include <QtCore/qdebug.h>
#include <QtCore/qatomic.h>

#include <QtMultimedia/qaudioformat.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Multimedia)


extern QDebug operator<<(QDebug dbg, const QAudioFormat& audioFormat);

extern QAudioFormat toQAudioFormat(const AudioStreamBasicDescription& streamFormat);
extern AudioStreamBasicDescription toAudioStreamBasicDescription(QAudioFormat const& audioFormat);

class QAudioRingBuffer
{
public:
    typedef QPair<char*, int> Region;

    QAudioRingBuffer(int bufferSize);
    ~QAudioRingBuffer();

    Region acquireReadRegion(int size)
    {
        const int used = m_bufferUsed.fetchAndAddAcquire(0);

        if (used > 0) {
            const int readSize = qMin(size, qMin(m_bufferSize - m_readPos, used));

            return readSize > 0 ? Region(m_buffer + m_readPos, readSize) : Region(0, 0);
        }

        return Region(0, 0);
    }

    void releaseReadRegion(Region const& region)
    {
        m_readPos = (m_readPos + region.second) % m_bufferSize;

        m_bufferUsed.fetchAndAddRelease(-region.second);
    }

    Region acquireWriteRegion(int size)
    {
        const int free = m_bufferSize - m_bufferUsed.fetchAndAddAcquire(0);

        if (free > 0) {
            const int writeSize = qMin(size, qMin(m_bufferSize - m_writePos, free));

            return writeSize > 0 ? Region(m_buffer + m_writePos, writeSize) : Region(0, 0);
        }

        return Region(0, 0);
    }

    void releaseWriteRegion(Region const& region)
    {
        m_writePos = (m_writePos + region.second) % m_bufferSize;

        m_bufferUsed.fetchAndAddRelease(region.second);
    }

    int used() const;
    int free() const;
    int size() const;

    void reset();

private:
    int     m_bufferSize;
    int     m_readPos;
    int     m_writePos;
    char*   m_buffer;
    QAtomicInt  m_bufferUsed;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif  // QAUDIO_MAC_P_H


