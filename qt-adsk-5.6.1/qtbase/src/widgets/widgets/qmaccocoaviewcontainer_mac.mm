/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#import <Cocoa/Cocoa.h>
#include "qmaccocoaviewcontainer_mac.h"

#include <QtCore/QDebug>
#include <QtGui/QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <private/qwidget_p.h>

/*!
    \class QMacCocoaViewContainer
    \since 4.5

    \brief The QMacCocoaViewContainer class provides a widget for OS X that can be used to wrap arbitrary
    Cocoa views (i.e., NSView subclasses) and insert them into Qt hierarchies.

    \ingroup advanced
    \inmodule QtWidgets

    While Qt offers a lot of classes for writing your application, Apple's
    Cocoa framework offers lots of functionality that is not currently in Qt or
    may never end up in Qt. Using QMacCocoaViewContainer, it is possible to put an
    arbitrary NSView-derived class from Cocoa and put it in a Qt hierarchy.
    Depending on how comfortable you are with using objective-C, you can use
    QMacCocoaViewContainer directly, or subclass it to wrap further functionality
    of the underlying NSView.

    QMacCocoaViewContainer works regardless if Qt is built against Carbon or
    Cocoa. However, QCocoaContainerView requires Mac OS X 10.5 or better to be
    used with Carbon.

    It should be also noted that at the low level on OS X, there is a
    difference between windows (top-levels) and view (widgets that are inside a
    window). For this reason, make sure that the NSView that you are wrapping
    doesn't end up as a top-level. The best way to ensure this is to make sure
    you always have a parent and not set the parent to 0.

    If you are using QMacCocoaViewContainer as a sub-class and are mixing and
    matching objective-C with C++ (a.k.a. objective-C++). It is probably
    simpler to have your file end with \tt{.mm} than \tt{.cpp}. Most Apple tools will
    correctly identify the source as objective-C++.

    QMacCocoaViewContainer requires knowledge of how Cocoa works, especially in
    regard to its reference counting (retain/release) nature. It is noted in
    the functions below if there is any change in the reference count. Cocoa
    views often generate temporary objects that are released by an autorelease
    pool. If this is done outside of a running event loop, it is up to the
    developer to provide the autorelease pool.

    The following is a snippet of subclassing QMacCocoaViewContainer to wrap a NSSearchField.
    \snippet macmainwindow.mm 0

*/

QT_BEGIN_NAMESPACE

namespace {
// TODO use QtMacExtras copy of this function when available.
inline QPlatformNativeInterface::NativeResourceForIntegrationFunction resolvePlatformFunction(const QByteArray &functionName)
{
    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function =
        nativeInterface->nativeResourceFunctionForIntegration(functionName);
    if (!function)
         qWarning() << "Qt could not resolve function" << functionName
                    << "from QGuiApplication::platformNativeInterface()->nativeResourceFunctionForIntegration()";
    return function;
}
} //namespsace

class QMacCocoaViewContainerPrivate : public QWidgetPrivate
{
    Q_DECLARE_PUBLIC(QMacCocoaViewContainer)
public:
    NSView *nsview;
    QMacCocoaViewContainerPrivate();
    ~QMacCocoaViewContainerPrivate();
};

QMacCocoaViewContainerPrivate::QMacCocoaViewContainerPrivate()
     : nsview(0)
{
}

QMacCocoaViewContainerPrivate::~QMacCocoaViewContainerPrivate()
{
    [nsview release];
}

/*!
    \fn QMacCocoaViewContainer::QMacCocoaViewContainer(NSView *cocoaViewToWrap, QWidget *parent)

    Create a new QMacCocoaViewContainer using the NSView pointer in \a
    cocoaViewToWrap with parent, \a parent. QMacCocoaViewContainer will
    retain \a cocoaViewToWrap.

*/
QMacCocoaViewContainer::QMacCocoaViewContainer(NSView *view, QWidget *parent)
   : QWidget(*new QMacCocoaViewContainerPrivate, parent, 0)
{

    if (view)
        setCocoaView(view);

    // QMacCocoaViewContainer requires a native window handle.
    setAttribute(Qt::WA_NativeWindow);
}

/*!
    Destroy the QMacCocoaViewContainer and release the wrapped view.
*/
QMacCocoaViewContainer::~QMacCocoaViewContainer()
{

}

/*!
    Returns the NSView that has been set on this container.
*/
NSView *QMacCocoaViewContainer::cocoaView() const
{
    Q_D(const QMacCocoaViewContainer);
    return d->nsview;
}

/*!
    Sets \a view as the NSView to contain and retains it. If this
    container already had a view set, it will release the previously set view.
*/
void QMacCocoaViewContainer::setCocoaView(NSView *view)
{
    Q_D(QMacCocoaViewContainer);
    NSView *oldView = d->nsview;
    [view retain];
    d->nsview = view;

    // Create window and platformwindow
    winId();
    QPlatformWindow *platformWindow = this->windowHandle()->handle();

    // Set the new view as the content view for the window.
    typedef void (*SetWindowContentViewFunction)(QPlatformWindow *window, NSView *nsview);
    reinterpret_cast<SetWindowContentViewFunction>(resolvePlatformFunction("setwindowcontentview"))(platformWindow, view);

    [oldView release];
}

QT_END_NAMESPACE
