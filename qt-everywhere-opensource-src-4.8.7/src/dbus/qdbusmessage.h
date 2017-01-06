/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDBus module of the Qt Toolkit.
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

#ifndef QDBUSMESSAGE_H
#define QDBUSMESSAGE_H

#include <QtDBus/qdbusmacros.h>
#include <QtDBus/qdbuserror.h>
#include <QtCore/qlist.h>
#include <QtCore/qvariant.h>

#ifndef QT_NO_DBUS

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(DBus)

class QDBusMessagePrivate;
class Q_DBUS_EXPORT QDBusMessage
{
public:
    enum MessageType {
        InvalidMessage,
        MethodCallMessage,
        ReplyMessage,
        ErrorMessage,
        SignalMessage
    };

    QDBusMessage();
    QDBusMessage(const QDBusMessage &other);
    QDBusMessage &operator=(const QDBusMessage &other);
    ~QDBusMessage();

    static QDBusMessage createSignal(const QString &path, const QString &interface,
                                     const QString &name);
    static QDBusMessage createMethodCall(const QString &destination, const QString &path,
                                         const QString &interface, const QString &method);
    static QDBusMessage createError(const QString &name, const QString &msg);
    static inline QDBusMessage createError(const QDBusError &err)
    { return createError(err.name(), err.message()); }
    static inline QDBusMessage createError(QDBusError::ErrorType type, const QString &msg)
    { return createError(QDBusError::errorString(type), msg); }

    QDBusMessage createReply(const QList<QVariant> &arguments = QList<QVariant>()) const;
    inline QDBusMessage createReply(const QVariant &argument) const
    { return createReply(QList<QVariant>() << argument); }

    QDBusMessage createErrorReply(const QString name, const QString &msg) const;
    inline QDBusMessage createErrorReply(const QDBusError &err) const
    { return createErrorReply(err.name(), err.message()); }
    QDBusMessage createErrorReply(QDBusError::ErrorType type, const QString &msg) const;

    // there are no setters; if this changes, see qdbusmessage_p.h
    QString service() const;
    QString path() const;
    QString interface() const;
    QString member() const;
    QString errorName() const;
    QString errorMessage() const;
    MessageType type() const;
    QString signature() const;

    bool isReplyRequired() const;

    void setDelayedReply(bool enable) const;
    bool isDelayedReply() const;

    void setAutoStartService(bool enable);
    bool autoStartService() const;

    void setArguments(const QList<QVariant> &arguments);
    QList<QVariant> arguments() const;

    QDBusMessage &operator<<(const QVariant &arg);

private:
    friend class QDBusMessagePrivate;
    QDBusMessagePrivate *d_ptr;
};

#ifndef QT_NO_DEBUG_STREAM
Q_DBUS_EXPORT QDebug operator<<(QDebug, const QDBusMessage &);
#endif

QT_END_NAMESPACE

QT_END_HEADER

#endif // QT_NO_DBUS
#endif

