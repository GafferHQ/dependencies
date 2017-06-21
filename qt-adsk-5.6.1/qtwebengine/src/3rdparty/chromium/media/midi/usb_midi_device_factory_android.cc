// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/usb_midi_device_factory_android.h"


#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "jni/UsbMidiDeviceFactoryAndroid_jni.h"
#include "media/midi/usb_midi_device_android.h"

namespace media {
namespace midi {

namespace {

typedef UsbMidiDevice::Factory::Callback Callback;

}  // namespace

UsbMidiDeviceFactoryAndroid::UsbMidiDeviceFactoryAndroid() : delegate_(NULL) {}

UsbMidiDeviceFactoryAndroid::~UsbMidiDeviceFactoryAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!raw_factory_.is_null())
    Java_UsbMidiDeviceFactoryAndroid_close(
        env, raw_factory_.obj(), base::android::GetApplicationContext());
}

void UsbMidiDeviceFactoryAndroid::EnumerateDevices(
    UsbMidiDeviceDelegate* delegate,
    Callback callback) {
  DCHECK(!delegate_);
  JNIEnv* env = base::android::AttachCurrentThread();
  uintptr_t pointer = reinterpret_cast<uintptr_t>(this);
  raw_factory_.Reset(Java_UsbMidiDeviceFactoryAndroid_create(
      env, base::android::GetApplicationContext(), pointer));

  delegate_ = delegate;
  callback_ = callback;

  if (Java_UsbMidiDeviceFactoryAndroid_enumerateDevices(
          env, raw_factory_.obj(), base::android::GetApplicationContext())) {
    // Asynchronous operation.
    return;
  }
  // No devices are found.
  ScopedVector<UsbMidiDevice> devices;
  callback.Run(true, &devices);
}

// Called from the Java world.
void UsbMidiDeviceFactoryAndroid::OnUsbMidiDeviceRequestDone(
    JNIEnv* env,
    jobject caller,
    jobjectArray devices) {
  size_t size = env->GetArrayLength(devices);
  ScopedVector<UsbMidiDevice> devices_to_pass;
  for (size_t i = 0; i < size; ++i) {
    UsbMidiDeviceAndroid::ObjectRef raw_device(
        env, env->GetObjectArrayElement(devices, i));
    devices_to_pass.push_back(new UsbMidiDeviceAndroid(raw_device, delegate_));
  }

  callback_.Run(true, &devices_to_pass);
}

// Called from the Java world.
void UsbMidiDeviceFactoryAndroid::OnUsbMidiDeviceAttached(
    JNIEnv* env,
    jobject caller,
    jobject device) {
  UsbMidiDeviceAndroid::ObjectRef raw_device(env, device);
  delegate_->OnDeviceAttached(
      scoped_ptr<UsbMidiDevice>(
          new UsbMidiDeviceAndroid(raw_device, delegate_)));
}

// Called from the Java world.
void UsbMidiDeviceFactoryAndroid::OnUsbMidiDeviceDetached(
    JNIEnv* env,
    jobject caller,
    jint index) {
  delegate_->OnDeviceDetached(index);
}

bool UsbMidiDeviceFactoryAndroid::RegisterUsbMidiDeviceFactory(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace midi
}  // namespace media
