/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#import <UIKit/UIDevice.h>

#include "iosproximitysensor.h"

char const * const IOSProximitySensor::id("ios.proximitysensor");

QT_BEGIN_NAMESPACE

@interface ProximitySensorCallback : NSObject
{
    IOSProximitySensor *m_iosProximitySensor;
}
@end

@implementation ProximitySensorCallback

- (id)initWithQIOSProximitySensor:(IOSProximitySensor *)iosProximitySensor
{
    self = [super init];
    if (self) {
        m_iosProximitySensor = iosProximitySensor;
        [[NSNotificationCenter defaultCenter]
            addObserver:self
            selector:@selector(proximityChanged:)
            name:@"UIDeviceProximityStateDidChangeNotification" object:nil];
    }
    return self;
}

-(void) dealloc
{
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
        name:@"UIDeviceProximityStateDidChangeNotification" object:nil];
    [super dealloc];
}

- (void)proximityChanged:(NSNotificationCenter *)notification
{
    Q_UNUSED(notification);
    bool close = [[UIDevice currentDevice] proximityState] == YES;
    m_iosProximitySensor->proximityChanged(close);
}

@end

bool IOSProximitySensor::available()
{
    UIDevice *device = [UIDevice currentDevice];
    if (device.proximityMonitoringEnabled)
        return true;
    // According to the docs, you need to switch it on and
    // re-read the property to check if it is available:
    device.proximityMonitoringEnabled = YES;
    bool available = device.proximityMonitoringEnabled;
    device.proximityMonitoringEnabled = NO;
    return available;
}

IOSProximitySensor::IOSProximitySensor(QSensor *sensor)
    : QSensorBackend(sensor)
    , m_proximitySensorCallback(0)
{
    setReading<QProximityReading>(&m_reading);
}

IOSProximitySensor::~IOSProximitySensor()
{
    [m_proximitySensorCallback release];
}

void IOSProximitySensor::start()
{
    m_proximitySensorCallback = [[ProximitySensorCallback alloc] initWithQIOSProximitySensor:this];
    [UIDevice currentDevice].proximityMonitoringEnabled = YES;
}

void IOSProximitySensor::proximityChanged(bool close)
{
    m_reading.setClose(close);
    m_reading.setTimestamp(quint64([[NSDate date] timeIntervalSinceReferenceDate] * 1e6));
    newReadingAvailable();
}

void IOSProximitySensor::stop()
{
    [UIDevice currentDevice].proximityMonitoringEnabled = NO;
    [m_proximitySensorCallback release];
    m_proximitySensorCallback = 0;
}

QT_END_NAMESPACE
