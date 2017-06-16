// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/audio_messages.h"
#include "content/renderer/media/audio_message_filter.h"
#include "media/audio/audio_output_ipc.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const int kRenderFrameId = 2;

class MockAudioDelegate : public media::AudioOutputIPCDelegate {
 public:
  MockAudioDelegate() {
    Reset();
  }

  void OnStateChanged(media::AudioOutputIPCDelegateState state) override {
    state_changed_received_ = true;
    state_ = state;
  }

  void OnStreamCreated(base::SharedMemoryHandle handle,
                       base::SyncSocket::Handle,
                       int length) override {
    created_received_ = true;
    handle_ = handle;
    length_ = length;
  }

  void OnOutputDeviceSwitched(int request_id,
                              media::SwitchOutputDeviceResult result) override {
    output_device_switched_received_ = true;
    switch_output_device_result_ = result;
  }

  void OnIPCClosed() override {}

  void Reset() {
    state_changed_received_ = false;
    state_ = media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_ERROR;

    created_received_ = false;
    handle_ = base::SharedMemory::NULLHandle();
    length_ = 0;

    volume_received_ = false;
    volume_ = 0;

    output_device_switched_received_ = false;
    switch_output_device_result_ =
        media::SWITCH_OUTPUT_DEVICE_RESULT_ERROR_NOT_SUPPORTED;
  }

  bool state_changed_received() { return state_changed_received_; }
  media::AudioOutputIPCDelegateState state() { return state_; }

  bool created_received() { return created_received_; }
  base::SharedMemoryHandle handle() { return handle_; }
  uint32 length() { return length_; }

  bool output_device_switched_received() {
    return output_device_switched_received_;
  }
  media::SwitchOutputDeviceResult switch_output_device_result() {
    return switch_output_device_result_;
  }

 private:
  bool state_changed_received_;
  media::AudioOutputIPCDelegateState state_;

  bool created_received_;
  base::SharedMemoryHandle handle_;
  int length_;

  bool volume_received_;
  double volume_;

  bool output_device_switched_received_;
  media::SwitchOutputDeviceResult switch_output_device_result_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioDelegate);
};

}  // namespace

TEST(AudioMessageFilterTest, Basic) {
  base::MessageLoopForIO message_loop;

  scoped_refptr<AudioMessageFilter> filter(
      new AudioMessageFilter(message_loop.task_runner()));

  MockAudioDelegate delegate;
  const scoped_ptr<media::AudioOutputIPC> ipc =
      filter->CreateAudioOutputIPC(kRenderFrameId);
  static const int kSessionId = 0;
  ipc->CreateStream(&delegate, media::AudioParameters(), kSessionId);
  static const int kStreamId = 1;
  EXPECT_EQ(&delegate, filter->delegates_.Lookup(kStreamId));

  // AudioMsg_NotifyStreamCreated
  base::SyncSocket::TransitDescriptor socket_descriptor;
  const uint32 kLength = 1024;
  EXPECT_FALSE(delegate.created_received());
  filter->OnMessageReceived(AudioMsg_NotifyStreamCreated(
      kStreamId, base::SharedMemory::NULLHandle(), socket_descriptor, kLength));
  EXPECT_TRUE(delegate.created_received());
  EXPECT_FALSE(base::SharedMemory::IsHandleValid(delegate.handle()));
  EXPECT_EQ(kLength, delegate.length());
  delegate.Reset();

  // AudioMsg_NotifyStreamStateChanged
  EXPECT_FALSE(delegate.state_changed_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamStateChanged(
          kStreamId, media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_PLAYING));
  EXPECT_TRUE(delegate.state_changed_received());
  EXPECT_EQ(media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_PLAYING, delegate.state());
  delegate.Reset();

  // AudioMsg_NotifyOutputDeviceSwitched
  static const int kSwitchOutputRequestId = 1;
  EXPECT_FALSE(delegate.output_device_switched_received());
  filter->OnOutputDeviceSwitched(kStreamId, kSwitchOutputRequestId,
                                 media::SWITCH_OUTPUT_DEVICE_RESULT_SUCCESS);
  EXPECT_TRUE(delegate.output_device_switched_received());
  EXPECT_EQ(media::SWITCH_OUTPUT_DEVICE_RESULT_SUCCESS,
            delegate.switch_output_device_result());
  message_loop.RunUntilIdle();

  ipc->CloseStream();
  EXPECT_EQ(static_cast<media::AudioOutputIPCDelegate*>(NULL),
            filter->delegates_.Lookup(kStreamId));
}

TEST(AudioMessageFilterTest, Delegates) {
  base::MessageLoopForIO message_loop;

  scoped_refptr<AudioMessageFilter> filter(
      new AudioMessageFilter(message_loop.task_runner()));

  MockAudioDelegate delegate1;
  MockAudioDelegate delegate2;
  const scoped_ptr<media::AudioOutputIPC> ipc1 =
      filter->CreateAudioOutputIPC(kRenderFrameId);
  const scoped_ptr<media::AudioOutputIPC> ipc2 =
      filter->CreateAudioOutputIPC(kRenderFrameId);
  static const int kSessionId = 0;
  ipc1->CreateStream(&delegate1, media::AudioParameters(), kSessionId);
  ipc2->CreateStream(&delegate2, media::AudioParameters(), kSessionId);
  static const int kStreamId1 = 1;
  static const int kStreamId2 = 2;
  EXPECT_EQ(&delegate1, filter->delegates_.Lookup(kStreamId1));
  EXPECT_EQ(&delegate2, filter->delegates_.Lookup(kStreamId2));

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamStateChanged(
          kStreamId1, media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_PLAYING));
  EXPECT_TRUE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  delegate1.Reset();

  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_FALSE(delegate2.state_changed_received());
  filter->OnMessageReceived(
      AudioMsg_NotifyStreamStateChanged(
          kStreamId2, media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_PLAYING));
  EXPECT_FALSE(delegate1.state_changed_received());
  EXPECT_TRUE(delegate2.state_changed_received());
  delegate2.Reset();

  message_loop.RunUntilIdle();

  ipc1->CloseStream();
  ipc2->CloseStream();
  EXPECT_EQ(static_cast<media::AudioOutputIPCDelegate*>(NULL),
            filter->delegates_.Lookup(kStreamId1));
  EXPECT_EQ(static_cast<media::AudioOutputIPCDelegate*>(NULL),
            filter->delegates_.Lookup(kStreamId2));
}

}  // namespace content
