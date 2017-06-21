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

#ifndef QXCBCLIPBOARD_H
#define QXCBCLIPBOARD_H

#include <qpa/qplatformclipboard.h>
#include <qxcbobject.h>
#include <xcb/xcb.h>
#include <xcb/xfixes.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_CLIPBOARD

class QXcbConnection;
class QXcbScreen;
class QXcbClipboardMime;

class QXcbClipboard : public QXcbObject, public QPlatformClipboard
{
public:
    QXcbClipboard(QXcbConnection *connection);
    ~QXcbClipboard();

    QMimeData *mimeData(QClipboard::Mode mode) Q_DECL_OVERRIDE;
    void setMimeData(QMimeData *data, QClipboard::Mode mode) Q_DECL_OVERRIDE;

    bool supportsMode(QClipboard::Mode mode) const Q_DECL_OVERRIDE;
    bool ownsMode(QClipboard::Mode mode) const Q_DECL_OVERRIDE;

    QXcbScreen *screen() const;

    xcb_window_t requestor() const;
    void setRequestor(xcb_window_t window);

    xcb_window_t owner() const;

    void handleSelectionRequest(xcb_selection_request_event_t *event);
    void handleSelectionClearRequest(xcb_selection_clear_event_t *event);
    void handleXFixesSelectionRequest(xcb_xfixes_selection_notify_event_t *event);

    bool clipboardReadProperty(xcb_window_t win, xcb_atom_t property, bool deleteProperty, QByteArray *buffer, int *size, xcb_atom_t *type, int *format);
    QByteArray clipboardReadIncrementalProperty(xcb_window_t win, xcb_atom_t property, int nbytes, bool nullterm);

    QByteArray getDataInFormat(xcb_atom_t modeAtom, xcb_atom_t fmtatom);

    void setProcessIncr(bool process) { m_incr_active = process; }
    bool processIncr() { return m_incr_active; }
    void incrTransactionPeeker(xcb_generic_event_t *ge, bool &accepted);

    xcb_window_t getSelectionOwner(xcb_atom_t atom) const;
    QByteArray getSelection(xcb_atom_t selection, xcb_atom_t target, xcb_atom_t property, xcb_timestamp_t t = 0);

private:
    xcb_generic_event_t *waitForClipboardEvent(xcb_window_t win, int type, int timeout, bool checkManager = false);

    xcb_atom_t sendTargetsSelection(QMimeData *d, xcb_window_t window, xcb_atom_t property);
    xcb_atom_t sendSelection(QMimeData *d, xcb_atom_t target, xcb_window_t window, xcb_atom_t property);

    xcb_atom_t atomForMode(QClipboard::Mode mode) const;
    QClipboard::Mode modeForAtom(xcb_atom_t atom) const;

    // Selection and Clipboard
    QXcbClipboardMime *m_xClipboard[2];
    QMimeData *m_clientClipboard[2];
    xcb_timestamp_t m_timestamp[2];

    xcb_window_t m_requestor;
    xcb_window_t m_owner;

    static const int clipboard_timeout;

    bool m_incr_active;
    bool m_clipboard_closing;
    xcb_timestamp_t m_incr_receive_time;
};

#endif // QT_NO_CLIPBOARD

QT_END_NAMESPACE

#endif // QXCBCLIPBOARD_H
