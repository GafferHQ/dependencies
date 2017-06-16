/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Copyright (C) 2012 Andre Hartmann <aha_1980@gmx.de>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
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

#include "qserialport_p.h"

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qthread.h>
#include <QtCore/qtimer.h>
#include <algorithm>

#ifndef CTL_CODE
#  define CTL_CODE(DeviceType, Function, Method, Access) ( \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
    )
#endif

#ifndef FILE_DEVICE_SERIAL_PORT
#  define FILE_DEVICE_SERIAL_PORT  27
#endif

#ifndef METHOD_BUFFERED
#  define METHOD_BUFFERED  0
#endif

#ifndef FILE_ANY_ACCESS
#  define FILE_ANY_ACCESS  0x00000000
#endif

#ifndef IOCTL_SERIAL_GET_DTRRTS
#  define IOCTL_SERIAL_GET_DTRRTS \
    CTL_CODE(FILE_DEVICE_SERIAL_PORT, 30, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#ifndef SERIAL_DTR_STATE
#  define SERIAL_DTR_STATE  0x00000001
#endif

#ifndef SERIAL_RTS_STATE
#  define SERIAL_RTS_STATE  0x00000002
#endif

QT_BEGIN_NAMESPACE

class QSerialPortPrivate;

class CommEventNotifier : public QThread
{
    Q_OBJECT
signals:
    void eventMask(quint32 mask);

public:
    CommEventNotifier(DWORD mask, QSerialPortPrivate *d, QObject *parent)
        : QThread(parent), dptr(d) {
        connect(this, &CommEventNotifier::eventMask, this, &CommEventNotifier::processNotification);
        ::SetCommMask(dptr->handle, mask);
    }

    virtual ~CommEventNotifier() {
        ::SetCommMask(dptr->handle, 0);
    }

protected:
    void run() Q_DECL_OVERRIDE {
        DWORD mask = 0;
        while (true) {
            if (::WaitCommEvent(dptr->handle, &mask, FALSE)) {
                // Wait until complete the operation changes the port settings,
                // see updateDcb().
                dptr->settingsChangeMutex.lock();
                dptr->settingsChangeMutex.unlock();
                emit eventMask(quint32(mask));
            }
        }
    }

private slots:
    void processNotification(quint32 eventMask) {

        bool error = false;

        // Check for unexpected event. This event triggered when pulled previously
        // opened device from the system, when opened as for not to read and not to
        // write options and so forth.
        if ((eventMask == 0)
                || ((eventMask & (EV_ERR | EV_RXCHAR | EV_TXEMPTY)) == 0)) {
            error = true;
        }

        if (EV_RXCHAR & eventMask)
            dptr->notifyRead();
        if (EV_TXEMPTY & eventMask)
            dptr->notifyWrite();
    }

private:
    QSerialPortPrivate *dptr;
};

class WaitCommEventBreaker : public QThread
{
    Q_OBJECT
public:
    WaitCommEventBreaker(HANDLE handle, int timeout, QObject *parent = Q_NULLPTR)
        : QThread(parent), handle(handle), timeout(timeout), worked(false) {
        start();
    }

    virtual ~WaitCommEventBreaker() {
        stop();
        wait();
    }

    void stop() {
        exit(0);
    }

    bool isWorked() const {
        return worked;
    }

protected:
    void run() {
        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, this, &WaitCommEventBreaker::processTimeout, Qt::DirectConnection);
        timer.start(timeout);
        exec();
        worked = true;
    }

private slots:
    void processTimeout() {
        ::SetCommMask(handle, 0);
        stop();
    }

private:
    HANDLE handle;
    int timeout;
    mutable bool worked;
};

#include "qserialport_wince.moc"

bool QSerialPortPrivate::open(QIODevice::OpenMode mode)
{
    DWORD desiredAccess = 0;
    DWORD eventMask = 0;

    if (mode & QIODevice::ReadOnly) {
        desiredAccess |= GENERIC_READ;
        eventMask |= EV_RXCHAR;
    }
    if (mode & QIODevice::WriteOnly) {
        desiredAccess |= GENERIC_WRITE;
        eventMask |= EV_TXEMPTY;
    }

    handle = ::CreateFile(reinterpret_cast<const wchar_t*>(systemLocation.utf16()),
                              desiredAccess, 0, Q_NULLPTR, OPEN_EXISTING, 0, Q_NULLPTR);

    if (handle == INVALID_HANDLE_VALUE) {
        setError(getSystemError());
        return false;
    }

    if (initialize(eventMask))
        return true;

    ::CloseHandle(handle);
    return false;
}

void QSerialPortPrivate::close()
{
    if (eventNotifier) {
        eventNotifier->terminate();
        eventNotifier->wait();
        delete eventNotifier;
        eventNotifier = Q_NULLPTR;
    }

    if (settingsRestoredOnClose) {
        ::SetCommState(handle, &restoredDcb);
        ::SetCommTimeouts(handle, &restoredCommTimeouts);
    }

    ::CloseHandle(handle);
    handle = INVALID_HANDLE_VALUE;
}

QSerialPort::PinoutSignals QSerialPortPrivate::pinoutSignals()
{
    DWORD modemStat = 0;

    if (!::GetCommModemStatus(handle, &modemStat)) {
        setError(getSystemError());
        return QSerialPort::NoSignal;
    }

    QSerialPort::PinoutSignals ret = QSerialPort::NoSignal;

    if (modemStat & MS_CTS_ON)
        ret |= QSerialPort::ClearToSendSignal;
    if (modemStat & MS_DSR_ON)
        ret |= QSerialPort::DataSetReadySignal;
    if (modemStat & MS_RING_ON)
        ret |= QSerialPort::RingIndicatorSignal;
    if (modemStat & MS_RLSD_ON)
        ret |= QSerialPort::DataCarrierDetectSignal;

    DWORD bytesReturned = 0;
    if (!::DeviceIoControl(handle, IOCTL_SERIAL_GET_DTRRTS, Q_NULLPTR, 0,
                          &modemStat, sizeof(modemStat),
                          &bytesReturned, Q_NULLPTR)) {
        setError(getSystemError());
        return ret;
    }

    if (modemStat & SERIAL_DTR_STATE)
        ret |= QSerialPort::DataTerminalReadySignal;
    if (modemStat & SERIAL_RTS_STATE)
        ret |= QSerialPort::RequestToSendSignal;

    return ret;
}

bool QSerialPortPrivate::setDataTerminalReady(bool set)
{
    if (!::EscapeCommFunction(handle, set ? SETDTR : CLRDTR)) {
        setError(getSystemError());
        return false;
    }

    currentDcb.fDtrControl = set ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
    return true;
}

bool QSerialPortPrivate::setRequestToSend(bool set)
{
    if (!::EscapeCommFunction(handle, set ? SETRTS : CLRRTS)) {
        setError(getSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::flush()
{
    return notifyWrite() && ::FlushFileBuffers(handle);
}

bool QSerialPortPrivate::clear(QSerialPort::Directions directions)
{
    DWORD flags = 0;
    if (directions & QSerialPort::Input)
        flags |= PURGE_RXABORT | PURGE_RXCLEAR;
    if (directions & QSerialPort::Output)
        flags |= PURGE_TXABORT | PURGE_TXCLEAR;
    return ::PurgeComm(handle, flags);
}

bool QSerialPortPrivate::sendBreak(int duration)
{
    if (!setBreakEnabled(true))
        return false;

    ::Sleep(duration);

    if (!setBreakEnabled(false))
        return false;

    return true;
}

bool QSerialPortPrivate::setBreakEnabled(bool set)
{
    if (set ? !::SetCommBreak(handle) : !::ClearCommBreak(handle)) {
        setError(getSystemError());
        return false;
    }

    return true;
}

bool QSerialPortPrivate::waitForReadyRead(int msec)
{
    if (!buffer.isEmpty())
        return true;

    QElapsedTimer stopWatch;
    stopWatch.start();

    forever {
        bool readyToRead = false;
        bool readyToWrite = false;
        if (!waitForReadOrWrite(&readyToRead, &readyToWrite,
                                true, !writeBuffer.isEmpty(),
                                qt_subtract_from_timeout(msec, stopWatch.elapsed()))) {
            return false;
        }
        if (readyToRead) {
            if (notifyRead())
                return true;
        }
        if (readyToWrite)
            notifyWrite();
    }
    return false;
}

bool QSerialPortPrivate::waitForBytesWritten(int msec)
{
    if (writeBuffer.isEmpty())
        return false;

    QElapsedTimer stopWatch;
    stopWatch.start();

    forever {
        bool readyToRead = false;
        bool readyToWrite = false;
        if (!waitForReadOrWrite(&readyToRead, &readyToWrite,
                                true, !writeBuffer.isEmpty(),
                                qt_subtract_from_timeout(msec, stopWatch.elapsed()))) {
            return false;
        }
        if (readyToRead) {
            if (!notifyRead())
                return false;
        }
        if (readyToWrite) {
            if (notifyWrite())
                return true;
        }
    }
    return false;
}

bool QSerialPortPrivate::setBaudRate()
{
    return setBaudRate(inputBaudRate, QSerialPort::AllDirections);
}

bool QSerialPortPrivate::setBaudRate(qint32 baudRate, QSerialPort::Directions directions)
{
    if (directions != QSerialPort::AllDirections) {
        setError(QSerialPortErrorInfo(QSerialPort::UnsupportedOperationError, QSerialPort::tr("Custom baud rate direction is unsupported")));
        return false;
    }
    currentDcb.BaudRate = baudRate;
    return updateDcb();
}

bool QSerialPortPrivate::setDataBits(QSerialPort::DataBits dataBits)
{
    currentDcb.ByteSize = dataBits;
    return updateDcb();
}

bool QSerialPortPrivate::setParity(QSerialPort::Parity parity)
{
    currentDcb.fParity = TRUE;
    switch (parity) {
    case QSerialPort::NoParity:
        currentDcb.Parity = NOPARITY;
        currentDcb.fParity = FALSE;
        break;
    case QSerialPort::OddParity:
        currentDcb.Parity = ODDPARITY;
        break;
    case QSerialPort::EvenParity:
        currentDcb.Parity = EVENPARITY;
        break;
    case QSerialPort::MarkParity:
        currentDcb.Parity = MARKPARITY;
        break;
    case QSerialPort::SpaceParity:
        currentDcb.Parity = SPACEPARITY;
        break;
    default:
        currentDcb.Parity = NOPARITY;
        currentDcb.fParity = FALSE;
        break;
    }
    return updateDcb();
}

bool QSerialPortPrivate::setStopBits(QSerialPort::StopBits stopBits)
{
    switch (stopBits) {
    case QSerialPort::OneStop:
        currentDcb.StopBits = ONESTOPBIT;
        break;
    case QSerialPort::OneAndHalfStop:
        currentDcb.StopBits = ONE5STOPBITS;
        break;
    case QSerialPort::TwoStop:
        currentDcb.StopBits = TWOSTOPBITS;
        break;
    default:
        currentDcb.StopBits = ONESTOPBIT;
        break;
    }
    return updateDcb();
}

bool QSerialPortPrivate::setFlowControl(QSerialPort::FlowControl flowControl)
{
    currentDcb.fInX = FALSE;
    currentDcb.fOutX = FALSE;
    currentDcb.fOutxCtsFlow = FALSE;
    currentDcb.fRtsControl = RTS_CONTROL_DISABLE;
    switch (flowControl) {
    case QSerialPort::NoFlowControl:
        break;
    case QSerialPort::SoftwareControl:
        currentDcb.fInX = TRUE;
        currentDcb.fOutX = TRUE;
        break;
    case QSerialPort::HardwareControl:
        currentDcb.fOutxCtsFlow = TRUE;
        currentDcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
        break;
    default:
        break;
    }
    return updateDcb();
}

bool QSerialPortPrivate::notifyRead()
{
    Q_Q(QSerialPort);

    DWORD bytesToRead = ReadChunkSize;

    if (readBufferMaxSize && bytesToRead > (readBufferMaxSize - buffer.size())) {
        bytesToRead = readBufferMaxSize - buffer.size();
        if (bytesToRead == 0) {
            // Buffer is full. User must read data from the buffer
            // before we can read more from the port.
            return false;
        }
    }

    char *ptr = buffer.reserve(bytesToRead);

    DWORD readBytes = 0;
    BOOL sucessResult = ::ReadFile(handle, ptr, bytesToRead, &readBytes, Q_NULLPTR);

    if (!sucessResult) {
        buffer.chop(bytesToRead);
        setError(QSerialPortErrorInfo(QSerialPort::ReadError));
        return false;
    }

    buffer.chop(bytesToRead - qMax(readBytes, DWORD(0)));

    if (readBytes > 0)
        emit q->readyRead();

    return true;
}

bool QSerialPortPrivate::notifyWrite()
{
    Q_Q(QSerialPort);

    int nextSize = writeBuffer.nextDataBlockSize();

    const char *ptr = writeBuffer.readPointer();

    DWORD bytesWritten = 0;
    if (!::WriteFile(handle, ptr, nextSize, &bytesWritten, Q_NULLPTR)) {
        setError(QSerialPortErrorInfo(QSerialPort::WriteError));
        return false;
    }

    writeBuffer.free(bytesWritten);

    if (bytesWritten > 0)
        emit q->bytesWritten(bytesWritten);

    return true;
}

qint64 QSerialPortPrivate::writeData(const char *data, qint64 maxSize)
{
    ::memcpy(writeBuffer.reserve(maxSize), data, maxSize);
    if (!writeBuffer.isEmpty())
        notifyWrite();
    return maxSize;
}

inline bool QSerialPortPrivate::initialize(DWORD eventMask)
{
    Q_Q(QSerialPort);

    ::ZeroMemory(&restoredDcb, sizeof(restoredDcb));
    restoredDcb.DCBlength = sizeof(restoredDcb);

    if (!::GetCommState(handle, &restoredDcb)) {
        setError(getSystemError());
        return false;
    }

    currentDcb = restoredDcb;
    currentDcb.fBinary = true;
    currentDcb.fInX = false;
    currentDcb.fOutX = false;
    currentDcb.fAbortOnError = false;
    currentDcb.fNull = false;
    currentDcb.fErrorChar = false;

    if (currentDcb.fDtrControl ==  DTR_CONTROL_HANDSHAKE)
        currentDcb.fDtrControl = DTR_CONTROL_DISABLE;

    if (!updateDcb())
        return false;

    if (!::GetCommTimeouts(handle, &restoredCommTimeouts)) {
        setError(getSystemError());
        return false;
    }

    ::memset(&currentCommTimeouts, 0, sizeof(currentCommTimeouts));
    currentCommTimeouts.ReadIntervalTimeout = MAXDWORD;

    if (!::SetCommTimeouts(handle, &currentCommTimeouts)) {
        setError(getSystemError());
        return false;
    }

    eventNotifier = new CommEventNotifier(eventMask, this, q);
    eventNotifier->start();

    return true;
}

bool QSerialPortPrivate::updateDcb()
{
    QMutexLocker locker(&settingsChangeMutex);

    DWORD eventMask = 0;
    // Save the event mask
    if (!::GetCommMask(handle, &eventMask))
        return false;

    // Break event notifier from WaitCommEvent
    ::SetCommMask(handle, 0);
    // Change parameters
    bool ret = ::SetCommState(handle, &currentDcb);
    if (!ret)
        setError(getSystemError());
    // Restore the event mask
    ::SetCommMask(handle, eventMask);

    return ret;
}

QSerialPortErrorInfo QSerialPortPrivate::getSystemError(int systemErrorCode) const
{
    if (systemErrorCode == -1)
        systemErrorCode = ::GetLastError();

    QSerialPortErrorInfo error;
    error.errorString = qt_error_string(systemErrorCode);

    switch (systemErrorCode) {
    case ERROR_IO_PENDING:
        error.errorCode = QSerialPort::NoError;
        break;
    case ERROR_MORE_DATA:
        error.errorCode = QSerialPort::NoError;
        break;
    case ERROR_FILE_NOT_FOUND:
        error.errorCode = QSerialPort::DeviceNotFoundError;
        break;
    case ERROR_PATH_NOT_FOUND:
        error.errorCode = QSerialPort::DeviceNotFoundError;
        break;
    case ERROR_INVALID_NAME:
        error.errorCode = QSerialPort::DeviceNotFoundError;
        break;
    case ERROR_ACCESS_DENIED:
        error.errorCode = QSerialPort::PermissionError;
        break;
    case ERROR_INVALID_HANDLE:
        error.errorCode = QSerialPort::ResourceError;
        break;
    case ERROR_INVALID_PARAMETER:
        error.errorCode = QSerialPort::UnsupportedOperationError;
        break;
    case ERROR_BAD_COMMAND:
        error.errorCode = QSerialPort::ResourceError;
        break;
    case ERROR_DEVICE_REMOVED:
        error.errorCode = QSerialPort::ResourceError;
        break;
    default:
        error.errorCode = QSerialPort::UnknownError;
        break;
    }
    return error;
}

bool QSerialPortPrivate::waitForReadOrWrite(bool *selectForRead, bool *selectForWrite,
                                           bool checkRead, bool checkWrite,
                                           int msecs)
{
    DWORD eventMask = 0;
    // FIXME: Here the situation is not properly handled with zero timeout:
    // breaker can work out before you call a method WaitCommEvent()
    // and so it will loop forever!
    WaitCommEventBreaker breaker(handle, qMax(msecs, 0));
    ::WaitCommEvent(handle, &eventMask, Q_NULLPTR);
    breaker.stop();

    if (breaker.isWorked()) {
        setError(QSerialPortErrorInfo(QSerialPort::TimeoutError));
    } else {
        if (checkRead) {
            Q_ASSERT(selectForRead);
            *selectForRead = eventMask & EV_RXCHAR;
        }
        if (checkWrite) {
            Q_ASSERT(selectForWrite);
            *selectForWrite = eventMask & EV_TXEMPTY;
        }

        return true;
    }

    return false;
}

static const QList<qint32> standardBaudRatePairList()
{

    static const QList<qint32> standardBaudRatesTable = QList<qint32>()

        #ifdef CBR_110
            << CBR_110
        #endif

        #ifdef CBR_300
            << CBR_300
        #endif

        #ifdef CBR_600
            << CBR_600
        #endif

        #ifdef CBR_1200
            << CBR_1200
        #endif

        #ifdef CBR_2400
            << CBR_2400
        #endif

        #ifdef CBR_4800
            << CBR_4800
        #endif

        #ifdef CBR_9600
            << CBR_9600
        #endif

        #ifdef CBR_14400
            << CBR_14400
        #endif

        #ifdef CBR_19200
            << CBR_19200
        #endif

        #ifdef CBR_38400
            << CBR_38400
        #endif

        #ifdef CBR_56000
            << CBR_56000
        #endif

        #ifdef CBR_57600
            << CBR_57600
        #endif

        #ifdef CBR_115200
            << CBR_115200
        #endif

        #ifdef CBR_128000
            << CBR_128000
        #endif

        #ifdef CBR_256000
            << CBR_256000
        #endif
    ;

    return standardBaudRatesTable;
};

QList<qint32> QSerialPortPrivate::standardBaudRates()
{
    return standardBaudRatePairList();
}

QSerialPort::Handle QSerialPort::handle() const
{
    Q_D(const QSerialPort);
    return d->handle;
}

QT_END_NAMESPACE
