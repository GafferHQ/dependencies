// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/at_exit.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/sync_socket.h"
#include "base/task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/thread_task_runner_handle.h"
#include "media/audio/audio_output_device.h"
#include "media/audio/sample_rates.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::CancelableSyncSocket;
using base::SharedMemory;
using base::SyncSocket;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::WithArgs;
using testing::StrictMock;
using testing::Values;

namespace media {

namespace {

class MockRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  MockRenderCallback() {}
  virtual ~MockRenderCallback() {}

  MOCK_METHOD2(Render, int(AudioBus* dest, int audio_delay_milliseconds));
  MOCK_METHOD0(OnRenderError, void());
};

class MockAudioOutputIPC : public AudioOutputIPC {
 public:
  MockAudioOutputIPC() {}
  virtual ~MockAudioOutputIPC() {}

  MOCK_METHOD3(CreateStream, void(AudioOutputIPCDelegate* delegate,
                                  const AudioParameters& params,
                                  int session_id));
  MOCK_METHOD0(PlayStream, void());
  MOCK_METHOD0(PauseStream, void());
  MOCK_METHOD0(CloseStream, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD3(SwitchOutputDevice,
               void(const std::string& device_id,
                    const GURL& security_origin,
                    int request_id));
};

class MockSwitchOutputDeviceCallback {
 public:
  MOCK_METHOD1(Callback, void(media::SwitchOutputDeviceResult result));
};

ACTION_P2(SendPendingBytes, socket, pending_bytes) {
  socket->Send(&pending_bytes, sizeof(pending_bytes));
}

// Used to terminate a loop from a different thread than the loop belongs to.
// |task_runner| should be a SingleThreadTaskRunner.
ACTION_P(QuitLoop, task_runner) {
  task_runner->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
}

}  // namespace.

class AudioOutputDeviceTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  AudioOutputDeviceTest();
  ~AudioOutputDeviceTest();

  void StartAudioDevice();
  void CreateStream();
  void ExpectRenderCallback();
  void WaitUntilRenderCallback();
  void StopAudioDevice();
  void SwitchOutputDevice();

 protected:
  // Used to clean up TLS pointers that the test(s) will initialize.
  // Must remain the first member of this class.
  base::ShadowingAtExitManager at_exit_manager_;
  base::MessageLoopForIO io_loop_;
  AudioParameters default_audio_parameters_;
  StrictMock<MockRenderCallback> callback_;
  MockAudioOutputIPC* audio_output_ipc_;  // owned by audio_device_
  scoped_refptr<AudioOutputDevice> audio_device_;
  MockSwitchOutputDeviceCallback switch_output_device_callback_;

 private:
  int CalculateMemorySize();
  void SwitchOutputDeviceCallback(SwitchOutputDeviceResult result);

  SharedMemory shared_memory_;
  CancelableSyncSocket browser_socket_;
  CancelableSyncSocket renderer_socket_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDeviceTest);
};

int AudioOutputDeviceTest::CalculateMemorySize() {
  // Calculate output memory size.
  return AudioBus::CalculateMemorySize(default_audio_parameters_);
}

AudioOutputDeviceTest::AudioOutputDeviceTest() {
  default_audio_parameters_.Reset(
      AudioParameters::AUDIO_PCM_LINEAR,
      CHANNEL_LAYOUT_STEREO, 2, 48000, 16, 1024);

  audio_output_ipc_ = new MockAudioOutputIPC();
  audio_device_ = new AudioOutputDevice(
      scoped_ptr<AudioOutputIPC>(audio_output_ipc_),
      io_loop_.task_runner());

  audio_device_->Initialize(default_audio_parameters_,
                            &callback_);

  io_loop_.RunUntilIdle();
}

AudioOutputDeviceTest::~AudioOutputDeviceTest() {
  audio_device_ = NULL;
}

void AudioOutputDeviceTest::StartAudioDevice() {
  audio_device_->Start();

  EXPECT_CALL(*audio_output_ipc_, CreateStream(audio_device_.get(), _, 0));

  io_loop_.RunUntilIdle();
}

void AudioOutputDeviceTest::CreateStream() {
  const int kMemorySize = CalculateMemorySize();

  ASSERT_TRUE(shared_memory_.CreateAndMapAnonymous(kMemorySize));
  memset(shared_memory_.memory(), 0xff, kMemorySize);

  ASSERT_TRUE(CancelableSyncSocket::CreatePair(&browser_socket_,
                                               &renderer_socket_));

  // Create duplicates of the handles we pass to AudioOutputDevice since
  // ownership will be transferred and AudioOutputDevice is responsible for
  // freeing.
  SyncSocket::TransitDescriptor audio_device_socket_descriptor;
  ASSERT_TRUE(renderer_socket_.PrepareTransitDescriptor(
      base::GetCurrentProcessHandle(), &audio_device_socket_descriptor));
  base::SharedMemoryHandle duplicated_memory_handle;
  ASSERT_TRUE(shared_memory_.ShareToProcess(base::GetCurrentProcessHandle(),
                                            &duplicated_memory_handle));

  audio_device_->OnStreamCreated(
      duplicated_memory_handle,
      SyncSocket::UnwrapHandle(audio_device_socket_descriptor), kMemorySize);
  io_loop_.RunUntilIdle();
}

void AudioOutputDeviceTest::ExpectRenderCallback() {
  // We should get a 'play' notification when we call OnStreamCreated().
  // Respond by asking for some audio data.  This should ask our callback
  // to provide some audio data that AudioOutputDevice then writes into the
  // shared memory section.
  const int kMemorySize = CalculateMemorySize();

  EXPECT_CALL(*audio_output_ipc_, PlayStream())
      .WillOnce(SendPendingBytes(&browser_socket_, kMemorySize));

  // We expect calls to our audio renderer callback, which returns the number
  // of frames written to the memory section.
  // Here's the second place where it gets hacky:  There's no way for us to
  // know (without using a sleep loop!) when the AudioOutputDevice has finished
  // writing the interleaved audio data into the shared memory section.
  // So, for the sake of this test, we consider the call to Render a sign
  // of success and quit the loop.
  const int kNumberOfFramesToProcess = 0;
  EXPECT_CALL(callback_, Render(_, _))
      .WillOnce(DoAll(
          QuitLoop(io_loop_.task_runner()),
          Return(kNumberOfFramesToProcess)));
}

void AudioOutputDeviceTest::WaitUntilRenderCallback() {
  // Don't hang the test if we never get the Render() callback.
  io_loop_.PostDelayedTask(FROM_HERE, base::MessageLoop::QuitClosure(),
                           TestTimeouts::action_timeout());
  io_loop_.Run();
}

void AudioOutputDeviceTest::StopAudioDevice() {
  audio_device_->Stop();

  EXPECT_CALL(*audio_output_ipc_, CloseStream());

  io_loop_.RunUntilIdle();
}

void AudioOutputDeviceTest::SwitchOutputDevice() {
  const GURL security_origin("http://localhost");
  const std::string device_id;
  const int request_id = 1;

  // Switch the output device and check that the IPC message is sent
  EXPECT_CALL(*audio_output_ipc_,
              SwitchOutputDevice(device_id, security_origin, request_id));
  audio_device_->SwitchOutputDevice(
      device_id, security_origin,
      base::Bind(&MockSwitchOutputDeviceCallback::Callback,
                 base::Unretained(&switch_output_device_callback_)));
  io_loop_.RunUntilIdle();

  // Simulate the reception of a successful response from the browser
  EXPECT_CALL(switch_output_device_callback_,
              Callback(SWITCH_OUTPUT_DEVICE_RESULT_SUCCESS));
  audio_device_->OnOutputDeviceSwitched(request_id,
                                        SWITCH_OUTPUT_DEVICE_RESULT_SUCCESS);
  io_loop_.RunUntilIdle();
}

TEST_P(AudioOutputDeviceTest, Initialize) {
  // Tests that the object can be constructed, initialized and destructed
  // without having ever been started/stopped.
}

// Calls Start() followed by an immediate Stop() and check for the basic message
// filter messages being sent in that case.
TEST_P(AudioOutputDeviceTest, StartStop) {
  StartAudioDevice();
  StopAudioDevice();
}

// AudioOutputDevice supports multiple start/stop sequences.
TEST_P(AudioOutputDeviceTest, StartStopStartStop) {
  StartAudioDevice();
  StopAudioDevice();
  StartAudioDevice();
  StopAudioDevice();
}

// Simulate receiving OnStreamCreated() prior to processing ShutDownOnIOThread()
// on the IO loop.
TEST_P(AudioOutputDeviceTest, StopBeforeRender) {
  StartAudioDevice();

  // Call Stop() but don't run the IO loop yet.
  audio_device_->Stop();

  // Expect us to shutdown IPC but not to render anything despite the stream
  // getting created.
  EXPECT_CALL(*audio_output_ipc_, CloseStream());
  CreateStream();
}

// Full test with output only.
TEST_P(AudioOutputDeviceTest, CreateStream) {
  StartAudioDevice();
  ExpectRenderCallback();
  CreateStream();
  WaitUntilRenderCallback();
  StopAudioDevice();
}

// Switch the output device
TEST_P(AudioOutputDeviceTest, SwitchOutputDevice) {
  StartAudioDevice();
  SwitchOutputDevice();
  StopAudioDevice();
}

INSTANTIATE_TEST_CASE_P(Render, AudioOutputDeviceTest, Values(false));

}  // namespace media.
