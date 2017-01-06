/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#include "qplatformdefs.h"

#include <QtCore/qatomic.h>

QT_BEGIN_NAMESPACE

class QCriticalSection
{
public:
        QCriticalSection()  { InitializeCriticalSection(&section); }
        ~QCriticalSection() { DeleteCriticalSection(&section); }
        void lock()         { EnterCriticalSection(&section); }
        void unlock()       { LeaveCriticalSection(&section); }

private:
        CRITICAL_SECTION section;
};

static QCriticalSection qAtomicCriticalSection;

Q_CORE_EXPORT
bool QBasicAtomicInt_testAndSetOrdered(volatile int *_q_value, int expectedValue, int newValue)
{
    bool returnValue = false;
    qAtomicCriticalSection.lock();
    if (*_q_value == expectedValue) {
        *_q_value = newValue;
        returnValue = true;
    }
        qAtomicCriticalSection.unlock();
    return returnValue;
}

Q_CORE_EXPORT
int QBasicAtomicInt_fetchAndStoreOrdered(volatile int *_q_value, int newValue)
{
    int returnValue;
        qAtomicCriticalSection.lock();
    returnValue = *_q_value;
    *_q_value = newValue;
        qAtomicCriticalSection.unlock();
    return returnValue;
}

Q_CORE_EXPORT
int QBasicAtomicInt_fetchAndAddOrdered(volatile int *_q_value, int valueToAdd)
{
    int returnValue;
        qAtomicCriticalSection.lock();
    returnValue = *_q_value;
    *_q_value += valueToAdd;
        qAtomicCriticalSection.unlock();
    return returnValue;
}

Q_CORE_EXPORT
bool QBasicAtomicPointer_testAndSetOrdered(void * volatile *_q_value,
                                           void *expectedValue,
                                           void *newValue)
{
    bool returnValue = false;
        qAtomicCriticalSection.lock();
    if (*_q_value == expectedValue) {
        *_q_value = newValue;
        returnValue = true;
    }
        qAtomicCriticalSection.unlock();
    return returnValue;
}

Q_CORE_EXPORT
void *QBasicAtomicPointer_fetchAndStoreOrdered(void * volatile *_q_value, void *newValue)
{
    void *returnValue;
        qAtomicCriticalSection.lock();
    returnValue = *_q_value;
    *_q_value = newValue;
        qAtomicCriticalSection.unlock();
    return returnValue;
}

Q_CORE_EXPORT
void *QBasicAtomicPointer_fetchAndAddOrdered(void * volatile *_q_value, qptrdiff valueToAdd)
{
    void *returnValue;
        qAtomicCriticalSection.lock();
    returnValue = *_q_value;
    *_q_value = reinterpret_cast<char *>(returnValue) + valueToAdd;
        qAtomicCriticalSection.unlock();
    return returnValue;
}

QT_END_NAMESPACE
