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

#include "qnsview.h"

#include <QtGui/QWindowSystemInterface>

#include <QtCore/QDebug>

@implementation QNSView

- (id) init
{
    self = [super init];
    if (self) {
        m_cgImage = 0;
        m_widget = 0;
        m_buttons = Qt::NoButton;
    }
    return self;
}

- (id)initWithWidget:(QWidget *)widget {
    self = [self init];
    if (self) {
        m_widget = widget;
    }
    return self;
}

- (void) setImage:(QImage *)image
{
    CGImageRelease(m_cgImage);

    const uchar *imageData = image->bits();
    int bitDepth = image->depth();
    int colorBufferSize = 8;
    int bytesPrLine = image->bytesPerLine();
    int width = image->width();
    int height = image->height();

    CGColorSpaceRef cgColourSpaceRef = CGColorSpaceCreateDeviceRGB();

    CGDataProviderRef cgDataProviderRef = CGDataProviderCreateWithData(
                NULL,
                imageData,
                image->byteCount(),
                NULL);

    m_cgImage = CGImageCreate(width,
                              height,
                              colorBufferSize,
                              bitDepth,
                              bytesPrLine,
                              cgColourSpaceRef,
                              kCGImageAlphaNone,
                              cgDataProviderRef,
                              NULL,
                              false,
                              kCGRenderingIntentDefault);

    CGColorSpaceRelease(cgColourSpaceRef);

}

- (void) drawRect:(NSRect)dirtyRect
{
    if (!m_cgImage)
        return;

    CGRect dirtyCGRect = NSRectToCGRect(dirtyRect);

    NSGraphicsContext *nsGraphicsContext = [NSGraphicsContext currentContext];
    CGContextRef cgContext = (CGContextRef) [nsGraphicsContext graphicsPort];

    CGContextSaveGState( cgContext );
    int dy = dirtyCGRect.origin.y + CGRectGetMaxY(dirtyCGRect);
    CGContextTranslateCTM(cgContext, 0, dy);
    CGContextScaleCTM(cgContext, 1, -1);

    CGImageRef subImage = CGImageCreateWithImageInRect(m_cgImage, dirtyCGRect);
    CGContextDrawImage(cgContext,dirtyCGRect,subImage);

    CGContextRestoreGState(cgContext);

    CGImageRelease(subImage);

}

- (BOOL) isFlipped
{
    return YES;
}

- (void)handleMouseEvent:(NSEvent *)theEvent;
{
    NSPoint point = [self convertPoint: [theEvent locationInWindow] fromView: nil];
    QPoint qt_localPoint(point.x,point.y);

    NSTimeInterval timestamp = [theEvent timestamp];
    ulong qt_timestamp = timestamp * 1000;

    QWindowSystemInterface::handleMouseEvent(m_widget,qt_timestamp,qt_localPoint,QPoint(),m_buttons);

}
    - (void)mouseDown:(NSEvent *)theEvent
    {
        m_buttons |= Qt::LeftButton;
        [self handleMouseEvent:theEvent];
    }
    - (void)mouseDragged:(NSEvent *)theEvent
    {
        if (!(m_buttons & Qt::LeftButton))
            qWarning("Internal Mousebutton tracking invalid(missing Qt::LeftButton");
        [self handleMouseEvent:theEvent];
    }
    - (void)mouseUp:(NSEvent *)theEvent
    {
        m_buttons &= QFlag(~int(Qt::LeftButton));
        [self handleMouseEvent:theEvent];
    }

- (void)mouseMoved:(NSEvent *)theEvent
{
    qDebug() << "mouseMove";
    [self handleMouseEvent:theEvent];
}
- (void)mouseEntered:(NSEvent *)theEvent
{
        Q_UNUSED(theEvent);
        QWindowSystemInterface::handleEnterEvent(m_widget);
}
- (void)mouseExited:(NSEvent *)theEvent
{
        Q_UNUSED(theEvent);
        QWindowSystemInterface::handleLeaveEvent(m_widget);
}
- (void)rightMouseDown:(NSEvent *)theEvent
{
        m_buttons |= Qt::RightButton;
    [self handleMouseEvent:theEvent];
}
- (void)rightMouseDragged:(NSEvent *)theEvent
{
        if (!(m_buttons & Qt::LeftButton))
            qWarning("Internal Mousebutton tracking invalid(missing Qt::LeftButton");
        [self handleMouseEvent:theEvent];
}
- (void)rightMouseUp:(NSEvent *)theEvent
{
        m_buttons &= QFlag(~int(Qt::RightButton));
        [self handleMouseEvent:theEvent];
}
- (void)otherMouseDown:(NSEvent *)theEvent
{
        m_buttons |= Qt::RightButton;
    [self handleMouseEvent:theEvent];
}
- (void)otherMouseDragged:(NSEvent *)theEvent
{
        if (!(m_buttons & Qt::LeftButton))
            qWarning("Internal Mousebutton tracking invalid(missing Qt::LeftButton");
        [self handleMouseEvent:theEvent];
}
- (void)otherMouseUp:(NSEvent *)theEvent
{
        m_buttons &= QFlag(~int(Qt::MiddleButton));
        [self handleMouseEvent:theEvent];
}



@end
