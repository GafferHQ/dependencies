// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vr/vr_device_manager.h"

#include "base/memory/singleton.h"
#include "third_party/WebKit/public/platform/modules/vr/WebVR.h"

namespace content {

namespace {
VRDeviceManager* g_vr_device_manager = nullptr;
}

VRDeviceManager::VRDeviceManager()
    : vr_initialized_(false), keep_alive_(false) {
  bindings_.set_error_handler(this);
// Register VRDeviceProviders for the current platform
}

VRDeviceManager::VRDeviceManager(scoped_ptr<VRDeviceProvider> provider)
    : vr_initialized_(false), keep_alive_(true) {
  thread_checker_.DetachFromThread();
  RegisterProvider(provider.Pass());
  SetInstance(this);
}

VRDeviceManager::~VRDeviceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  g_vr_device_manager = nullptr;
}

void VRDeviceManager::BindRequest(mojo::InterfaceRequest<VRService> request) {
  VRDeviceManager* device_manager = GetInstance();
  device_manager->bindings_.AddBinding(device_manager, request.Pass());
}

void VRDeviceManager::OnConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (bindings_.empty() && !keep_alive_) {
    // Delete the device manager when it has no active connections.
    delete g_vr_device_manager;
  }
}

VRDeviceManager* VRDeviceManager::GetInstance() {
  if (!g_vr_device_manager)
    g_vr_device_manager = new VRDeviceManager();
  return g_vr_device_manager;
}

void VRDeviceManager::SetInstance(VRDeviceManager* instance) {
  // Unit tests can create multiple instances but only one should exist at any
  // given time so g_vr_device_manager should only go from nullptr to
  // non-nullptr and vica versa.
  CHECK_NE(!!instance, !!g_vr_device_manager);
  g_vr_device_manager = instance;
}

bool VRDeviceManager::HasInstance() {
  // For testing. Checks to see if a VRDeviceManager instance is active.
  return !!g_vr_device_manager;
}

mojo::Array<VRDeviceInfoPtr> VRDeviceManager::GetVRDevices() {
  DCHECK(thread_checker_.CalledOnValidThread());

  InitializeProviders();

  std::vector<VRDevice*> devices;
  for (const auto& provider : providers_)
    provider->GetDevices(devices);

  mojo::Array<VRDeviceInfoPtr> out_devices(0);
  for (const auto& device : devices) {
    if (device->id() == VR_DEVICE_LAST_ID)
      continue;

    if (devices_.find(device->id()) == devices_.end())
      devices_[device->id()] = device;

    VRDeviceInfoPtr vr_device_info = device->GetVRDevice();
    if (vr_device_info.is_null())
      continue;

    vr_device_info->index = device->id();
    out_devices.push_back(vr_device_info.Pass());
  }

  return out_devices;
}

VRDevice* VRDeviceManager::GetDevice(unsigned int index) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DeviceMap::iterator iter = devices_.find(index);
  if (iter == devices_.end()) {
    return nullptr;
  }
  return iter->second;
}

void VRDeviceManager::InitializeProviders() {
  if (vr_initialized_) {
    return;
  }

  for (const auto& provider : providers_)
    provider->Initialize();

  vr_initialized_ = true;
}

void VRDeviceManager::RegisterProvider(scoped_ptr<VRDeviceProvider> provider) {
  providers_.push_back(make_linked_ptr(provider.release()));
}

void VRDeviceManager::GetDevices(const GetDevicesCallback& callback) {
  callback.Run(GetVRDevices());
}

void VRDeviceManager::GetSensorState(uint32_t index,
                                     const GetSensorStateCallback& callback) {
  VRDevice* device = GetDevice(index);
  if (device) {
    callback.Run(device->GetSensorState());
  } else {
    callback.Run(nullptr);
  }
}

void VRDeviceManager::ResetSensor(uint32_t index) {
  VRDevice* device = GetDevice(index);
  if (device)
    device->ResetSensor();
}

}  // namespace content
