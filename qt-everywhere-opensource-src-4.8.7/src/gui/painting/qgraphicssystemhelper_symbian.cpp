/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qapplication_p.h"
#include "qgraphicssystem_p.h"
#include "qgraphicssystemex_symbian_p.h"
#include "qgraphicssystemhelper_symbian.h"
#include "qt_s60_p.h"
#include "qwidget_p.h"

QT_BEGIN_NAMESPACE

/*!
    \enum QSymbianGraphicsSystemHelper::NativePaintMode

    This enum controls the way in which QWidget paints content from the Qt
    backing store into the corresponding native window surface.

    \value NativePaintModeDefault Default painting behaviour.
    \value NativePaintModeZeroFill Ignore contents of backing store, and
        fill the window surface region with zeroes.
    \value NativePaintModeWriteAlpha By default, alpha values are only copied
        from the backing store into the window surface if the top-level widget
        has the Qt::WA_TranslucentBackground attribute.  If this mode is set,
        alpha values are copied regardless of the value of that attribute.
    \value NativePaintModeDisable Do not paint anything into the native window
        surface.
 */

/*!
    \class QSymbianGraphicsSystemHelper
    \ingroup painting

    \brief QSymbianGraphicsSystemHelper defines functions required by
    QtMultimediaKit in order to enable video rendering.

    This class is not intended for use by applications.
*/

/*!
    Specify whether native focus change events should be ignored by the widget.
*/

void QSymbianGraphicsSystemHelper::setIgnoreFocusChanged(QWidget *widget, bool value)
{
    static_cast<QSymbianControl *>(widget->winId())->setIgnoreFocusChanged(value);
}

/*!
    Set the native paint mode to the specified \a mode.
*/

void QSymbianGraphicsSystemHelper::setNativePaintMode(QWidget *widget, NativePaintMode mode)
{
    QWidgetPrivate *widgetPrivate = qt_widget_private(widget->window());
    widgetPrivate->createExtra();
    QWExtra::NativePaintMode widgetMode = QWExtra::Default;
    switch (mode) {
    case NativePaintModeDefault:
        break;
    case NativePaintModeZeroFill:
        widgetMode = QWExtra::ZeroFill;
        break;
    case NativePaintModeWriteAlpha:
        widgetMode = QWExtra::BlitWriteAlpha;
        break;
    case NativePaintModeDisable:
        widgetMode = QWExtra::Disable;
        break;
    }
    widgetPrivate->extraData()->nativePaintMode = widgetMode;
}

/*!
    Set the native paint mode to the specified \a mode.
*/

void QSymbianGraphicsSystemHelper::setNativePaintMode(WId wid, NativePaintMode mode)
{
    QWidget *window = static_cast<QSymbianControl *>(wid)->widget()->window();
    setNativePaintMode(window, mode);
}

/*!
    Specify whether the widget should receive receive native paint events.

    If enabled, the QWidget::beginNativePaintEvent slot is called before
    content from the backing store is written into the native window
    surface, and QWidget::endNativePaintEvent is called once writing to
    the native window surface is complete.

    This function is intended for use by QWidget clients such as video
    widgets, which wish to use Direct Screen Access to write into the
    native window surface.  Such clients should stop their DSA session on
    receipt of QWidget::beginNativePaintEvent, and re-start it on receipt of
    QWidget::endNativePaintEvent.
*/

void QSymbianGraphicsSystemHelper::setReceiveNativePaintEvents(QWidget *widget, bool value)
{
    QWidgetPrivate *widgetPrivate = qt_widget_private(widget);
    widgetPrivate->createExtra();
    widgetPrivate->extraData()->receiveNativePaintEvents = value;
}

void QSymbianGraphicsSystemHelper::releaseCachedGpuResources()
{
    QSymbianGraphicsSystemEx *ex = static_cast<QSymbianGraphicsSystemEx *>(
        QApplicationPrivate::graphicsSystem()->platformExtension());
    if (ex)
        ex->releaseCachedGpuResources();
}

QT_END_NAMESPACE
