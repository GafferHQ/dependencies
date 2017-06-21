/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef RENDER_WIDGET_HOST_VIEW_QT_DELEGATE_H
#define RENDER_WIDGET_HOST_VIEW_QT_DELEGATE_H

#include "qtwebenginecoreglobal.h"

#include <QRect>
#include <QtGui/qwindowdefs.h>

QT_BEGIN_NAMESPACE
class QCursor;
class QEvent;
class QPainter;
class QSGImageNode;
class QSGLayer;
class QSGNode;
class QSGTexture;
class QVariant;
class QWindow;
class QInputMethodEvent;
QT_END_NAMESPACE

namespace QtWebEngineCore {

class WebContentsAdapterClient;

class QWEBENGINE_EXPORT RenderWidgetHostViewQtDelegateClient {
public:
    virtual ~RenderWidgetHostViewQtDelegateClient() { }
    virtual QSGNode *updatePaintNode(QSGNode *) = 0;
    virtual void notifyResize() = 0;
    virtual void notifyShown() = 0;
    virtual void notifyHidden() = 0;
    virtual void windowBoundsChanged() = 0;
    virtual void windowChanged() = 0;
    virtual bool forwardEvent(QEvent *) = 0;
    virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const = 0;
};

class QWEBENGINE_EXPORT RenderWidgetHostViewQtDelegate {
public:
    virtual ~RenderWidgetHostViewQtDelegate() { }
    virtual void initAsChild(WebContentsAdapterClient*) = 0;
    virtual void initAsPopup(const QRect&) = 0;
    virtual QRectF screenRect() const = 0;
    virtual QRectF contentsRect() const = 0;
    virtual void setKeyboardFocus() = 0;
    virtual bool hasKeyboardFocus() = 0;
    virtual void lockMouse() = 0;
    virtual void unlockMouse() = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual bool isVisible() const = 0;
    virtual QWindow* window() const = 0;
    virtual QSGTexture *createTextureFromImage(const QImage &) = 0;
    virtual QSGLayer *createLayer() = 0;
    virtual QSGImageNode *createImageNode() = 0;
    virtual void update() = 0;
    virtual void updateCursor(const QCursor &) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void move(const QPoint &) = 0;
    virtual void inputMethodStateChanged(bool editorVisible) = 0;
    virtual void setTooltip(const QString &) = 0;
    virtual void setClearColor(const QColor &color) = 0;
};

} // namespace QtWebEngineCore

#endif // RENDER_WIDGET_HOST_VIEW_QT_DELEGATE_H
