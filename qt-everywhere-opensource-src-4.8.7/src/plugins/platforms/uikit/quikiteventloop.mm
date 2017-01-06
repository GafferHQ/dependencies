/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "quikiteventloop.h"
#include "quikitintegration.h"
#include "quikitscreen.h"
#include "quikitwindow.h"
#include "quikitwindowsurface.h"

#include <UIKit/UIKit.h>

#include <QtGui/QApplication>
#include <QtGui/QWidget>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsItem>
#include <QtDebug>

@interface QUIKitAppDelegate :  NSObject <UIApplicationDelegate> {
    UIInterfaceOrientation mOrientation;
}

- (void)updateOrientation:(NSNotification *)notification;

@end

@interface EventLoopHelper : NSObject {
    QUIKitEventLoop *mIntegration;
}

- (id)initWithEventLoopIntegration:(QUIKitEventLoop *)integration;

- (void)processEventsAndSchedule;

@end

@implementation QUIKitAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    Q_UNUSED(launchOptions)
    mOrientation = application.statusBarOrientation;
    [self updateOrientation:nil];
    if (QUIKitIntegration::instance()->screens().size() > 0) {
        QUIKitScreen *screen = static_cast<QUIKitScreen *>(QUIKitIntegration::instance()->screens().at(0));
        screen->updateInterfaceOrientation();
    }
    foreach (QWidget *widget, qApp->topLevelWidgets()) {
        QUIKitWindow *platformWindow = static_cast<QUIKitWindow *>(widget->platformWindow());
        if (platformWindow) platformWindow->ensureNativeWindow();
    }
    // orientation support
    [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
    [[NSNotificationCenter defaultCenter] addObserver:self
                              selector:@selector(updateOrientation:)
                              name:UIDeviceOrientationDidChangeNotification
                              object:nil];
    return YES;
}

- (void)updateOrientation:(NSNotification *)notification
{
    Q_UNUSED(notification)
    UIInterfaceOrientation newOrientation = mOrientation;
    NSString *infoValue = @"";
    switch ([UIDevice currentDevice].orientation) {
    case UIDeviceOrientationUnknown:
        break;
    case UIDeviceOrientationPortrait:
        newOrientation = UIInterfaceOrientationPortrait;
        infoValue = @"UIInterfaceOrientationPortrait";
        break;
    case UIDeviceOrientationPortraitUpsideDown:
        newOrientation = UIInterfaceOrientationPortraitUpsideDown;
        infoValue = @"UIInterfaceOrientationPortraitUpsideDown";
        break;
    case UIDeviceOrientationLandscapeLeft:
        newOrientation = UIInterfaceOrientationLandscapeRight; // as documentated
        infoValue = @"UIInterfaceOrientationLandscapeRight";
        break;
    case UIDeviceOrientationLandscapeRight:
        newOrientation = UIInterfaceOrientationLandscapeLeft; // as documentated
        infoValue = @"UIInterfaceOrientationLandscapeLeft";
        break;
    case UIDeviceOrientationFaceUp:
    case UIDeviceOrientationFaceDown:
        break;
    }

    if (newOrientation == mOrientation)
        return;

    // check against supported orientations
    NSBundle *bundle = [NSBundle mainBundle];
    NSArray *orientations = [bundle objectForInfoDictionaryKey:@"UISupportedInterfaceOrientations"];
    if (![orientations containsObject:infoValue])
        return;

    mOrientation = newOrientation;
    [UIApplication sharedApplication].statusBarOrientation = mOrientation;
    if (QUIKitIntegration::instance()->screens().size() > 0) {
        QUIKitScreen *screen = static_cast<QUIKitScreen *>(QUIKitIntegration::instance()->screens().at(0));
        screen->updateInterfaceOrientation();
    }
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    Q_UNUSED(application)
    qApp->quit();
}

@end

@implementation EventLoopHelper

- (id)initWithEventLoopIntegration:(QUIKitEventLoop *)integration
{
    if ((self = [self init])) {
        mIntegration = integration;
    }
    return self;
}

- (void)processEventsAndSchedule
{
    QPlatformEventLoopIntegration::processEvents();
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSDate *nextDate = [[NSDate date] dateByAddingTimeInterval:((double)mIntegration->nextTimerEvent()/1000.)];
    [mIntegration->mTimer setFireDate:nextDate];
    [pool release];
}

@end

QT_BEGIN_NAMESPACE

QUIKitEventLoop::QUIKitEventLoop()
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    mInputHandler = new QUIKitSoftwareInputHandler;
    mHelper = [[EventLoopHelper alloc] initWithEventLoopIntegration:this];
    mTimer = [[NSTimer timerWithTimeInterval:0.030 target:mHelper selector:@selector(processEventsAndSchedule) userInfo:nil repeats:YES] retain];
    [pool release];
}

QUIKitEventLoop::~QUIKitEventLoop()
{
    [mTimer release];
    [mHelper release];
    delete mInputHandler;
}

void QUIKitEventLoop::startEventLoop()
{
    qApp->installEventFilter(mInputHandler);
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    [[NSRunLoop currentRunLoop] addTimer:[mTimer autorelease] forMode:NSDefaultRunLoopMode];
    UIApplicationMain(qApp->argc(), qApp->argv(), nil, @"QUIKitAppDelegate");
    [pool release];
}

void QUIKitEventLoop::quitEventLoop()
{

}

void QUIKitEventLoop::qtNeedsToProcessEvents()
{
    [mHelper performSelectorOnMainThread:@selector(processEventsAndSchedule) withObject:nil waitUntilDone:NO];
}

static UIReturnKeyType keyTypeForObject(QObject *obj)
{
    if (strcmp(obj->metaObject()->className(), "QDeclarativeTextEdit") == 0)
        return UIReturnKeyDefault;
    return UIReturnKeyDone;
}

bool QUIKitSoftwareInputHandler::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::RequestSoftwareInputPanel) {
        UIReturnKeyType returnKeyType = UIReturnKeyDone;
        if (QGraphicsView *declarativeView = qobject_cast<QGraphicsView *>(obj)) {
            // register on loosing the focus, so we can auto-remove the input panel again
            QGraphicsScene *scene = declarativeView->scene();
            if (scene) {
                if (mCurrentFocusObject)
                    disconnect(mCurrentFocusObject, 0, this, SLOT(activeFocusChanged(bool)));
                QObject *focus = dynamic_cast<QObject *>(scene->focusItem());
                mCurrentFocusObject = focus;
                if (focus) {
                    connect(mCurrentFocusObject, SIGNAL(activeFocusChanged(bool)), this, SLOT(activeFocusChanged(bool)));
                    returnKeyType = keyTypeForObject(mCurrentFocusObject);
                }
            }
        }
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if (widget) {
            mCurrentFocusWidget = widget;
            QUIKitWindow *platformWindow = static_cast<QUIKitWindow *>(widget->window()->platformWindow());
            if (platformWindow) {
                platformWindow->nativeView().returnKeyType = returnKeyType;
                [platformWindow->nativeView() becomeFirstResponder];
            }
            return true;
        }
    } else if (event->type() == QEvent::CloseSoftwareInputPanel) {
        QWidget *widget = qobject_cast<QWidget *>(obj);
        return closeSoftwareInputPanel(widget);
    }
    return false;
}

bool QUIKitSoftwareInputHandler::closeSoftwareInputPanel(QWidget *widget)
{
    if (widget) {
        QUIKitWindow *platformWindow = static_cast<QUIKitWindow *>(widget->window()->platformWindow());
        if (platformWindow) {
            [platformWindow->nativeView() resignFirstResponder];
            mCurrentFocusWidget = 0;
            if (mCurrentFocusObject)
                disconnect(mCurrentFocusObject, 0, this, SLOT(activeFocusChanged(bool)));
            mCurrentFocusObject = 0;
        }
        return true;
    }
    return false;
}

void QUIKitSoftwareInputHandler::activeFocusChanged(bool focus)
{
    if (!focus && mCurrentFocusWidget && mCurrentFocusObject) {
        closeSoftwareInputPanel(mCurrentFocusWidget);
    }
}

QT_END_NAMESPACE
