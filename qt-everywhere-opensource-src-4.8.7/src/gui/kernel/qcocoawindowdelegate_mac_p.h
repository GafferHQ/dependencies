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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qmacdefines_mac.h"

#ifdef QT_MAC_USE_COCOA
#import <Cocoa/Cocoa.h>

QT_BEGIN_NAMESPACE
template <class Key, class T> class QHash;
QT_END_NAMESPACE
using QT_PREPEND_NAMESPACE(QHash);
QT_FORWARD_DECLARE_CLASS(QWidget)
QT_FORWARD_DECLARE_CLASS(QSize)
QT_FORWARD_DECLARE_CLASS(QWidgetData)

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_5
@protocol NSWindowDelegate <NSObject>
- (NSSize)windowWillResize:(NSWindow *)window toSize:(NSSize)proposedFrameSize;
- (void)windowDidMiniaturize:(NSNotification*)notification;
- (void)windowDidResize:(NSNotification *)notification;
- (NSRect)windowWillUseStandardFrame:(NSWindow *)window defaultFrame:(NSRect)defaultFrame;
- (void)windowDidMove:(NSNotification *)notification;
- (BOOL)windowShouldClose:(id)window;
- (void)windowDidDeminiaturize:(NSNotification *)notification;
- (void)windowDidBecomeMain:(NSNotification*)notification;
- (void)windowDidResignMain:(NSNotification*)notification;
- (void)windowDidBecomeKey:(NSNotification*)notification;
- (void)windowDidResignKey:(NSNotification*)notification;
- (void)windowDidChangeScreen:(NSNotification*)notification;
- (BOOL)window:(NSWindow *)window shouldPopUpDocumentPathMenu:(NSMenu *)menu;
- (BOOL)window:(NSWindow *)window shouldDragDocumentWithEvent:(NSEvent *)event from:(NSPoint)dragImageLocation withPasteboard:(NSPasteboard *)pasteboard;
- (BOOL)windowShouldZoom:(NSWindow *)window toFrame:(NSRect)newFrame;
@end

@protocol NSDrawerDelegate <NSObject>
- (NSSize)drawerWillResizeContents:(NSDrawer *)sender toSize:(NSSize)contentSize;
@end

#endif



@interface QT_MANGLE_NAMESPACE(QCocoaWindowDelegate) : NSObject<NSWindowDelegate, NSDrawerDelegate> {
    QHash<NSWindow *, QWidget *> *m_windowHash;
    QHash<NSDrawer *, QWidget *> *m_drawerHash;
}
+ (QT_MANGLE_NAMESPACE(QCocoaWindowDelegate)*)sharedDelegate;
- (void)becomeDelegateForWindow:(NSWindow *)window  widget:(QWidget *)widget;
- (void)resignDelegateForWindow:(NSWindow *)window;
- (void)becomeDelegateForDrawer:(NSDrawer *)drawer widget:(QWidget *)widget;
- (void)resignDelegateForDrawer:(NSDrawer *)drawer;
- (void)dumpMaximizedStateforWidget:(QWidget*)qwidget window:(NSWindow *)window;
- (void)syncSizeForWidget:(QWidget *)qwidget
        toSize:(const QSize &)newSize
        fromSize:(const QSize &)oldSize;
- (NSSize)closestAcceptableSizeForWidget:(QWidget *)qwidget
        window:(NSWindow *)window withNewSize:(NSSize)proposedSize;
- (QWidget *)qt_qwidgetForWindow:(NSWindow *)window;
- (void)syncContentViewFrame: (NSNotification *)notification;
@end
#endif
