// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

namespace device {

BluetoothAdapter::ServiceOptions::ServiceOptions() {
}
BluetoothAdapter::ServiceOptions::~ServiceOptions() {
}

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS) && !defined(OS_MACOSX) && \
    !defined(OS_WIN)
//static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return base::WeakPtr<BluetoothAdapter>();
}
#endif  // !defined(OS_CHROMEOS) && !defined(OS_WIN) && !defined(OS_MACOSX)

base::WeakPtr<BluetoothAdapter> BluetoothAdapter::GetWeakPtrForTesting() {
  return weak_ptr_factory_.GetWeakPtr();
}

#if defined(OS_CHROMEOS)
void BluetoothAdapter::Shutdown() {
  NOTIMPLEMENTED();
}
#endif

void BluetoothAdapter::AddObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAdapter::RemoveObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void BluetoothAdapter::StartDiscoverySession(
    const DiscoverySessionCallback& callback,
    const ErrorCallback& error_callback) {
  StartDiscoverySessionWithFilter(nullptr, callback, error_callback);
}

void BluetoothAdapter::StartDiscoverySessionWithFilter(
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const DiscoverySessionCallback& callback,
    const ErrorCallback& error_callback) {
  BluetoothDiscoveryFilter* ptr = discovery_filter.get();
  AddDiscoverySession(ptr,
                      base::Bind(&BluetoothAdapter::OnStartDiscoverySession,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 base::Passed(&discovery_filter), callback),
                      error_callback);
}

scoped_ptr<BluetoothDiscoveryFilter>
BluetoothAdapter::GetMergedDiscoveryFilter() const {
  return GetMergedDiscoveryFilterHelper(nullptr, false);
}

scoped_ptr<BluetoothDiscoveryFilter>
BluetoothAdapter::GetMergedDiscoveryFilterMasked(
    BluetoothDiscoveryFilter* masked_filter) const {
  return GetMergedDiscoveryFilterHelper(masked_filter, true);
}

BluetoothAdapter::DeviceList BluetoothAdapter::GetDevices() {
  ConstDeviceList const_devices =
    const_cast<const BluetoothAdapter *>(this)->GetDevices();

  DeviceList devices;
  for (ConstDeviceList::const_iterator i = const_devices.begin();
       i != const_devices.end(); ++i)
    devices.push_back(const_cast<BluetoothDevice *>(*i));

  return devices;
}

BluetoothAdapter::ConstDeviceList BluetoothAdapter::GetDevices() const {
  ConstDeviceList devices;
  for (DevicesMap::const_iterator iter = devices_.begin();
       iter != devices_.end();
       ++iter)
    devices.push_back(iter->second);

  return devices;
}

BluetoothDevice* BluetoothAdapter::GetDevice(const std::string& address) {
  return const_cast<BluetoothDevice *>(
      const_cast<const BluetoothAdapter *>(this)->GetDevice(address));
}

const BluetoothDevice* BluetoothAdapter::GetDevice(
    const std::string& address) const {
  std::string canonicalized_address =
      BluetoothDevice::CanonicalizeAddress(address);
  if (canonicalized_address.empty())
    return NULL;

  DevicesMap::const_iterator iter = devices_.find(canonicalized_address);
  if (iter != devices_.end())
    return iter->second;

  return NULL;
}

void BluetoothAdapter::AddPairingDelegate(
    BluetoothDevice::PairingDelegate* pairing_delegate,
    PairingDelegatePriority priority) {
  // Remove the delegate, if it already exists, before inserting to allow a
  // change of priority.
  RemovePairingDelegate(pairing_delegate);

  // Find the first point with a lower priority, or the end of the list.
  std::list<PairingDelegatePair>::iterator iter = pairing_delegates_.begin();
  while (iter != pairing_delegates_.end() && iter->second >= priority)
    ++iter;

  pairing_delegates_.insert(iter, std::make_pair(pairing_delegate, priority));
}

void BluetoothAdapter::RemovePairingDelegate(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  for (std::list<PairingDelegatePair>::iterator iter =
       pairing_delegates_.begin(); iter != pairing_delegates_.end(); ++iter) {
    if (iter->first == pairing_delegate) {
      RemovePairingDelegateInternal(pairing_delegate);
      pairing_delegates_.erase(iter);
      return;
    }
  }
}

BluetoothDevice::PairingDelegate* BluetoothAdapter::DefaultPairingDelegate() {
  if (pairing_delegates_.empty())
    return NULL;

  return pairing_delegates_.front().first;
}

BluetoothAdapter::BluetoothAdapter() : weak_ptr_factory_(this) {
}

BluetoothAdapter::~BluetoothAdapter() {
  STLDeleteValues(&devices_);
}

void BluetoothAdapter::OnStartDiscoverySession(
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const DiscoverySessionCallback& callback) {
  VLOG(1) << "Discovery session started!";
  scoped_ptr<BluetoothDiscoverySession> discovery_session(
      new BluetoothDiscoverySession(scoped_refptr<BluetoothAdapter>(this),
                                    discovery_filter.Pass()));
  discovery_sessions_.insert(discovery_session.get());
  callback.Run(discovery_session.Pass());
}

void BluetoothAdapter::MarkDiscoverySessionsAsInactive() {
  // As sessions are marked as inactive they will notify the adapter that they
  // have become inactive, upon which the adapter will remove them from
  // |discovery_sessions_|. To avoid invalidating the iterator, make a copy
  // here.
  std::set<BluetoothDiscoverySession*> temp(discovery_sessions_);
  for (std::set<BluetoothDiscoverySession*>::iterator
          iter = temp.begin();
       iter != temp.end(); ++iter) {
    (*iter)->MarkAsInactive();
  }
}

void BluetoothAdapter::DiscoverySessionBecameInactive(
    BluetoothDiscoverySession* discovery_session) {
  DCHECK(!discovery_session->IsActive());
  discovery_sessions_.erase(discovery_session);
}

scoped_ptr<BluetoothDiscoveryFilter>
BluetoothAdapter::GetMergedDiscoveryFilterHelper(
    const BluetoothDiscoveryFilter* masked_filter,
    bool omit) const {
  scoped_ptr<BluetoothDiscoveryFilter> result;
  bool first_merge = true;

  std::set<BluetoothDiscoverySession*> temp(discovery_sessions_);
  for (const auto& iter : temp) {
    const BluetoothDiscoveryFilter* curr_filter = iter->GetDiscoveryFilter();

    if (!iter->IsActive())
      continue;

    if (omit && curr_filter == masked_filter) {
      // if masked_filter is pointing to empty filter, and there are
      // multiple empty filters in discovery_sessions_, make sure we'll
      // process next empty sessions.
      omit = false;
      continue;
    }

    if (first_merge) {
      first_merge = false;
      if (curr_filter) {
        result.reset(new BluetoothDiscoveryFilter(
            BluetoothDiscoveryFilter::Transport::TRANSPORT_DUAL));
        result->CopyFrom(*curr_filter);
      }
      continue;
    }

    result = BluetoothDiscoveryFilter::Merge(result.get(), curr_filter);
  }

  return result.Pass();
}

}  // namespace device
