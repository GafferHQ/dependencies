// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/usb_midi_device_android.h"


#include "base/android/jni_array.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "jni/UsbMidiDeviceAndroid_jni.h"
#include "media/midi/usb_midi_descriptor_parser.h"

namespace media {
namespace midi {

UsbMidiDeviceAndroid::UsbMidiDeviceAndroid(ObjectRef raw_device,
                                           UsbMidiDeviceDelegate* delegate)
    : raw_device_(raw_device), delegate_(delegate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UsbMidiDeviceAndroid_registerSelf(env, raw_device_.obj(),
                                         reinterpret_cast<jlong>(this));

  GetDescriptorsInternal();
  InitDeviceInfo();
}

UsbMidiDeviceAndroid::~UsbMidiDeviceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UsbMidiDeviceAndroid_close(env, raw_device_.obj());
}

std::vector<uint8> UsbMidiDeviceAndroid::GetDescriptors() {
  return descriptors_;
}

std::string UsbMidiDeviceAndroid::GetManufacturer() {
  return manufacturer_;
}

std::string UsbMidiDeviceAndroid::GetProductName() {
  return product_;
}

std::string UsbMidiDeviceAndroid::GetDeviceVersion() {
  return device_version_;
}

void UsbMidiDeviceAndroid::Send(int endpoint_number,
                                const std::vector<uint8>& data) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const uint8* head = data.size() ? &data[0] : NULL;
  ScopedJavaLocalRef<jbyteArray> data_to_pass =
      base::android::ToJavaByteArray(env, head, data.size());

  Java_UsbMidiDeviceAndroid_send(env, raw_device_.obj(), endpoint_number,
                                 data_to_pass.obj());
}

void UsbMidiDeviceAndroid::OnData(JNIEnv* env,
                                  jobject caller,
                                  jint endpoint_number,
                                  jbyteArray data) {
  std::vector<uint8> bytes;
  base::android::JavaByteArrayToByteVector(env, data, &bytes);

  const uint8* head = bytes.size() ? &bytes[0] : NULL;
  delegate_->ReceiveUsbMidiData(this, endpoint_number, head, bytes.size(),
                                base::TimeTicks::Now());
}

bool UsbMidiDeviceAndroid::RegisterUsbMidiDevice(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void UsbMidiDeviceAndroid::GetDescriptorsInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> descriptors =
      Java_UsbMidiDeviceAndroid_getDescriptors(env, raw_device_.obj());

  base::android::JavaByteArrayToByteVector(env, descriptors.obj(),
                                           &descriptors_);
}

void UsbMidiDeviceAndroid::InitDeviceInfo() {
  UsbMidiDescriptorParser parser;
  UsbMidiDescriptorParser::DeviceInfo info;

  const uint8* data = descriptors_.size() > 0 ? &descriptors_[0] : nullptr;

  if (!parser.ParseDeviceInfo(data, descriptors_.size(), &info)) {
    // We don't report the error here. If it is critical, we will realize it
    // when we parse the descriptors again for ports.
    manufacturer_ = "invalid descriptor";
    product_ = "invalid descriptor";
    device_version_ = "invalid descriptor";
    return;
  }

  manufacturer_ =
      GetString(info.manufacturer_index,
                base::StringPrintf("(vendor id = 0x%04x)", info.vendor_id));
  product_ =
      GetString(info.product_index,
                base::StringPrintf("(product id = 0x%04x)", info.product_id));
  device_version_ = info.BcdVersionToString(info.bcd_device_version);
}

std::vector<uint8> UsbMidiDeviceAndroid::GetStringDescriptor(int index) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> descriptors =
      Java_UsbMidiDeviceAndroid_getStringDescriptor(env, raw_device_.obj(),
                                                    index);

  std::vector<uint8> ret;
  base::android::JavaByteArrayToByteVector(env, descriptors.obj(), &ret);
  return ret;
}

std::string UsbMidiDeviceAndroid::GetString(int index,
                                            const std::string& backup) {
  const uint8 DESCRIPTOR_TYPE_STRING = 3;

  if (!index) {
    // index 0 means there is no such descriptor.
    return backup;
  }
  std::vector<uint8> descriptor = GetStringDescriptor(index);
  if (descriptor.size() < 2 || descriptor.size() < descriptor[0] ||
      descriptor[1] != DESCRIPTOR_TYPE_STRING) {
    // |descriptor| is not a valid string descriptor.
    return backup;
  }
  size_t size = descriptor[0];
  std::string encoded(reinterpret_cast<char*>(&descriptor[0]) + 2, size - 2);
  std::string result;
  // Unicode ECN specifies that the string is encoded in UTF-16LE.
  if (!base::ConvertToUtf8AndNormalize(encoded, "utf-16le", &result))
    return backup;
  return result;
}

}  // namespace midi
}  // namespace media
