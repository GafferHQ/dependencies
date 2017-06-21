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

#ifndef QWINDOWSDRAG_H
#define QWINDOWSDRAG_H

#include "qwindowsinternalmimedata.h"

#include <qpa/qplatformdrag.h>
#include <QtGui/QPixmap>

struct IDropTargetHelper;

QT_BEGIN_NAMESPACE

class QPlatformScreen;

class QWindowsDropMimeData : public QWindowsInternalMimeData {
public:
    QWindowsDropMimeData() {}
    IDataObject *retrieveDataObject() const Q_DECL_OVERRIDE;
};

class QWindowsOleDropTarget : public IDropTarget
{
public:
    explicit QWindowsOleDropTarget(QWindow *w);
    virtual ~QWindowsOleDropTarget();

    // IUnknown methods
    STDMETHOD(QueryInterface)(REFIID riid, void FAR* FAR* ppvObj);
    STDMETHOD_(ULONG, AddRef)(void);
    STDMETHOD_(ULONG, Release)(void);

    // IDropTarget methods
    STDMETHOD(DragEnter)(LPDATAOBJECT pDataObj, DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);
    STDMETHOD(DragOver)(DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);
    STDMETHOD(DragLeave)();
    STDMETHOD(Drop)(LPDATAOBJECT pDataObj, DWORD grfKeyState, POINTL pt, LPDWORD pdwEffect);

private:
    void handleDrag(QWindow *window, DWORD grfKeyState, const QPoint &, LPDWORD pdwEffect);

    ULONG m_refs;
    QWindow *const m_window;
    QRect m_answerRect;
    QPoint m_lastPoint;
    DWORD m_chosenEffect;
    DWORD m_lastKeyState;
};

class QWindowsDrag : public QPlatformDrag
{
public:
    QWindowsDrag();
    virtual ~QWindowsDrag();

    QMimeData *platformDropData() Q_DECL_OVERRIDE { return &m_dropData; }

    Qt::DropAction drag(QDrag *drag) Q_DECL_OVERRIDE;

    static QWindowsDrag *instance();

    IDataObject *dropDataObject() const             { return m_dropDataObject; }
    void setDropDataObject(IDataObject *dataObject) { m_dropDataObject = dataObject; }
    void releaseDropDataObject();
    QMimeData *dropData();

    IDropTargetHelper* dropHelper();

private:
    QWindowsDropMimeData m_dropData;
    IDataObject *m_dropDataObject;

    IDropTargetHelper* m_cachedDropTargetHelper;
};

QT_END_NAMESPACE

#endif // QWINDOWSDRAG_H
