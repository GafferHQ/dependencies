// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_audio_sink.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class BluetoothAdvertisement;
class BluetoothDiscoveryFilter;
class BluetoothDiscoverySession;
class BluetoothGattCharacteristic;
class BluetoothGattDescriptor;
class BluetoothGattService;
class BluetoothSocket;
class BluetoothUUID;
struct BluetoothAdapterDeleter;

// BluetoothAdapter represents a local Bluetooth adapter which may be used to
// interact with remote Bluetooth devices. As well as providing support for
// determining whether an adapter is present and whether the radio is powered,
// this class also provides support for obtaining the list of remote devices
// known to the adapter, discovering new devices, and providing notification of
// updates to device information.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapter
    : public base::RefCounted<BluetoothAdapter> {
 public:
  // Interface for observing changes from bluetooth adapters.
  class DEVICE_BLUETOOTH_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Called when the presence of the adapter |adapter| changes. When |present|
    // is true the adapter is now present, false means the adapter has been
    // removed from the system.
    virtual void AdapterPresentChanged(BluetoothAdapter* adapter,
                                       bool present) {}

    // Called when the radio power state of the adapter |adapter| changes. When
    // |powered| is true the adapter radio is powered, false means the adapter
    // radio is off.
    virtual void AdapterPoweredChanged(BluetoothAdapter* adapter,
                                       bool powered) {}

    // Called when the discoverability state of the  adapter |adapter| changes.
    // When |discoverable| is true the adapter is discoverable by other devices,
    // false means the adapter is not discoverable.
    virtual void AdapterDiscoverableChanged(BluetoothAdapter* adapter,
                                           bool discoverable) {}

    // Called when the discovering state of the adapter |adapter| changes. When
    // |discovering| is true the adapter is seeking new devices, false means it
    // is not.
    virtual void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                           bool discovering) {}

    // Called when a new device |device| is added to the adapter |adapter|,
    // either because it has been discovered or a connection made. |device|
    // should not be cached. Instead, copy its Bluetooth address.
    virtual void DeviceAdded(BluetoothAdapter* adapter,
                             BluetoothDevice* device) {}

    // Called when properties of the device |device| known to the adapter
    // |adapter| change. |device| should not be cached. Instead, copy its
    // Bluetooth address.
    virtual void DeviceChanged(BluetoothAdapter* adapter,
                               BluetoothDevice* device) {}

    // Called when the device |device| is removed from the adapter |adapter|,
    // either as a result of a discovered device being lost between discovering
    // phases or pairing information deleted. |device| should not be
    // cached. Instead, copy its Bluetooth address.
    virtual void DeviceRemoved(BluetoothAdapter* adapter,
                               BluetoothDevice* device) {}

    // Called when a new GATT service |service| is added to the device |device|,
    // as the service is received from the device. Don't cache |service|. Store
    // its identifier instead (i.e. BluetoothGattService::GetIdentifier).
    virtual void GattServiceAdded(BluetoothAdapter* adapter,
                                  BluetoothDevice* device,
                                  BluetoothGattService* service) {}

    // Called when the GATT service |service| is removed from the device
    // |device|. This can happen if the attribute database of the remote device
    // changes or when |device| gets removed.
    virtual void GattServiceRemoved(BluetoothAdapter* adapter,
                                    BluetoothDevice* device,
                                    BluetoothGattService* service) {}

    // Called when all characteristic and descriptor discovery procedures are
    // known to be completed for the GATT service |service|. This method will be
    // called after the initial discovery of a GATT service and will usually be
    // preceded by calls to GattCharacteristicAdded and GattDescriptorAdded.
    virtual void GattDiscoveryCompleteForService(
        BluetoothAdapter* adapter,
        BluetoothGattService* service) {}

    // Called when properties of the remote GATT service |service| have changed.
    // This will get called for properties such as UUID, as well as for changes
    // to the list of known characteristics and included services. Observers
    // should read all GATT characteristic and descriptors objects and do any
    // necessary set up required for a changed service.
    virtual void GattServiceChanged(BluetoothAdapter* adapter,
                                    BluetoothGattService* service) {}

    // Called when the remote GATT characteristic |characteristic| has been
    // discovered. Use this to issue any initial read/write requests to the
    // characteristic but don't cache the pointer as it may become invalid.
    // Instead, use the specially assigned identifier to obtain a characteristic
    // and cache that identifier as necessary, as it can be used to retrieve the
    // characteristic from its GATT service. The number of characteristics with
    // the same UUID belonging to a service depends on the particular profile
    // the remote device implements, hence the client of a GATT based profile
    // will usually operate on the whole set of characteristics and not just
    // one.
    virtual void GattCharacteristicAdded(
        BluetoothAdapter* adapter,
        BluetoothGattCharacteristic* characteristic) {}

    // Called when a GATT characteristic |characteristic| has been removed from
    // the system.
    virtual void GattCharacteristicRemoved(
        BluetoothAdapter* adapter,
        BluetoothGattCharacteristic* characteristic) {}

    // Called when the remote GATT characteristic descriptor |descriptor| has
    // been discovered. Don't cache the arguments as the pointers may become
    // invalid. Instead, use the specially assigned identifier to obtain a
    // descriptor and cache that identifier as necessary.
    virtual void GattDescriptorAdded(BluetoothAdapter* adapter,
                                     BluetoothGattDescriptor* descriptor) {}

    // Called when a GATT characteristic descriptor |descriptor| has been
    // removed from the system.
    virtual void GattDescriptorRemoved(BluetoothAdapter* adapter,
                                       BluetoothGattDescriptor* descriptor) {}

    // Called when the value of a characteristic has changed. This might be a
    // result of a read/write request to, or a notification/indication from, a
    // remote GATT characteristic.
    virtual void GattCharacteristicValueChanged(
        BluetoothAdapter* adapter,
        BluetoothGattCharacteristic* characteristic,
        const std::vector<uint8>& value) {}

    // Called when the value of a characteristic descriptor has been updated.
    virtual void GattDescriptorValueChanged(BluetoothAdapter* adapter,
                                            BluetoothGattDescriptor* descriptor,
                                            const std::vector<uint8>& value) {}
  };

  // Used to configure a listening servie.
  struct DEVICE_BLUETOOTH_EXPORT ServiceOptions {
    ServiceOptions();
    ~ServiceOptions();

    scoped_ptr<int> channel;
    scoped_ptr<int> psm;
    scoped_ptr<std::string> name;
  };

  // The ErrorCallback is used for methods that can fail in which case it is
  // called, in the success case the callback is simply not called.
  typedef base::Closure ErrorCallback;

  // The InitCallback is used to trigger a callback after asynchronous
  // initialization, if initialization is asynchronous on the platform.
  typedef base::Callback<void()> InitCallback;

  typedef base::Callback<void(scoped_ptr<BluetoothDiscoverySession>)>
      DiscoverySessionCallback;
  typedef std::vector<BluetoothDevice*> DeviceList;
  typedef std::vector<const BluetoothDevice*> ConstDeviceList;
  typedef base::Callback<void(scoped_refptr<BluetoothSocket>)>
      CreateServiceCallback;
  typedef base::Callback<void(const std::string& message)>
      CreateServiceErrorCallback;
  typedef base::Callback<void(scoped_refptr<BluetoothAudioSink>)>
      AcquiredCallback;
  typedef base::Callback<void(scoped_refptr<BluetoothAdvertisement>)>
      CreateAdvertisementCallback;
  typedef base::Callback<void(BluetoothAdvertisement::ErrorCode)>
      CreateAdvertisementErrorCallback;

  // Returns a weak pointer to a new adapter.  For platforms with asynchronous
  // initialization, the returned adapter will run the |init_callback| once
  // asynchronous initialization is complete.
  // Caution: The returned pointer also transfers ownership of the adapter.  The
  // caller is expected to call |AddRef()| on the returned pointer, typically by
  // storing it into a |scoped_refptr|.
  static base::WeakPtr<BluetoothAdapter> CreateAdapter(
      const InitCallback& init_callback);

  // Returns a weak pointer to an existing adapter for testing purposes only.
  base::WeakPtr<BluetoothAdapter> GetWeakPtrForTesting();

#if defined(OS_CHROMEOS)
  // Shutdown the adapter: tear down and clean up all objects owned by
  // BluetoothAdapter. After this call, the BluetoothAdapter will behave as if
  // no Bluetooth controller exists in the local system. |IsPresent| will return
  // false.
  virtual void Shutdown();
#endif

  // Adds and removes observers for events on this bluetooth adapter. If
  // monitoring multiple adapters, check the |adapter| parameter of observer
  // methods to determine which adapter is issuing the event.
  virtual void AddObserver(BluetoothAdapter::Observer* observer);
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer);

  // The address of this adapter. The address format is "XX:XX:XX:XX:XX:XX",
  // where each XX is a hexadecimal number.
  virtual std::string GetAddress() const = 0;

  // The name of the adapter.
  virtual std::string GetName() const = 0;

  // Set the human-readable name of the adapter to |name|. On success,
  // |callback| will be called. On failure, |error_callback| will be called.
  virtual void SetName(const std::string& name,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) = 0;

  // Indicates whether the adapter is initialized and ready to use.
  virtual bool IsInitialized() const = 0;

  // Indicates whether the adapter is actually present on the system. For the
  // default adapter, this indicates whether any adapter is present. An adapter
  // is only considered present if the address has been obtained.
  virtual bool IsPresent() const = 0;

  // Indicates whether the adapter radio is powered.
  virtual bool IsPowered() const = 0;

  // Requests a change to the adapter radio power. Setting |powered| to true
  // will turn on the radio and false will turn it off. On success, |callback|
  // will be called. On failure, |error_callback| will be called.
  virtual void SetPowered(bool powered,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Indicates whether the adapter radio is discoverable.
  virtual bool IsDiscoverable() const = 0;

  // Requests that the adapter change its discoverability state. If
  // |discoverable| is true, then it will be discoverable by other Bluetooth
  // devices. On successfully changing the adapter's discoverability, |callback|
  // will be called. On failure, |error_callback| will be called.
  virtual void SetDiscoverable(bool discoverable,
                               const base::Closure& callback,
                               const ErrorCallback& error_callback) = 0;

  // Indicates whether the adapter is currently discovering new devices.
  virtual bool IsDiscovering() const = 0;

  // Requests the adapter to start a new discovery session. On success, a new
  // instance of BluetoothDiscoverySession will be returned to the caller via
  // |callback| and the adapter will be discovering nearby Bluetooth devices.
  // The returned BluetoothDiscoverySession is owned by the caller and it's the
  // owner's responsibility to properly clean it up and stop the session when
  // device discovery is no longer needed.
  //
  // If clients desire device discovery to run, they should always call this
  // method and never make it conditional on the value of IsDiscovering(), as
  // another client might cause discovery to stop unexpectedly. Hence, clients
  // should always obtain a BluetoothDiscoverySession and call
  // BluetoothDiscoverySession::Stop when done. When this method gets called,
  // device discovery may actually be in progress. Clients can call GetDevices()
  // and check for those with IsPaired() as false to obtain the list of devices
  // that have been discovered so far. Otherwise, clients can be notified of all
  // new and lost devices by implementing the Observer methods "DeviceAdded" and
  // "DeviceRemoved".
  virtual void StartDiscoverySession(const DiscoverySessionCallback& callback,
                                     const ErrorCallback& error_callback);
  virtual void StartDiscoverySessionWithFilter(
      scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
      const DiscoverySessionCallback& callback,
      const ErrorCallback& error_callback);

  // Return all discovery filters assigned to this adapter merged together.
  scoped_ptr<BluetoothDiscoveryFilter> GetMergedDiscoveryFilter() const;

  // Works like GetMergedDiscoveryFilter, but doesn't take |masked_filter| into
  // account. |masked_filter| is compared by pointer, and must be a member of
  // active session.
  scoped_ptr<BluetoothDiscoveryFilter> GetMergedDiscoveryFilterMasked(
      BluetoothDiscoveryFilter* masked_filter) const;

  // Requests the list of devices from the adapter. All devices are returned,
  // including those currently connected and those paired. Use the returned
  // device pointers to determine which they are.
  virtual DeviceList GetDevices();
  virtual ConstDeviceList GetDevices() const;

  // Returns a pointer to the device with the given address |address| or NULL if
  // no such device is known.
  virtual BluetoothDevice* GetDevice(const std::string& address);
  virtual const BluetoothDevice* GetDevice(const std::string& address) const;

  // Possible priorities for AddPairingDelegate(), low is intended for
  // permanent UI and high is intended for interactive UI or applications.
  enum PairingDelegatePriority {
    PAIRING_DELEGATE_PRIORITY_LOW,
    PAIRING_DELEGATE_PRIORITY_HIGH
  };

  // Adds a default pairing delegate with priority |priority|. Method calls
  // will be made on |pairing_delegate| for incoming pairing requests if the
  // priority is higher than any other registered; or for those of the same
  // priority, the first registered.
  //
  // |pairing_delegate| must not be freed without first calling
  // RemovePairingDelegate().
  virtual void AddPairingDelegate(
      BluetoothDevice::PairingDelegate* pairing_delegate,
      PairingDelegatePriority priority);

  // Removes a previously added pairing delegate.
  virtual void RemovePairingDelegate(
      BluetoothDevice::PairingDelegate* pairing_delegate);

  // Returns the first registered pairing delegate with the highest priority,
  // or NULL if no delegate is registered. Used to select the delegate for
  // incoming pairing requests.
  virtual BluetoothDevice::PairingDelegate* DefaultPairingDelegate();

  // Creates an RFCOMM service on this adapter advertised with UUID |uuid|,
  // listening on channel |options.channel|, which may be left null to
  // automatically allocate one. The service will be advertised with
  // |options.name| as the English name of the service. |callback| will be
  // called on success with a BluetoothSocket instance that is to be owned by
  // the received.  |error_callback| will be called on failure with a message
  // indicating the cause.
  virtual void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) = 0;

  // Creates an L2CAP service on this adapter advertised with UUID |uuid|,
  // listening on PSM |options.psm|, which may be left null to automatically
  // allocate one. The service will be advertised with |options.name| as the
  // English name of the service. |callback| will be called on success with a
  // BluetoothSocket instance that is to be owned by the received.
  // |error_callback| will be called on failure with a message indicating the
  // cause.
  virtual void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) = 0;

  // Creates and registers a BluetoothAudioSink with |options|. If the fields in
  // |options| are not specified, the default values will be used. |callback|
  // will be called on success with a BluetoothAudioSink which is to be owned by
  // the caller of this method. |error_callback| will be called on failure with
  // a message indicating the cause.
  virtual void RegisterAudioSink(
      const BluetoothAudioSink::Options& options,
      const AcquiredCallback& callback,
      const BluetoothAudioSink::ErrorCallback& error_callback) = 0;

  // Creates and registers an advertisement for broadcast over the LE channel.
  // The created advertisement will be returned via the success callback.
  virtual void RegisterAdvertisement(
      scoped_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const CreateAdvertisementErrorCallback& error_callback) = 0;

 protected:
  friend class base::RefCounted<BluetoothAdapter>;
  friend class BluetoothDiscoverySession;

  typedef std::map<const std::string, BluetoothDevice*> DevicesMap;
  typedef std::pair<BluetoothDevice::PairingDelegate*, PairingDelegatePriority>
      PairingDelegatePair;

  BluetoothAdapter();
  virtual ~BluetoothAdapter();

  // Internal methods for initiating and terminating device discovery sessions.
  // An implementation of BluetoothAdapter keeps an internal reference count to
  // make sure that the underlying controller is constantly searching for nearby
  // devices and retrieving information from them as long as there are clients
  // who have requested discovery. These methods behave in the following way:
  //
  // On a call to AddDiscoverySession:
  //    - If there is a pending request to the subsystem, queue this request to
  //      execute once the pending requests are done.
  //    - If the count is 0, issue a request to the subsystem to start
  //      device discovery. On success, increment the count to 1.
  //    - If the count is greater than 0, increment the count and return
  //      success.
  //    As long as the count is non-zero, the underlying controller will be
  //    discovering for devices. This means that Chrome will restart device
  //    scan and inquiry sessions if they ever end, unless these sessions
  //    terminate due to an unexpected reason.
  //
  // On a call to RemoveDiscoverySession:
  //    - If there is a pending request to the subsystem, queue this request to
  //      execute once the pending requests are done.
  //    - If the count is 0, return failure, as there is no active discovery
  //      session.
  //    - If the count is 1, issue a request to the subsystem to stop device
  //      discovery and decrement the count to 0 on success.
  //    - If the count is greater than 1, decrement the count and return
  //      success.
  //
  // |discovery_filter| passed to AddDiscoverySession and RemoveDiscoverySession
  // is owned by other objects and shall not be freed.  When the count is
  // greater than 0 and AddDiscoverySession or RemoveDiscoverySession is called
  // the filter being used by the underlying controller must be updated.
  //
  // These methods invoke |callback| for success and |error_callback| for
  // failures.
  virtual void AddDiscoverySession(BluetoothDiscoveryFilter* discovery_filter,
                                   const base::Closure& callback,
                                   const ErrorCallback& error_callback) = 0;
  virtual void RemoveDiscoverySession(
      BluetoothDiscoveryFilter* discovery_filter,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Used to set and update the discovery filter used by the underlying
  // Bluetooth controller.
  virtual void SetDiscoveryFilter(
      scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Called by RemovePairingDelegate() in order to perform any class-specific
  // internal functionality necessary to remove the pairing delegate, such as
  // cleaning up ongoing pairings using it.
  virtual void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) = 0;

  // Success callback passed to AddDiscoverySession by StartDiscoverySession.
  void OnStartDiscoverySession(
      scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
      const DiscoverySessionCallback& callback);

  // Marks all known DiscoverySession instances as inactive. Called by
  // BluetoothAdapter in the event that the adapter unexpectedly stops
  // discovering. This should be called by all platform implementations.
  void MarkDiscoverySessionsAsInactive();

  // Removes |discovery_session| from |discovery_sessions_|, if its in there.
  // Called by DiscoverySession when an instance is destroyed or becomes
  // inactive.
  void DiscoverySessionBecameInactive(
      BluetoothDiscoverySession* discovery_session);

  // Observers of BluetoothAdapter, notified from implementation subclasses.
  base::ObserverList<device::BluetoothAdapter::Observer> observers_;

  // Devices paired with, connected to, discovered by, or visible to the
  // adapter. The key is the Bluetooth address of the device and the value is
  // the BluetoothDevice object whose lifetime is managed by the adapter
  // instance.
  DevicesMap devices_;

  // Default pairing delegates registered with the adapter.
  std::list<PairingDelegatePair> pairing_delegates_;

 private:
  // Return all discovery filters assigned to this adapter merged together.
  // If |omit| is true, |discovery_filter| will not be processed.
  scoped_ptr<BluetoothDiscoveryFilter> GetMergedDiscoveryFilterHelper(
      const BluetoothDiscoveryFilter* discovery_filter,
      bool omit) const;

  // List of active DiscoverySession objects. This is used to notify sessions to
  // become inactive in case of an unexpected change to the adapter discovery
  // state. We keep raw pointers, with the invariant that a DiscoverySession
  // will remove itself from this list when it gets destroyed or becomes
  // inactive by calling DiscoverySessionBecameInactive(), hence no pointers to
  // deallocated sessions are kept.
  std::set<BluetoothDiscoverySession*> discovery_sessions_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapter> weak_ptr_factory_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_H_
