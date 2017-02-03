/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the qmake application of the Qt Toolkit.
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

#include <QtCore/qdir.h>
#include <QtCore/qxmlstream.h>

#include "epocroot_p.h"
#include "../windows/registry_p.h"

QT_BEGIN_NAMESPACE

// Registry key under which the location of the Symbian devices.xml file is
// stored.
// Note that, on 64-bit machines, this key is located under the 32-bit
// compatibility key:
//     HKEY_LOCAL_MACHINE\Software\Wow6432Node
#define SYMBIAN_SDKS_REG_SUBKEY "Software\\Symbian\\EPOC SDKs\\CommonPath"

#ifdef Q_OS_WIN32
#   define SYMBIAN_SDKS_REG_HANDLE HKEY_LOCAL_MACHINE
#else
#   define SYMBIAN_SDKS_REG_HANDLE 0
#endif

// Value which is populated and returned by the epocRoot() function.
// Stored as a static value in order to avoid unnecessary re-evaluation.
static QString epocRootValue;

static QString getDevicesXmlPath()
    {
    // Note that the following call will return a null string on platforms other
    // than Windows.  If support is required on other platforms for devices.xml,
    // an alternative mechanism for retrieving the location of this file will
    // be required.
    return qt_readRegistryKey(SYMBIAN_SDKS_REG_HANDLE, QLatin1String(SYMBIAN_SDKS_REG_SUBKEY));
    }

/**
 * Checks whether epocRootValue points to an existent directory.
 * If not, epocRootValue is set to an empty string and an error message is printed.
 */
static void checkEpocRootExists(const QString &source)
{
    if (!epocRootValue.isEmpty()) {
        QDir dir(epocRootValue);
        if (!dir.exists()) {
            qWarning("Warning: %s is set to an invalid path: '%s'", qPrintable(source),
                     qPrintable(epocRootValue));
            epocRootValue = QString();
        }
    }
}

/**
 * Translate path from Windows to Qt format.
 */
static void fixEpocRoot(QString &path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    if (!path.size() || path[path.size()-1] != QLatin1Char('/')) {
        path += QLatin1Char('/');
    }
#ifdef Q_OS_WIN32
    // Make sure we have drive letter in epocroot
    if (path.startsWith(QLatin1Char('/')))
        path.prepend(QDir::currentPath().left(2));
#endif
}

/**
 * Determine the epoc root for the currently active SDK.
 */
QString qt_epocRoot()
{
    if (epocRootValue.isEmpty()) {
        // 1. If environment variable EPOCROOT is set and points to an existent
        //    directory, this is returned.
        epocRootValue = QString::fromLocal8Bit(qgetenv("EPOCROOT").constData());
        checkEpocRootExists(QLatin1String("EPOCROOT environment variable"));

        if (epocRootValue.isEmpty()) {
            // 2. The location of devices.xml is specified by a registry key.  If this
            //    file exists, it is parsed.
            QString devicesXmlPath = getDevicesXmlPath();
            if (!devicesXmlPath.isEmpty()) {
                devicesXmlPath += QLatin1String("/devices.xml");
                QFile devicesFile(devicesXmlPath);
                if (devicesFile.open(QIODevice::ReadOnly)) {

                    // 3. If the EPOCDEVICE environment variable is set and a corresponding
                    //    entry is found in devices.xml, and its epocroot value points to an
                    //    existent directory, it is returned.
                    // 4. If a device element marked as default is found in devices.xml and its
                    //    epocroot value points to an existent directory, this is returned.

                    const QString epocDeviceValue = QString::fromLocal8Bit(qgetenv("EPOCDEVICE").constData());
                    bool epocDeviceFound = false;

                    QXmlStreamReader xml(&devicesFile);
                    while (!xml.atEnd()) {
                        xml.readNext();
                        if (xml.isStartElement() && xml.name() == QLatin1String("devices")) {
                            if (xml.attributes().value(QLatin1String("version")) == QLatin1String("1.0")) {
                                while (!(xml.isEndElement() && xml.name() == QLatin1String("devices")) && !xml.atEnd()) {
                                    xml.readNext();
                                    if (xml.isStartElement() && xml.name() == QLatin1String("device")) {
                                        const bool isDefault = xml.attributes().value(QLatin1String("default")) == QLatin1String("yes");
                                        const QString id = xml.attributes().value(QLatin1String("id")).toString();
                                        const QString name = xml.attributes().value(QLatin1String("name")).toString();
                                        const QString alias = xml.attributes().value(QLatin1String("alias")).toString();
                                        bool epocDeviceMatch = QString(id + QLatin1String(":") + name) == epocDeviceValue;
                                        if (!alias.isEmpty())
                                            epocDeviceMatch |= alias == epocDeviceValue;
                                        epocDeviceFound |= epocDeviceMatch;

                                        if((epocDeviceValue.isEmpty() && isDefault) || epocDeviceMatch) {
                                            // Found a matching device
                                            while (!(xml.isEndElement() && xml.name() == QLatin1String("device")) && !xml.atEnd()) {
                                                xml.readNext();
                                                if (xml.isStartElement() && xml.name() == QLatin1String("epocroot")) {
                                                    epocRootValue = xml.readElementText();
                                                    const QString deviceSource = epocDeviceValue.isEmpty()
                                                        ? QLatin1String("default device")
                                                        : QString(QLatin1String("EPOCDEVICE (") + epocDeviceValue + QLatin1String(")"));
                                                    checkEpocRootExists(deviceSource);
                                                }
                                            }

                                            if (epocRootValue.isEmpty())
                                                xml.raiseError(QLatin1String("No epocroot element found"));
                                        }
                                    }
                                }
                            } else {
                                xml.raiseError(QLatin1String("Invalid 'devices' element version"));
                            }
                        }
                    }
                    if (xml.hasError()) {
                        qWarning("Warning: Error \"%s\" when parsing devices.xml",
                                 qPrintable(xml.errorString()));
                    } else {
                        if (epocRootValue.isEmpty()) {
                            if (!epocDeviceValue.isEmpty()) {
                                if (epocDeviceFound) {
                                    qWarning("Warning: Missing or invalid epocroot attribute in device '%s' in devices.xml.",
                                             qPrintable(epocDeviceValue));
                                } else {
                                    qWarning("Warning: No device matching EPOCDEVICE (%s) in devices.xml.",
                                             qPrintable(epocDeviceValue));
                                }
                            } else {
                                if (epocDeviceFound) {
                                    qWarning("Warning: Missing or invalid epocroot attribute in default device in devices.xml.");
                                } else {
                                    qWarning("Warning: No default device set in devices.xml.");
                                }
                            }
                        }
                    }
                } else {
                    qWarning("Warning: Could not open file: '%s'.", qPrintable(devicesXmlPath));
                }
            }
        }

        if (epocRootValue.isEmpty()) {
            // 5. An empty string is returned.
            qWarning("Warning: failed to resolve epocroot."
#ifdef Q_OS_WIN32
                     "\nEither\n"
                     "    1. Set EPOCROOT environment variable to a valid value.\n"
                     " or 2. Ensure that the HKEY_LOCAL_MACHINE\\" SYMBIAN_SDKS_REG_SUBKEY
                     " registry key is set, and then\n"
                     "       a. Set EPOCDEVICE environment variable to a valid device\n"
                     "    or b. Specify a default device in the devices.xml file.");
#else
                     " Set EPOCROOT environment variable to a valid value.");
#endif
        } else {
            fixEpocRoot(epocRootValue);
        }
    }

    return epocRootValue;
}

QT_END_NAMESPACE
