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

#ifndef QDBUSBINDING_H
#define QDBUSBINDING_H

#include <QtDBus/QtDBus>
#include <QtScript/qscriptable.h>
#include <QtScript/qscriptengine.h>

#ifndef QT_NO_DBUS

class QDBusConnectionConstructor : public QObject,
                                   public QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QScriptValue sessionBus READ sessionBus)
    Q_PROPERTY(QScriptValue systemBus READ systemBus)

public:
    QDBusConnectionConstructor(QScriptEngine *engine, QScriptValue extensionObject);

    QScriptValue sessionBus() const;
    QScriptValue systemBus() const;

public Q_SLOTS:
    QObject *qscript_call(const QString &name);

    void disconnectFromBus(const QString &name);
    QDBusConnection connectToBus(const QString &address, const QString &name);
    QDBusConnection connectToBus(QDBusConnection::BusType type, const QString &name);
};

class QScriptDBusConnection : public QObject,
                              public QScriptable
{
    Q_OBJECT
    Q_PROPERTY(QString baseService READ baseService)
    Q_PROPERTY(bool isConnected READ isConnected)
    Q_PROPERTY(QScriptValue dbusInterface READ dbusInterface)
public:
    QScriptDBusConnection(const QDBusConnection &conn, QObject *parent);

    inline QString baseService() const { return connection.baseService(); }
    inline bool isConnected() const { return connection.isConnected(); }
    QScriptValue dbusInterface() const;

    inline QDBusConnection dbusConnection() const { return connection; }

public Q_SLOTS:
    inline bool send(const QDBusMessage &message) const
    { return connection.send(message); }
    inline QDBusMessage call(const QDBusMessage &message, int callMode = QDBus::Block, int timeout = -1) const
    { return connection.call(message, QDBus::CallMode(callMode), timeout); }

    inline bool registerService(const QString &serviceName)
    { return connection.registerService(serviceName); }
    inline bool unregisterService(const QString &serviceName)
    { return connection.unregisterService(serviceName); }

    inline QDBusError lastError() const
    { return connection.lastError(); }

    inline void unregisterObject(const QString &path, QDBusConnection::UnregisterMode mode = QDBusConnection::UnregisterNode)
    { return connection.unregisterObject(path, mode); }
    inline QObject *objectRegisteredAt(const QString &path) const
    { return connection.objectRegisteredAt(path); }

#if 0
    bool callWithCallback(const QDBusMessage &message, QObject *receiver,
                          const char *slot, int timeout = -1) const;

    bool connect(const QString &service, const QString &path, const QString &interface,
                 const QString &name, QObject *receiver, const char *slot);
    bool disconnect(const QString &service, const QString &path, const QString &interface,
                    const QString &name, QObject *receiver, const char *slot);

    bool connect(const QString &service, const QString &path, const QString &interface,
                 const QString &name, const QString& signature,
                 QObject *receiver, const char *slot);
    bool disconnect(const QString &service, const QString &path, const QString &interface,
                    const QString &name, const QString& signature,
                    QObject *receiver, const char *slot);

    bool registerObject(const QString &path, QObject *object,
                        RegisterOptions options = ExportAdaptors);

#endif

private:
    QDBusConnection connection;
};

Q_DECLARE_METATYPE(QScriptDBusConnection*)

class QScriptDBusInterfaceConstructor : public QObject,
                                        public QScriptable
{
    Q_OBJECT
public:
    QScriptDBusInterfaceConstructor(QScriptEngine *engine, QScriptValue extensionObject);

public Q_SLOTS:
    QScriptValue qscript_call(const QString &service, const QString &path, const QString &interface = QString(),
                              const QScriptValue &conn = QScriptValue());
};

Q_DECLARE_METATYPE(QDBusMessage)

class QScriptDBusMessageConstructor : public QObject, public QScriptable
{
    Q_OBJECT
    Q_ENUMS(MessageType)
public:
    enum MessageType {
        InvalidMessage = QDBusMessage::InvalidMessage,
        MethodCallMessage = QDBusMessage::MethodCallMessage,
        ReplyMessage = QDBusMessage::ReplyMessage,
        ErrorMessage = QDBusMessage::ErrorMessage,
        SignalMessage = QDBusMessage::SignalMessage
    };

    QScriptDBusMessageConstructor(QScriptEngine *engine, QScriptValue extensionObject);

    QScriptValue protoType() const { return proto; }

public Q_SLOTS:
    QDBusMessage createSignal(const QString &path, const QString &interface, const QString &name);
    QDBusMessage createMethodCall(const QString &destination, const QString &path, const QString &interface, const QString &method);
    QDBusMessage createError(const QString &name, const QString &msg);

public:
    static QScriptValue createReply(QScriptContext *context, QScriptEngine *engine);
    static QScriptValue createErrorReply(QScriptContext *context, QScriptEngine *engine);

private:
    QScriptValue proto;
};

#endif // QT_NO_DBUS
#endif // QDBUSBINDING_H
