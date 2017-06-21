/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qioswindow.h"

#include "qiosapplicationdelegate.h"
#include "qioscontext.h"
#include "qiosglobal.h"
#include "qiosintegration.h"
#include "qiosscreen.h"
#include "qiosviewcontroller.h"
#include "quiview.h"

#include <QtGui/private/qwindow_p.h>
#include <qpa/qplatformintegration.h>

#import <QuartzCore/CAEAGLLayer.h>

#include <QtDebug>

QIOSWindow::QIOSWindow(QWindow *window)
    : QPlatformWindow(window)
    , m_view([[QUIView alloc] initWithQIOSWindow:this])
    , m_windowLevel(0)
{
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &QIOSWindow::applicationStateChanged);

    setParent(QPlatformWindow::parent());

    // Resolve default window geometry in case it was not set before creating the
    // platform window. This picks up eg. minimum-size if set, and defaults to
    // the "maxmized" geometry (even though we're not in that window state).
    // FIXME: Detect if we apply a maximized geometry and send a window state
    // change event in that case.
    m_normalGeometry = initialGeometry(window, QPlatformWindow::geometry(),
        screen()->availableGeometry().width(), screen()->availableGeometry().height());

    setWindowState(window->windowState());
    setOpacity(window->opacity());

    Qt::ScreenOrientation initialOrientation = window->contentOrientation();
    if (initialOrientation != Qt::PrimaryOrientation) {
        // Start up in portrait, then apply possible content orientation,
        // as per Apple's documentation.
        dispatch_async(dispatch_get_main_queue(), ^{
            handleContentOrientationChange(initialOrientation);
        });
    }
}

QIOSWindow::~QIOSWindow()
{
    // According to the UIResponder documentation, Cocoa Touch should react to system interruptions
    // that "might cause the view to be removed from the window" by sending touchesCancelled, but in
    // practice this doesn't seem to happen when removing the view from its superview. To ensure that
    // Qt's internal state for touch and mouse handling is kept consistent, we therefor have to force
    // cancellation of all touch events.
    [m_view touchesCancelled:0 withEvent:0];

    clearAccessibleCache();
    m_view->m_qioswindow = 0;
    [m_view removeFromSuperview];
    [m_view release];
}


QSurfaceFormat QIOSWindow::format() const
{
    return window()->requestedFormat();
}


bool QIOSWindow::blockedByModal()
{
    QWindow *modalWindow = QGuiApplication::modalWindow();
    return modalWindow && modalWindow != window();
}

void QIOSWindow::setVisible(bool visible)
{
    m_view.hidden = !visible;
    [m_view setNeedsDisplay];

    if (!isQtApplication() || !window()->isTopLevel())
        return;

    // Since iOS doesn't do window management the way a Qt application
    // expects, we need to raise and activate windows ourselves:
    if (visible)
        updateWindowLevel();

    if (blockedByModal()) {
        if (visible)
            raise();
        return;
    }

    if (visible && shouldAutoActivateWindow()) {
        if (!window()->property("_q_showWithoutActivating").toBool())
            requestActivateWindow();
    } else if (!visible && [m_view isActiveWindow]) {
        // Our window was active/focus window but now hidden, so relinquish
        // focus to the next possible window in the stack.
        NSArray *subviews = m_view.viewController.view.subviews;
        for (int i = int(subviews.count) - 1; i >= 0; --i) {
            UIView *view = [subviews objectAtIndex:i];
            if (view.hidden)
                continue;

            QWindow *w = view.qwindow;
            if (!w || !w->isTopLevel())
                continue;

            QIOSWindow *iosWindow = static_cast<QIOSWindow *>(w->handle());
            if (!iosWindow->shouldAutoActivateWindow())
                continue;

            iosWindow->requestActivateWindow();
            break;
        }
    }
}

bool QIOSWindow::shouldAutoActivateWindow() const
{
    if (![m_view canBecomeFirstResponder])
        return false;

    // We don't want to do automatic window activation for popup windows
    // that are unlikely to contain editable controls (to avoid hiding
    // the keyboard while the popup is showing)
    const Qt::WindowType type = window()->type();
    return (type != Qt::Popup && type != Qt::ToolTip) || !window()->isActive();
}

void QIOSWindow::setOpacity(qreal level)
{
    m_view.alpha = qBound(0.0, level, 1.0);
}

void QIOSWindow::setGeometry(const QRect &rect)
{
    m_normalGeometry = rect;

    if (window()->windowState() != Qt::WindowNoState) {
        QPlatformWindow::setGeometry(rect);

        // The layout will realize the requested geometry was not applied, and
        // send geometry-change events that match the actual geometry.
        [m_view setNeedsLayout];

        if (window()->inherits("QWidgetWindow")) {
            // QWidget wrongly assumes that setGeometry resets the window
            // state back to Qt::NoWindowState, so we need to inform it that
            // that his is not the case by re-issuing the current window state.
            QWindowSystemInterface::handleWindowStateChanged(window(), window()->windowState());

            // It also needs to be told immediately that the geometry it requested
            // did not apply, otherwise it will continue on as if it did, instead
            // of waiting for a resize event.
            [m_view layoutIfNeeded];
        }

        return;
    }

    applyGeometry(rect);
}

void QIOSWindow::applyGeometry(const QRect &rect)
{
    // Geometry changes are asynchronous, but QWindow::geometry() is
    // expected to report back the 'requested geometry' until we get
    // a callback with the updated geometry from the window system.
    // The baseclass takes care of persisting this for us.
    QPlatformWindow::setGeometry(rect);

    m_view.frame = toCGRect(rect);

    // iOS will automatically trigger -[layoutSubviews:] for resize,
    // but not for move, so we force it just in case.
    [m_view setNeedsLayout];

    if (window()->inherits("QWidgetWindow"))
        [m_view layoutIfNeeded];
}

bool QIOSWindow::isExposed() const
{
    // Note: At startup of an iOS app it will enter UIApplicationStateInactive
    // while showing the launch screen, and once the application returns from
    // applicationDidFinishLaunching it will hide the launch screen and enter
    // UIApplicationStateActive. Technically, a window is not exposed until
    // it's actually visible on screen, and Apple also documents that "Apps
    // that use OpenGL ES for drawing must not use didFinishLaunching to
    // prepare their drawing environment. Instead, defer any OpenGL ES
    // drawing calls to applicationDidBecomeActive". Unfortunately, if we
    // wait until the applicationState reaches ApplicationActive to signal
    // that the window is exposed, we get a lag between hiding the launch
    // screen and blitting the first pixels of the application, as Qt
    // spends some time drawing those pixels in response to the expose.
    // In practice there doesn't seem to be any issues starting GL setup
    // and drawing from within applicationDidFinishLaunching, and this is
    // also the recommended approach for other 3rd party GL toolkits on iOS,
    // so we 'cheat', and report that a window is exposed even if the app
    // is in UIApplicationStateInactive, so that the startup transition
    // between the launch screen and the application content is smooth.

    return qApp->applicationState() > Qt::ApplicationHidden
        && window()->isVisible() && !window()->geometry().isEmpty();
}

void QIOSWindow::setWindowState(Qt::WindowState state)
{
    // Update the QWindow representation straight away, so that
    // we can update the statusbar visibility based on the new
    // state before applying geometry changes.
    qt_window_private(window())->windowState = state;

    if (window()->isTopLevel() && window()->isVisible() && window()->isActive())
        [m_view.qtViewController updateProperties];

    switch (state) {
    case Qt::WindowNoState:
        applyGeometry(m_normalGeometry);
        break;
    case Qt::WindowMaximized:
        applyGeometry(window()->flags() & Qt::MaximizeUsingFullscreenGeometryHint ?
            screen()->geometry() : screen()->availableGeometry());
        break;
    case Qt::WindowFullScreen:
        applyGeometry(screen()->geometry());
        break;
    case Qt::WindowMinimized:
        applyGeometry(QRect());
        break;
    case Qt::WindowActive:
        Q_UNREACHABLE();
    default:
        Q_UNREACHABLE();
    }
}

void QIOSWindow::setParent(const QPlatformWindow *parentWindow)
{
    UIView *parentView = parentWindow ? reinterpret_cast<UIView *>(parentWindow->winId())
        : isQtApplication() ? static_cast<QIOSScreen *>(screen())->uiWindow().rootViewController.view : 0;

    [parentView addSubview:m_view];
}

void QIOSWindow::requestActivateWindow()
{
    // Note that several windows can be active at the same time if they exist in the same
    // hierarchy (transient children). But only one window can be QGuiApplication::focusWindow().
    // Dispite the name, 'requestActivateWindow' means raise and transfer focus to the window:
    if (blockedByModal())
        return;

    Q_ASSERT(m_view.window);
    [m_view.window makeKeyWindow];
    [m_view becomeFirstResponder];

    if (window()->isTopLevel())
        raise();
}

void QIOSWindow::raiseOrLower(bool raise)
{
    // Re-insert m_view at the correct index among its sibling views
    // (QWindows) according to their current m_windowLevel:
    if (!isQtApplication())
        return;

    NSArray *subviews = m_view.superview.subviews;
    if (subviews.count == 1)
        return;

    for (int i = int(subviews.count) - 1; i >= 0; --i) {
        UIView *view = static_cast<UIView *>([subviews objectAtIndex:i]);
        if (view.hidden || view == m_view || !view.qwindow)
            continue;
        int level = static_cast<QIOSWindow *>(view.qwindow->handle())->m_windowLevel;
        if (m_windowLevel > level || (raise && m_windowLevel == level)) {
            [m_view.superview insertSubview:m_view aboveSubview:view];
            return;
        }
    }
    [m_view.superview insertSubview:m_view atIndex:0];
}

void QIOSWindow::updateWindowLevel()
{
    Qt::WindowType type = window()->type();

    if (type == Qt::ToolTip)
        m_windowLevel = 120;
    else if (window()->flags() & Qt::WindowStaysOnTopHint)
        m_windowLevel = 100;
    else if (window()->isModal())
        m_windowLevel = 40;
    else if (type == Qt::Popup)
        m_windowLevel = 30;
    else if (type == Qt::SplashScreen)
        m_windowLevel = 20;
    else if (type == Qt::Tool)
        m_windowLevel = 10;
    else
        m_windowLevel = 0;

    // A window should be in at least the same m_windowLevel as its parent:
    QWindow *transientParent = window()->transientParent();
    QIOSWindow *transientParentWindow = transientParent ? static_cast<QIOSWindow *>(transientParent->handle()) : 0;
    if (transientParentWindow)
        m_windowLevel = qMax(transientParentWindow->m_windowLevel, m_windowLevel);
}

void QIOSWindow::handleContentOrientationChange(Qt::ScreenOrientation orientation)
{
    // Update the QWindow representation straight away, so that
    // we can update the statusbar orientation based on the new
    // content orientation.
    qt_window_private(window())->contentOrientation = orientation;

    [m_view.qtViewController updateProperties];
}

void QIOSWindow::applicationStateChanged(Qt::ApplicationState)
{
    if (window()->isExposed() != isExposed())
        [m_view sendUpdatedExposeEvent];
}

qreal QIOSWindow::devicePixelRatio() const
{
    return m_view.contentScaleFactor;
}

void QIOSWindow::clearAccessibleCache()
{
    [m_view clearAccessibleCache];
}

#include "moc_qioswindow.cpp"

QT_END_NAMESPACE
