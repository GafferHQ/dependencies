// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_

#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"
#include "media/midi/midi_port_info.h"
#include "media/midi/result.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessorClient.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// MessageFilter that handles MIDI messages.
class CONTENT_EXPORT MidiMessageFilter : public IPC::MessageFilter {
 public:
  explicit MidiMessageFilter(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Each client registers for MIDI access here.
  // If permission is granted, then the client's
  void AddClient(blink::WebMIDIAccessorClient* client);
  void RemoveClient(blink::WebMIDIAccessorClient* client);

  // A client will only be able to call this method if it has a suitable
  // output port (from addOutputPort()).
  void SendMidiData(uint32 port,
                    const uint8* data,
                    size_t length,
                    double timestamp);

  // IO task runner associated with this message filter.
  base::SingleThreadTaskRunner* io_task_runner() const {
    return io_task_runner_.get();
  }

  static blink::WebMIDIAccessorClient::MIDIPortState ToBlinkState(
      media::midi::MidiPortState state) {
    // "open" status is separately managed by blink per MIDIAccess instance.
    if (state == media::midi::MIDI_PORT_OPENED)
      state = media::midi::MIDI_PORT_CONNECTED;
    return static_cast<blink::WebMIDIAccessorClient::MIDIPortState>(state);
  }

 protected:
  ~MidiMessageFilter() override;

 private:
  void StartSessionOnIOThread();

  void SendMidiDataOnIOThread(uint32 port,
                              const std::vector<uint8>& data,
                              double timestamp);

  void EndSessionOnIOThread();

  // Sends an IPC message using |sender_|.
  void Send(IPC::Message* message);

  // IPC::MessageFilter override. Called on |io_task_runner|.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Sender* sender) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

  // Called when the browser process has approved (or denied) access to
  // MIDI hardware.
  void OnSessionStarted(media::midi::Result result);

  // These functions are called in 2 cases:
  //  (1) Just before calling |OnSessionStarted|, to notify the recipient about
  //      existing ports.
  //  (2) To notify the recipient that a new device was connected and that new
  //      ports have been created.
  void OnAddInputPort(media::midi::MidiPortInfo info);
  void OnAddOutputPort(media::midi::MidiPortInfo info);

  // These functions are called to notify the recipient that a device that is
  // notified via OnAddInputPort() or OnAddOutputPort() gets disconnected, or
  // connected again.
  void OnSetInputPortState(uint32 port, media::midi::MidiPortState state);
  void OnSetOutputPortState(uint32 port, media::midi::MidiPortState state);

  // Called when the browser process has sent MIDI data containing one or
  // more messages.
  void OnDataReceived(uint32 port,
                      const std::vector<uint8>& data,
                      double timestamp);

  // From time-to-time, the browser incrementally informs us of how many bytes
  // it has successfully sent. This is part of our throttling process to avoid
  // sending too much data before knowing how much has already been sent.
  void OnAcknowledgeSentData(size_t bytes_sent);

  // Following methods, Handle*, run on |main_task_runner_|.
  void HandleClientAdded(media::midi::Result result);

  void HandleAddInputPort(media::midi::MidiPortInfo info);
  void HandleAddOutputPort(media::midi::MidiPortInfo info);
  void HandleSetInputPortState(uint32 port, media::midi::MidiPortState state);
  void HandleSetOutputPortState(uint32 port, media::midi::MidiPortState state);

  void HandleDataReceived(uint32 port,
                          const std::vector<uint8>& data,
                          double timestamp);

  void HandleAckknowledgeSentData(size_t bytes_sent);

  // IPC sender for Send(); must only be accessed on |io_task_runner_|.
  IPC::Sender* sender_;

  // Task runner on which IPC calls are driven.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Main task runner.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  /*
   * Notice: Following members are designed to be accessed only on
   * |main_task_runner_|.
   */
  // Keeps track of all MIDI clients. This should be std::set so that various
  // for-loops work correctly. To change the type, make sure that the new type
  // is safe to modify the container inside for-loops.
  typedef std::set<blink::WebMIDIAccessorClient*> ClientsSet;
  ClientsSet clients_;

  // Represents clients that are waiting for a session being open.
  // Note: std::vector is not safe to invoke callbacks inside iterator based
  // for-loops.
  typedef std::vector<blink::WebMIDIAccessorClient*> ClientsQueue;
  ClientsQueue clients_waiting_session_queue_;

  // Represents a result on starting a session. Can be accessed only on
  media::midi::Result session_result_;

  // Holds MidiPortInfoList for input ports and output ports.
  media::midi::MidiPortInfoList inputs_;
  media::midi::MidiPortInfoList outputs_;

  size_t unacknowledged_bytes_sent_;

  DISALLOW_COPY_AND_ASSIGN(MidiMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_
