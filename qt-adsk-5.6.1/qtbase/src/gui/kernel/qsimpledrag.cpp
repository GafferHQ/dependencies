/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include "qsimpledrag_p.h"

#include "qbitmap.h"
#include "qdrag.h"
#include "qpixmap.h"
#include "qevent.h"
#include "qfile.h"
#include "qtextcodec.h"
#include "qguiapplication.h"
#include "qpoint.h"
#include "qbuffer.h"
#include "qimage.h"
#include "qregexp.h"
#include "qdir.h"
#include "qimagereader.h"
#include "qimagewriter.h"

#include <QtCore/QEventLoop>
#include <QtCore/QDebug>

#include <private/qguiapplication_p.h>
#include <private/qdnd_p.h>

#include <private/qshapedpixmapdndwindow_p.h>
#include <private/qhighdpiscaling_p.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_DRAGANDDROP

static QWindow* topLevelAt(const QPoint &pos)
{
    QWindowList list = QGuiApplication::topLevelWindows();
    for (int i = list.count()-1; i >= 0; --i) {
        QWindow *w = list.at(i);
        if (w->isVisible() && w->geometry().contains(pos) && !qobject_cast<QShapedPixmapWindow*>(w))
            return w;
    }
    return 0;
}

/*!
    \class QBasicDrag
    \brief QBasicDrag is a base class for implementing platform drag and drop.
    \since 5.0
    \internal
    \ingroup qpa

    QBasicDrag implements QPlatformDrag::drag() by running a local event loop in which
    it tracks mouse movements and moves the drag icon (QShapedPixmapWindow) accordingly.
    It provides new virtuals allowing for querying whether the receiving window
    (within the Qt application or outside) accepts the drag and sets the state accordingly.
*/

QBasicDrag::QBasicDrag() :
    m_restoreCursor(false), m_eventLoop(0),
    m_executed_drop_action(Qt::IgnoreAction), m_can_drop(false),
    m_drag(0), m_drag_icon_window(0), m_useCompositing(true),
    m_screen(Q_NULLPTR)
{
}

QBasicDrag::~QBasicDrag()
{
    delete m_drag_icon_window;
}

void QBasicDrag::enableEventFilter()
{
    qApp->installEventFilter(this);
}

void QBasicDrag::disableEventFilter()
{
    qApp->removeEventFilter(this);
}


static inline QPoint getNativeMousePos(QEvent *e, QObject *o)
{
    return QHighDpi::toNativePixels(static_cast<QMouseEvent *>(e)->globalPos(), qobject_cast<QWindow*>(o));
}

bool QBasicDrag::eventFilter(QObject *o, QEvent *e)
{
    Q_UNUSED(o);

    if (!m_drag) {
        if (e->type() == QEvent::KeyRelease && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
            disableEventFilter();
            exitDndEventLoop();
            return true; // block the key release
        }
        return false;
    }

    switch (e->type()) {
        case QEvent::ShortcutOverride:
            // prevent accelerators from firing while dragging
            e->accept();
            return true;

        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        {
            QKeyEvent *ke = static_cast<QKeyEvent *>(e);
            if (ke->key() == Qt::Key_Escape && e->type() == QEvent::KeyPress) {
                cancel();
                disableEventFilter();
                exitDndEventLoop();

            }
            return true; // Eat all key events
        }

        case QEvent::MouseMove:
        {
            QPoint nativePosition = getNativeMousePos(e, o);
            move(nativePosition);
            return true; // Eat all mouse move events
        }
        case QEvent::MouseButtonRelease:
            disableEventFilter();
            if (canDrop()) {
                QPoint nativePosition = getNativeMousePos(e, o);
                drop(nativePosition);
            } else {
                cancel();
            }
            exitDndEventLoop();
            QCoreApplication::postEvent(o, new QMouseEvent(*static_cast<QMouseEvent *>(e)));
            return true; // defer mouse release events until drag event loop has returned
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
            return true;
        default:
             break;
    }
    return false;
}

Qt::DropAction QBasicDrag::drag(QDrag *o)
{
    m_drag = o;
    m_executed_drop_action = Qt::IgnoreAction;
    m_can_drop = false;
    m_restoreCursor = true;
#ifndef QT_NO_CURSOR
    qApp->setOverrideCursor(Qt::DragCopyCursor);
    updateCursor(m_executed_drop_action);
#endif
    startDrag();
    m_eventLoop = new QEventLoop;
    m_eventLoop->exec();
    delete m_eventLoop;
    m_eventLoop = 0;
    m_drag = 0;
    endDrag();
    return m_executed_drop_action;
}

void QBasicDrag::restoreCursor()
{
    if (m_restoreCursor) {
#ifndef QT_NO_CURSOR
        QGuiApplication::restoreOverrideCursor();
#endif
        m_restoreCursor = false;
    }
}

void QBasicDrag::startDrag()
{
    QPoint pos;
#ifndef QT_NO_CURSOR
    pos = QCursor::pos();
    if (pos.x() == int(qInf())) {
        // ### fixme: no mouse pos registered. Get pos from touch...
        pos = QPoint();
    }
#endif
    recreateShapedPixmapWindow(m_screen, pos);
    enableEventFilter();
}

void QBasicDrag::endDrag()
{
}

void QBasicDrag::recreateShapedPixmapWindow(QScreen *screen, const QPoint &pos)
{
    delete m_drag_icon_window;
    // ### TODO Check if its really necessary to have m_drag_icon_window
    // when QDrag is used without a pixmap - QDrag::setPixmap()
    m_drag_icon_window = new QShapedPixmapWindow(screen);

    m_drag_icon_window->setUseCompositing(m_useCompositing);
    m_drag_icon_window->setPixmap(m_drag->pixmap());
    m_drag_icon_window->setHotspot(m_drag->hotSpot());
    m_drag_icon_window->updateGeometry(pos);
    m_drag_icon_window->setVisible(true);
}

void QBasicDrag::cancel()
{
    disableEventFilter();
    restoreCursor();
    m_drag_icon_window->setVisible(false);
}

/*!
  Move the drag label to \a globalPos, which is
  interpreted in device independent coordinates. Typically called from reimplementations of move().
 */

void QBasicDrag::moveShapedPixmapWindow(const QPoint &globalPos)
{
    if (m_drag)
        m_drag_icon_window->updateGeometry(globalPos);
}

void QBasicDrag::drop(const QPoint &)
{
    disableEventFilter();
    restoreCursor();
    m_drag_icon_window->setVisible(false);
}

void  QBasicDrag::exitDndEventLoop()
{
    if (m_eventLoop && m_eventLoop->isRunning())
        m_eventLoop->exit();
}

void QBasicDrag::updateCursor(Qt::DropAction action)
{
#ifndef QT_NO_CURSOR
    Qt::CursorShape cursorShape = Qt::ForbiddenCursor;
    if (canDrop()) {
        switch (action) {
        case Qt::CopyAction:
            cursorShape = Qt::DragCopyCursor;
            break;
        case Qt::LinkAction:
            cursorShape = Qt::DragLinkCursor;
            break;
        default:
            cursorShape = Qt::DragMoveCursor;
            break;
        }
    }

    QCursor *cursor = QGuiApplication::overrideCursor();
    QPixmap pixmap = m_drag->dragCursor(action);
    if (!cursor) {
        QGuiApplication::changeOverrideCursor((pixmap.isNull()) ? QCursor(cursorShape) : QCursor(pixmap));
    } else {
        if (!pixmap.isNull()) {
            if ((cursor->pixmap().cacheKey() != pixmap.cacheKey())) {
                QGuiApplication::changeOverrideCursor(QCursor(pixmap));
            }
        } else {
            if (cursorShape != cursor->shape()) {
                QGuiApplication::changeOverrideCursor(QCursor(cursorShape));
            }
        }
    }
#endif
    updateAction(action);
}

/*!
    \class QSimpleDrag
    \brief QSimpleDrag implements QBasicDrag for Drag and Drop operations within the Qt Application itself.
    \since 5.0
    \internal
    \ingroup qpa

    The class checks whether the receiving window is a window of the Qt application
    and sets the state accordingly. It does not take windows of other applications
    into account.
*/

QSimpleDrag::QSimpleDrag() : m_current_window(0)
{
}

QMimeData *QSimpleDrag::platformDropData()
{
    if (drag())
        return drag()->mimeData();
    return 0;
}

void QSimpleDrag::startDrag()
{
    QBasicDrag::startDrag();
    m_current_window = topLevelAt(QCursor::pos());
    if (m_current_window) {
        QPlatformDragQtResponse response = QWindowSystemInterface::handleDrag(m_current_window, drag()->mimeData(), QCursor::pos(), drag()->supportedActions());
        setCanDrop(response.isAccepted());
        updateCursor(response.acceptedAction());
    } else {
        setCanDrop(false);
        updateCursor(Qt::IgnoreAction);
    }
    setExecutedDropAction(Qt::IgnoreAction);
}

void QSimpleDrag::cancel()
{
    QBasicDrag::cancel();
    if (drag() && m_current_window) {
        QWindowSystemInterface::handleDrag(m_current_window, 0, QPoint(), Qt::IgnoreAction);
        m_current_window = 0;
    }
}

void QSimpleDrag::move(const QPoint &globalPos)
{
    //### not high-DPI aware
    moveShapedPixmapWindow(globalPos);
    QWindow *window = topLevelAt(globalPos);
    if (!window)
        return;

    const QPoint pos = globalPos - window->geometry().topLeft();
    const QPlatformDragQtResponse qt_response =
        QWindowSystemInterface::handleDrag(window, drag()->mimeData(), pos, drag()->supportedActions());

    updateCursor(qt_response.acceptedAction());
    setCanDrop(qt_response.isAccepted());
}

void QSimpleDrag::drop(const QPoint &globalPos)
{
    //### not high-DPI aware

    QBasicDrag::drop(globalPos);
    QWindow *window = topLevelAt(globalPos);
    if (!window)
        return;

    const QPoint pos = globalPos - window->geometry().topLeft();
    const QPlatformDropQtResponse response =
            QWindowSystemInterface::handleDrop(window, drag()->mimeData(),pos, drag()->supportedActions());
    if (response.isAccepted()) {
        setExecutedDropAction(response.acceptedAction());
    } else {
        setExecutedDropAction(Qt::IgnoreAction);
    }
}

#endif // QT_NO_DRAGANDDROP

QT_END_NAMESPACE
