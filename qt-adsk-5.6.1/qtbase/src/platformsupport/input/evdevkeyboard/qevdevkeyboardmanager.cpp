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

#include "qevdevkeyboardmanager_p.h"

#include <QStringList>
#include <QCoreApplication>
#include <QLoggingCategory>

#include <private/qguiapplication_p.h>
#include <private/qinputdevicemanager_p_p.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(qLcEvdevKey)

QEvdevKeyboardManager::QEvdevKeyboardManager(const QString &key, const QString &specification, QObject *parent)
    : QObject(parent)
{
    Q_UNUSED(key);


    QString spec = QString::fromLocal8Bit(qgetenv("QT_QPA_EVDEV_KEYBOARD_PARAMETERS"));

    if (spec.isEmpty())
        spec = specification;

    QStringList args = spec.split(QLatin1Char(':'));
    QStringList devices;

    foreach (const QString &arg, args) {
        if (arg.startsWith(QLatin1String("/dev/"))) {
            // if device is specified try to use it
            devices.append(arg);
            args.removeAll(arg);
        }
    }

    // build new specification without /dev/ elements
    m_spec = args.join(QLatin1Char(':'));

    // add all keyboards for devices specified in the argument list
    foreach (const QString &device, devices)
        addKeyboard(device);

    if (devices.isEmpty()) {
        qCDebug(qLcEvdevKey) << "evdevkeyboard: Using device discovery";
        m_deviceDiscovery = QDeviceDiscovery::create(QDeviceDiscovery::Device_Keyboard, this);
        if (m_deviceDiscovery) {
            // scan and add already connected keyboards
            QStringList devices = m_deviceDiscovery->scanConnectedDevices();
            foreach (const QString &device, devices) {
                addKeyboard(device);
            }

            connect(m_deviceDiscovery, SIGNAL(deviceDetected(QString)), this, SLOT(addKeyboard(QString)));
            connect(m_deviceDiscovery, SIGNAL(deviceRemoved(QString)), this, SLOT(removeKeyboard(QString)));
        }
    }
}

QEvdevKeyboardManager::~QEvdevKeyboardManager()
{
    qDeleteAll(m_keyboards);
    m_keyboards.clear();
}

void QEvdevKeyboardManager::addKeyboard(const QString &deviceNode)
{
    qCDebug(qLcEvdevKey) << "Adding keyboard at" << deviceNode;
    QEvdevKeyboardHandler *keyboard;
    keyboard = QEvdevKeyboardHandler::create(deviceNode, m_spec, m_defaultKeymapFile);
    if (keyboard) {
        m_keyboards.insert(deviceNode, keyboard);
        QInputDeviceManagerPrivate::get(QGuiApplicationPrivate::inputDeviceManager())->setDeviceCount(
            QInputDeviceManager::DeviceTypeKeyboard, m_keyboards.count());
    } else {
        qWarning("Failed to open keyboard device %s", qPrintable(deviceNode));
    }
}

void QEvdevKeyboardManager::removeKeyboard(const QString &deviceNode)
{
    if (m_keyboards.contains(deviceNode)) {
        qCDebug(qLcEvdevKey) << "Removing keyboard at" << deviceNode;
        QEvdevKeyboardHandler *keyboard = m_keyboards.value(deviceNode);
        m_keyboards.remove(deviceNode);
        QInputDeviceManagerPrivate::get(QGuiApplicationPrivate::inputDeviceManager())->setDeviceCount(
            QInputDeviceManager::DeviceTypeKeyboard, m_keyboards.count());
        delete keyboard;
    }
}

void QEvdevKeyboardManager::loadKeymap(const QString &file)
{
    m_defaultKeymapFile = file;

    if (file.isEmpty()) {
        // Restore the default, which is either the built-in keymap or
        // the one given in the plugin spec.
        QString keymapFromSpec;
        foreach (const QString &arg, m_spec.split(QLatin1Char(':'))) {
            if (arg.startsWith(QLatin1String("keymap=")))
                keymapFromSpec = arg.mid(7);
        }
        foreach (QEvdevKeyboardHandler *handler, m_keyboards) {
            if (keymapFromSpec.isEmpty())
                handler->unloadKeymap();
            else
                handler->loadKeymap(keymapFromSpec);
        }
    } else {
        foreach (QEvdevKeyboardHandler *handler, m_keyboards)
            handler->loadKeymap(file);
    }
}

QT_END_NAMESPACE
