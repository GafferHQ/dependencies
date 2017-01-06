/****************************************************************************
**
** Copyright (C) 2011 - 2012 Research In Motion <blackberry-qt@qnx.com>
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

//#define QBBVIRTUALKEYBOARD_DEBUG

#include "qbbvirtualkeyboardpps.h"

#include <QDebug>
#include <QSocketNotifier>
#include <QtCore/private/qcore_unix_p.h>
#include <QtGui/QApplication>
#include <QtGui/QPlatformScreen>
#include <QtGui/QPlatformWindow>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/iomsg.h>
#include <sys/pps.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


QT_BEGIN_NAMESPACE

const char  *QBBVirtualKeyboardPps::sPPSPath = "/pps/services/input/control";
const size_t QBBVirtualKeyboardPps::sBufferSize = 2048;

// Huge hack for keyboard shadow (see QNX PR 88400). Should be removed ASAP.
#define KEYBOARD_SHADOW_HEIGHT 8

QBBVirtualKeyboardPps::QBBVirtualKeyboardPps()
    : mEncoder(0),
      mDecoder(0),
      mBuffer(0),
      mFd(-1),
      mReadNotifier(0)
{
}

QBBVirtualKeyboardPps::~QBBVirtualKeyboardPps()
{
    close();
}

void QBBVirtualKeyboardPps::start()
{
#ifdef QBBVIRTUALKEYBOARD_DEBUG
    qDebug() << "QBB: starting keyboard event processing";
#endif

    if (!connect())
        return;
}

void QBBVirtualKeyboardPps::applyKeyboardMode(KeyboardMode mode)
{
    applyKeyboardModeOptions(mode);
}

void QBBVirtualKeyboardPps::close()
{
    delete mReadNotifier;
    mReadNotifier = 0;

    if (mFd != -1) {
        // any reads will fail after we close the fd, which is basically what we want.
        ::close(mFd);
        mFd = -1;
    }

    if (mDecoder) {
        pps_decoder_cleanup(mDecoder);
        delete mDecoder;
        mDecoder = 0;
    }

    if (mEncoder) {
        pps_encoder_cleanup(mEncoder);
        delete mEncoder;
        mEncoder = 0;
    }

    delete [] mBuffer;
    mBuffer = 0;
}

bool QBBVirtualKeyboardPps::connect()
{
    close();

    mEncoder = new pps_encoder_t;
    mDecoder = new pps_decoder_t;

    pps_encoder_initialize(mEncoder, false);
    pps_decoder_initialize(mDecoder, NULL);

    errno = 0;
    mFd = ::open(sPPSPath, O_RDWR);
    if (mFd == -1) {
        qCritical("QBBVirtualKeyboard: Unable to open \"%s\" for keyboard: %s (%d).",
                sPPSPath, strerror(errno), errno);
        close();
        return false;
    }

    mBuffer = new char[sBufferSize];
    if (!mBuffer) {
        qCritical("QBBVirtualKeyboard: Unable to allocate buffer of %d bytes. Size is unavailable.",  sBufferSize);
        return false;
    }

    if (!queryPPSInfo())
        return false;

    mReadNotifier = new QSocketNotifier(mFd, QSocketNotifier::Read);
    QObject::connect(mReadNotifier, SIGNAL(activated(int)), this, SLOT(ppsDataReady()));

    return true;
}

bool QBBVirtualKeyboardPps::queryPPSInfo()
{
    // Request info, requires id to regenerate res message.
    pps_encoder_add_string(mEncoder, "msg", "info");
    pps_encoder_add_string(mEncoder, "id", "libWebView");

    if (::write(mFd, pps_encoder_buffer(mEncoder), pps_encoder_length(mEncoder)) == -1) {
        close();
        return false;
    }

    pps_encoder_reset(mEncoder);

    return true;
}

void QBBVirtualKeyboardPps::ppsDataReady()
{
    ssize_t nread = qt_safe_read(mFd, mBuffer, sBufferSize - 1);

#ifdef QBBVIRTUALKEYBOARD_DEBUG
    qDebug() << "QBB: keyboardMessage size: " << nread;
#endif
    if (nread < 0) {
        connect(); // reconnect
        return;
    }

    // We sometimes get spurious read notifications when no data is available.
    // Bail out early in this case
    if (nread == 0)
        return;

    // nread is the real space necessary, not the amount read.
    if (static_cast<size_t>(nread) > sBufferSize - 1) {
        qCritical("QBBVirtualKeyboard: Keyboard buffer size too short; need %u.", nread + 1);
        connect(); // reconnect
        return;
    }

    mBuffer[nread] = 0;
    pps_decoder_parse_pps_str(mDecoder, mBuffer);
    pps_decoder_push(mDecoder, NULL);
#ifdef QBBVIRTUALKEYBOARD_DEBUG
    pps_decoder_dump_tree(mDecoder, stderr);
#endif

    const char* value;
    if (pps_decoder_get_string(mDecoder, "error", &value) == PPS_DECODER_OK) {
        qCritical("QBBVirtualKeyboard: Keyboard PPS decoder error: %s", value ? value : "[null]");
        return;
    }

    if (pps_decoder_get_string(mDecoder, "msg", &value) == PPS_DECODER_OK) {
        if (strcmp(value, "show") == 0)
            setVisible(true);
        else if (strcmp(value, "hide") == 0)
            setVisible(false);
        else if (strcmp(value, "info") == 0)
            handleKeyboardInfoMessage();
        else if (strcmp(value, "connect") == 0) { }
        else
            qCritical("QBBVirtualKeyboard: Unexpected keyboard PPS msg value: %s", value ? value : "[null]");
    } else if (pps_decoder_get_string(mDecoder, "res", &value) == PPS_DECODER_OK) {
        if (strcmp(value, "info") == 0)
            handleKeyboardInfoMessage();
        else
            qCritical("QBBVirtualKeyboard: Unexpected keyboard PPS res value: %s", value ? value : "[null]");
    } else {
        qCritical("QBBVirtualKeyboard: Unexpected keyboard PPS message type");
    }
}

void QBBVirtualKeyboardPps::handleKeyboardInfoMessage()
{
    int newHeight = 0;
    const char* value;

    if (pps_decoder_push(mDecoder, "dat") != PPS_DECODER_OK) {
        qCritical("QBBVirtualKeyboard: Keyboard PPS dat object not found");
        return;
    }
    if (pps_decoder_get_int(mDecoder, "size", &newHeight) != PPS_DECODER_OK) {
        qCritical("QBBVirtualKeyboard: Keyboard PPS size field not found");
        return;
    }
    if (pps_decoder_push(mDecoder, "locale") != PPS_DECODER_OK) {
        qCritical("QBBVirtualKeyboard: Keyboard PPS locale object not found");
        return;
    }
    if (pps_decoder_get_string(mDecoder, "languageId", &value) != PPS_DECODER_OK) {
        qCritical("QBBVirtualKeyboard: Keyboard PPS languageId field not found");
        return;
    }
    setLanguage(QString::fromLatin1(value));

    if (pps_decoder_get_string(mDecoder, "countryId", &value) != PPS_DECODER_OK) {
        qCritical("QBBVirtualKeyboard: Keyboard PPS size countryId not found");
        return;
    }
    setCountry(QString::fromLatin1(value));

    // HUGE hack, should be removed ASAP.
    newHeight -= KEYBOARD_SHADOW_HEIGHT; // We want to ignore the 8 pixel shadow above the keyboard. (PR 88400)
    setHeight(newHeight);

#ifdef QBBVIRTUALKEYBOARD_DEBUG
    qDebug() << "QBB: handleKeyboardInfoMessage size=" << getHeight() << "languageId=" << languageId() << " countryId=" << countryId();
#endif
}

bool QBBVirtualKeyboardPps::showKeyboard()
{
#ifdef QBBVIRTUALKEYBOARD_DEBUG
    qDebug() << "QBB: showKeyboard()";
#endif

    // Try to connect.
    if (mFd == -1 && !connect())
        return false;

    // NOTE:  This must be done everytime the keyboard is shown even if there is no change because
    // hiding the keyboard wipes the setting.
    applyKeyboardModeOptions(keyboardMode());

    pps_encoder_reset(mEncoder);

    // Send the show message.
    pps_encoder_add_string(mEncoder, "msg", "show");

    if (::write(mFd, pps_encoder_buffer(mEncoder), pps_encoder_length(mEncoder)) == -1) {
        close();
        return false;
    }

    pps_encoder_reset(mEncoder);

    // Return true if no error occurs.  Sizing response will be triggered when confirmation of
    // the change arrives.
    return true;
}

bool QBBVirtualKeyboardPps::hideKeyboard()
{
#ifdef QBBVIRTUALKEYBOARD_DEBUG
    qDebug() << "QBB: hideKeyboard()";
#endif

    if (mFd == -1 && !connect())
        return false;

    pps_encoder_add_string(mEncoder, "msg", "hide");

    if (::write(mFd, pps_encoder_buffer(mEncoder), pps_encoder_length(mEncoder)) == -1) {
        close();

        //Try again.
        if (connect()) {
            if (::write(mFd, pps_encoder_buffer(mEncoder), pps_encoder_length(mEncoder)) == -1) {
                close();
                return false;
            }
        } else {
            return false;
        }
    }

    pps_encoder_reset(mEncoder);

    // Return true if no error occurs.  Sizing response will be triggered when confirmation of
    // the change arrives.
    return true;
}

void QBBVirtualKeyboardPps::applyKeyboardModeOptions(KeyboardMode mode)
{
    // Try to connect.
    if (mFd == -1 && !connect())
        return;

    // Send the options message.
    pps_encoder_add_string(mEncoder, "msg", "options");

    pps_encoder_start_object(mEncoder, "dat");
    switch (mode) {
    case Url:
        addUrlModeOptions();
        break;
    case Email:
        addEmailModeOptions();
        break;
    case Web:
        addWebModeOptions();
        break;
    case NumPunc:
        addNumPuncModeOptions();
        break;
    case Symbol:
        addSymbolModeOptions();
        break;
    case Phone:
        addPhoneModeOptions();
        break;
    case Pin:
        addPinModeOptions();
        break;
    case Default: // fall through
    default:
        addDefaultModeOptions();
        break;
    }

    pps_encoder_end_object(mEncoder);

    if (::write(mFd, pps_encoder_buffer(mEncoder), pps_encoder_length(mEncoder)) == -1)
        close();

    pps_encoder_reset(mEncoder);
}

void QBBVirtualKeyboardPps::addDefaultModeOptions()
{
    pps_encoder_add_string(mEncoder, "enter", "enter.default");
    pps_encoder_add_string(mEncoder, "type", "default");
}

void QBBVirtualKeyboardPps::addUrlModeOptions()
{
    pps_encoder_add_string(mEncoder, "enter", "enter.default");
    pps_encoder_add_string(mEncoder, "type", "url");
}

void QBBVirtualKeyboardPps::addEmailModeOptions()
{
    pps_encoder_add_string(mEncoder, "enter", "enter.default");
    pps_encoder_add_string(mEncoder, "type", "email");
}

void QBBVirtualKeyboardPps::addWebModeOptions()
{
    pps_encoder_add_string(mEncoder, "enter", "enter.default");
    pps_encoder_add_string(mEncoder, "type", "web");
}

void QBBVirtualKeyboardPps::addNumPuncModeOptions()
{
    pps_encoder_add_string(mEncoder, "enter", "enter.default");
    pps_encoder_add_string(mEncoder, "type", "numPunc");
}

void QBBVirtualKeyboardPps::addPhoneModeOptions()
{
    pps_encoder_add_string(mEncoder, "enter", "enter.default");
    pps_encoder_add_string(mEncoder, "type", "phone");
}

void QBBVirtualKeyboardPps::addPinModeOptions()
{
    pps_encoder_add_string(mEncoder, "enter", "enter.default");
    pps_encoder_add_string(mEncoder, "type", "pin");
}

void QBBVirtualKeyboardPps::addSymbolModeOptions()
{
    pps_encoder_add_string(mEncoder, "enter", "enter.default");
    pps_encoder_add_string(mEncoder, "type", "symbol");
}

QT_END_NAMESPACE
