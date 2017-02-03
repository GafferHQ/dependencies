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

#include "qabstractvideosurface_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QAbstractVideoSurface
    \brief The QAbstractVideoSurface class is a base class for video presentation surfaces.
    \since 4.6

    The QAbstractVideoSurface class defines the standard interface that video producers use to
    inter-operate with video presentation surfaces.  It is not supposed to be instantiated directly.
    Instead, you should subclass it to create new video surfaces.

    A video surface presents a continuous stream of identically formatted frames, where the format
    of each frame is compatible with a stream format supplied when starting a presentation.

    A list of pixel formats a surface can present is given by the supportedPixelFormats() function,
    and the isFormatSupported() function will test if a video surface format is supported.  If a
    format is not supported the nearestFormat() function may be able to suggest a similar format.
    For example if a surface supports fixed set of resolutions it may suggest the smallest
    supported resolution that contains the proposed resolution.

    The start() function takes a supported format and enables a video surface.  Once started a
    surface will begin displaying the frames it receives in the present() function.  Surfaces may
    hold a reference to the buffer of a presented video frame until a new frame is presented or
    streaming is stopped. The stop() function will disable a surface and a release any video
    buffers it holds references to.
*/

/*!
    \enum QAbstractVideoSurface::Error
    This enum describes the errors that may be returned by the error() function.

    \value NoError No error occurred.
    \value UnsupportedFormatError A video format was not supported.
    \value IncorrectFormatError A video frame was not compatible with the format of the surface.
    \value StoppedError The surface has not been started.
    \value ResourceError The surface could not allocate some resource.
*/

/*!
    Constructs a video surface with the given \a parent.
*/

QAbstractVideoSurface::QAbstractVideoSurface(QObject *parent)
    : QObject(*new QAbstractVideoSurfacePrivate, parent)
{
}

/*!
    \internal
*/

QAbstractVideoSurface::QAbstractVideoSurface(QAbstractVideoSurfacePrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

/*!
    Destroys a video surface.
*/

QAbstractVideoSurface::~QAbstractVideoSurface()
{
}

/*!
    \fn QAbstractVideoSurface::supportedPixelFormats(QAbstractVideoBuffer::HandleType type) const

    Returns a list of pixel formats a video surface can present for a given handle \a type.

    The pixel formats returned for the QAbstractVideoBuffer::NoHandle type are valid for any buffer
    that can be mapped in read-only mode.

    Types that are first in the list can be assumed to be faster to render.
*/

/*!
    Tests a video surface \a format to determine if a surface can accept it.

    Returns true if the format is supported by the surface, and false otherwise.
*/

bool QAbstractVideoSurface::isFormatSupported(const QVideoSurfaceFormat &format) const
{
    return supportedPixelFormats(format.handleType()).contains(format.pixelFormat());
}

/*!
    Returns a supported video surface format that is similar to \a format.

    A similar surface format is one that has the same \l {QVideoSurfaceFormat::pixelFormat()}{pixel
    format} and \l {QVideoSurfaceFormat::handleType()}{handle type} but differs in some of the other
    properties.  For example if there are restrictions on the \l {QVideoSurfaceFormat::frameSize()}
    {frame sizes} a video surface can accept it may suggest a format with a larger frame size and
    a \l {QVideoSurfaceFormat::viewport()}{viewport} the size of the original frame size.

    If the format is already supported it will be returned unchanged, or if there is no similar
    supported format an invalid format will be returned.
*/

QVideoSurfaceFormat QAbstractVideoSurface::nearestFormat(const QVideoSurfaceFormat &format) const
{
    return isFormatSupported(format)
            ? format
            : QVideoSurfaceFormat();
}

/*!
    \fn QAbstractVideoSurface::supportedFormatsChanged()

    Signals that the set of formats supported by a video surface has changed.

    \sa supportedPixelFormats(), isFormatSupported()
*/

/*!
    Returns the format of a video surface.
*/

QVideoSurfaceFormat QAbstractVideoSurface::surfaceFormat() const
{
    return d_func()->format;
}

/*!
    \fn QAbstractVideoSurface::surfaceFormatChanged(const QVideoSurfaceFormat &format)

    Signals that the configured \a format of a video surface has changed.

    \sa surfaceFormat(), start()
*/

/*!
    Starts a video surface presenting \a format frames.

    Returns true if the surface was started, and false if an error occurred.

    \sa isActive(), stop()
*/

bool QAbstractVideoSurface::start(const QVideoSurfaceFormat &format)
{
    Q_D(QAbstractVideoSurface);

    bool wasActive  = d->active;

    d->active = true;
    d->format = format;
    d->error = NoError;

    emit surfaceFormatChanged(d->format);

    if (!wasActive)
        emit activeChanged(true);

    return true;
}

/*!
    Stops a video surface presenting frames and releases any resources acquired in start().

    \sa isActive(), start()
*/

void QAbstractVideoSurface::stop()
{
    Q_D(QAbstractVideoSurface);

    if (d->active) {
        d->format = QVideoSurfaceFormat();
        d->active = false;

        emit activeChanged(false);
        emit surfaceFormatChanged(d->format);
    }
}

/*!
    Indicates whether a video surface has been started.

    Returns true if the surface has been started, and false otherwise.
*/

bool QAbstractVideoSurface::isActive() const
{
    return d_func()->active;
}

/*!
    \fn QAbstractVideoSurface::activeChanged(bool active)

    Signals that the \a active state of a video surface has changed.

    \sa isActive(), start(), stop()
*/

/*!
    \fn QAbstractVideoSurface::present(const QVideoFrame &frame)

    Presents a video \a frame.

    Returns true if the frame was presented, and false if an error occurred.

    Not all surfaces will block until the presentation of a frame has completed.  Calling present()
    on a non-blocking surface may fail if called before the presentation of a previous frame has
    completed.  In such cases the surface may not return to a ready state until it's had an
    opportunity to process events.

    If present() fails for any other reason the surface will immediately enter the stopped state
    and an error() value will be set.

    A video surface must be in the started state for present() to succeed, and the format of the
    video frame must be compatible with the current video surface format.

    \sa error()
*/

/*!
    Returns the last error that occurred.

    If a surface fails to start(), or stops unexpectedly this function can be called to discover
    what error occurred.
*/

QAbstractVideoSurface::Error QAbstractVideoSurface::error() const
{
    return d_func()->error;
}

/*!
    Sets the value of error() to \a error.
*/

void QAbstractVideoSurface::setError(Error error)
{
    Q_D(QAbstractVideoSurface);

    d->error = error;
}

QT_END_NAMESPACE
