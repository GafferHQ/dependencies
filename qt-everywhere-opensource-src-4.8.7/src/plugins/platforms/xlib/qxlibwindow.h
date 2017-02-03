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

#ifndef QTESTLITEWINDOW_H
#define QTESTLITEWINDOW_H

#include "qxlibintegration.h"

#include <QPlatformWindow>
#include <QEvent>

#include <QObject>
#include <QImage>

struct QXlibMWMHints {
    ulong flags, functions, decorations;
    long input_mode;
    ulong status;
};

enum {
    MWM_HINTS_FUNCTIONS   = (1L << 0),

    MWM_FUNC_ALL      = (1L << 0),
    MWM_FUNC_RESIZE   = (1L << 1),
    MWM_FUNC_MOVE     = (1L << 2),
    MWM_FUNC_MINIMIZE = (1L << 3),
    MWM_FUNC_MAXIMIZE = (1L << 4),
    MWM_FUNC_CLOSE    = (1L << 5),

    MWM_HINTS_DECORATIONS = (1L << 1),

    MWM_DECOR_ALL      = (1L << 0),
    MWM_DECOR_BORDER   = (1L << 1),
    MWM_DECOR_RESIZEH  = (1L << 2),
    MWM_DECOR_TITLE    = (1L << 3),
    MWM_DECOR_MENU     = (1L << 4),
    MWM_DECOR_MINIMIZE = (1L << 5),
    MWM_DECOR_MAXIMIZE = (1L << 6),

    MWM_HINTS_INPUT_MODE = (1L << 2),

    MWM_INPUT_MODELESS                  = 0L,
    MWM_INPUT_PRIMARY_APPLICATION_MODAL = 1L,
    MWM_INPUT_FULL_APPLICATION_MODAL    = 3L
};

class QXlibWindow : public QPlatformWindow
{
public:
    QXlibWindow(QWidget *window);
    ~QXlibWindow();


    void mousePressEvent(XButtonEvent*);
    void handleMouseEvent(QEvent::Type, XButtonEvent *ev);

    void handleCloseEvent();
    void handleEnterEvent();
    void handleLeaveEvent();
    void handleFocusInEvent();
    void handleFocusOutEvent();

    void resizeEvent(XConfigureEvent *configure_event);
    void paintEvent();

    void requestActivateWindow();

    void setGeometry(const QRect &rect);

    Qt::WindowFlags setWindowFlags(Qt::WindowFlags type);
    Qt::WindowFlags windowFlags() const;
    void setVisible(bool visible);
    WId winId() const;
    void setParent(const QPlatformWindow *window);
    void raise();
    void lower();
    void setWindowTitle(const QString &title);

    void setCursor(const Cursor &cursor);

    QPlatformGLContext *glContext() const;

    Window xWindow() const;
    GC graphicsContext() const;

    inline uint depth() const { return mDepth; }
    QImage::Format format() const { return mFormat; }
    Visual* visual() const { return mVisual; }

protected:
    QVector<Atom> getNetWmState() const;
    void setMWMHints(const QXlibMWMHints &mwmhints);
    QXlibMWMHints getMWMHints() const;

    void doSizeHints();

private:
    QPlatformWindowFormat correctColorBuffers(const QPlatformWindowFormat &windowFormat)const;

    Window x_window;
    GC gc;

    uint mDepth;
    QImage::Format mFormat;
    Visual* mVisual;

    GC createGC();

    QPlatformGLContext *mGLContext;
    QXlibScreen *mScreen;
    Qt::WindowFlags mWindowFlags;
};

#endif
