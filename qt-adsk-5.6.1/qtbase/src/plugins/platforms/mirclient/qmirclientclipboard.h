/****************************************************************************
**
** Copyright (C) 2014 Canonical, Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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


#ifndef QMIRCLIENTCLIPBOARD_H
#define QMIRCLIENTCLIPBOARD_H

#include <qpa/qplatformclipboard.h>

#include <QMimeData>
#include <QPointer>
class QDBusInterface;
class QDBusPendingCallWatcher;

class QMirClientClipboard : public QObject, public QPlatformClipboard
{
    Q_OBJECT
public:
    QMirClientClipboard();
    virtual ~QMirClientClipboard();

    // QPlatformClipboard methods.
    QMimeData* mimeData(QClipboard::Mode mode = QClipboard::Clipboard) override;
    void setMimeData(QMimeData* data, QClipboard::Mode mode = QClipboard::Clipboard) override;
    bool supportsMode(QClipboard::Mode mode) const override;
    bool ownsMode(QClipboard::Mode mode) const override;

    void requestDBusClipboardContents();

private Q_SLOTS:
    void onDBusClipboardGetContentsFinished(QDBusPendingCallWatcher*);
    void onDBusClipboardSetContentsFinished(QDBusPendingCallWatcher*);
    void updateMimeData(const QByteArray &serializedMimeData);

private:
    void setupDBus();

    QByteArray serializeMimeData(QMimeData *mimeData) const;
    QMimeData *deserializeMimeData(const QByteArray &serializedMimeData) const;

    void setDBusClipboardContents(const QByteArray &clipboardContents);

    QMimeData *mMimeData;
    bool mIsOutdated;

    QPointer<QDBusInterface> mDBusClipboard;

    QPointer<QDBusPendingCallWatcher> mPendingGetContentsCall;
    QPointer<QDBusPendingCallWatcher> mPendingSetContentsCall;

    bool mUpdatesDisabled;
    bool mDBusSetupDone;
};

#endif // QMIRCLIENTCLIPBOARD_H
