// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_USB_H_
#define MEDIA_MIDI_MIDI_MANAGER_USB_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "media/midi/midi_manager.h"
#include "media/midi/usb_midi_device.h"
#include "media/midi/usb_midi_export.h"
#include "media/midi/usb_midi_input_stream.h"
#include "media/midi/usb_midi_jack.h"
#include "media/midi/usb_midi_output_stream.h"

namespace media {
namespace midi {

class MidiScheduler;

// MidiManager for USB-MIDI.
class USB_MIDI_EXPORT MidiManagerUsb
    : public MidiManager,
      public UsbMidiDeviceDelegate,
      NON_EXPORTED_BASE(public UsbMidiInputStream::Delegate) {
 public:
  explicit MidiManagerUsb(scoped_ptr<UsbMidiDevice::Factory> device_factory);
  ~MidiManagerUsb() override;

  // MidiManager implementation.
  void StartInitialization() override;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32 port_index,
                            const std::vector<uint8>& data,
                            double timestamp) override;

  // UsbMidiDeviceDelegate implementation.
  void ReceiveUsbMidiData(UsbMidiDevice* device,
                          int endpoint_number,
                          const uint8* data,
                          size_t size,
                          base::TimeTicks time) override;
  void OnDeviceAttached(scoped_ptr<UsbMidiDevice> device) override;
  void OnDeviceDetached(size_t index) override;

  // UsbMidiInputStream::Delegate implementation.
  void OnReceivedData(size_t jack_index,
                      const uint8* data,
                      size_t size,
                      base::TimeTicks time) override;

  const ScopedVector<UsbMidiOutputStream>& output_streams() const {
    return output_streams_;
  }
  const UsbMidiInputStream* input_stream() const { return input_stream_.get(); }

  // Initializes this object.
  // When the initialization finishes, |callback| will be called with the
  // result.
  // When this factory is destroyed during the operation, the operation
  // will be canceled silently (i.e. |callback| will not be called).
  // The function is public just for unit tests. Do not call this function
  // outside code for testing.
  void Initialize(base::Callback<void(Result result)> callback);

 private:
  void OnEnumerateDevicesDone(bool result, UsbMidiDevice::Devices* devices);
  bool AddPorts(UsbMidiDevice* device, int device_id);

  scoped_ptr<UsbMidiDevice::Factory> device_factory_;
  ScopedVector<UsbMidiDevice> devices_;
  ScopedVector<UsbMidiOutputStream> output_streams_;
  scoped_ptr<UsbMidiInputStream> input_stream_;

  base::Callback<void(Result result)> initialize_callback_;

  // A map from <endpoint_number, cable_number> to the index of input jacks.
  base::hash_map<std::pair<int, int>, size_t> input_jack_dictionary_;

  scoped_ptr<MidiScheduler> scheduler_;

  DISALLOW_COPY_AND_ASSIGN(MidiManagerUsb);
};

}  // namespace midi
}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_USB_H_
