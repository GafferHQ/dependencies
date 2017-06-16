// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_SENSORS_INERTIAL_SENSOR_CONSTS_H_
#define CONTENT_BROWSER_DEVICE_SENSORS_INERTIAL_SENSOR_CONSTS_H_

#include "base/time/time.h"

namespace content {

// Constants related to the Device {Motion|Orientation|Light} APIs.

enum ConsumerType {
  CONSUMER_TYPE_MOTION = 1 << 0,
  CONSUMER_TYPE_ORIENTATION = 1 << 1,
  CONSUMER_TYPE_LIGHT = 1 << 2,
};

// Specifies the sampling rate for sensor data updates.
// Note that when changing this value it is desirable to have an adequate
// matching value |DeviceSensorEventPump::kDefaultPumpFrequencyHz| in
// content/renderer/device_orientation/device_sensor_event_pump.cc.
const int kInertialSensorSamplingRateHz = 60;
const int kInertialSensorIntervalMicroseconds =
    base::Time::kMicrosecondsPerSecond / kInertialSensorSamplingRateHz;

// Corresponding |kDefaultLightPumpFrequencyHz| is in
// content/renderer/device_sensors/device_light_event_pump.cc.
const int kLightSensorSamplingRateHz = 5;
const int kLightSensorIntervalMicroseconds =
    base::Time::kMicrosecondsPerSecond / kLightSensorSamplingRateHz;

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_SENSORS_INERTIAL_SENSOR_CONSTS_H_
