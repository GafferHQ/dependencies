/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtMultimedia module of the Qt Toolkit.
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


#ifndef QAUDIOENGINEPLUGIN_H
#define QAUDIOENGINEPLUGIN_H

#include <QtCore/qstring.h>
#include <QtCore/qplugin.h>
#include <QtCore/qfactoryinterface.h>

#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qaudiodeviceinfo.h>
#include <QtMultimedia/qaudioengine.h>


QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Multimedia)

struct Q_MULTIMEDIA_EXPORT QAudioEngineFactoryInterface : public QFactoryInterface
{
    virtual QList<QByteArray> availableDevices(QAudio::Mode) const = 0;
    virtual QAbstractAudioInput* createInput(const QByteArray& device, const QAudioFormat& format = QAudioFormat()) = 0;
    virtual QAbstractAudioOutput* createOutput(const QByteArray& device, const QAudioFormat& format = QAudioFormat()) = 0;
    virtual QAbstractAudioDeviceInfo* createDeviceInfo(const QByteArray& device, QAudio::Mode mode) = 0;
};

#define QAudioEngineFactoryInterface_iid \
    "com.nokia.qt.QAudioEngineFactoryInterface"
Q_DECLARE_INTERFACE(QAudioEngineFactoryInterface, QAudioEngineFactoryInterface_iid)

class Q_MULTIMEDIA_EXPORT QAudioEnginePlugin : public QObject, public QAudioEngineFactoryInterface
{
    Q_OBJECT
    Q_INTERFACES(QAudioEngineFactoryInterface:QFactoryInterface)

public:
    QAudioEnginePlugin(QObject *parent = 0);
    ~QAudioEnginePlugin();

    virtual QStringList keys() const = 0;
    virtual QList<QByteArray> availableDevices(QAudio::Mode) const = 0;
    virtual QAbstractAudioInput* createInput(const QByteArray& device, const QAudioFormat& format = QAudioFormat()) = 0;
    virtual QAbstractAudioOutput* createOutput(const QByteArray& device, const QAudioFormat& format = QAudioFormat()) = 0;
    virtual QAbstractAudioDeviceInfo* createDeviceInfo(const QByteArray& device, QAudio::Mode mode) = 0;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QAUDIOENGINEPLUGIN_H
