// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureController.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#endif

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;

namespace content {

class MockVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  explicit MockVideoCaptureControllerEventHandler(
      VideoCaptureController* controller)
      : controller_(controller),
        resource_utilization_(-1.0) {}
  ~MockVideoCaptureControllerEventHandler() override {}

  // These mock methods are delegated to by our fake implementation of
  // VideoCaptureControllerEventHandler, to be used in EXPECT_CALL().
  MOCK_METHOD1(DoBufferCreated, void(VideoCaptureControllerID));
  MOCK_METHOD1(DoBufferDestroyed, void(VideoCaptureControllerID));
  MOCK_METHOD2(DoI420BufferReady,
               void(VideoCaptureControllerID, const gfx::Size&));
  MOCK_METHOD2(DoTextureBufferReady,
               void(VideoCaptureControllerID, const gfx::Size&));
  MOCK_METHOD1(DoEnded, void(VideoCaptureControllerID));
  MOCK_METHOD1(DoError, void(VideoCaptureControllerID));

  void OnError(VideoCaptureControllerID id) override {
    DoError(id);
  }
  void OnBufferCreated(VideoCaptureControllerID id,
                       base::SharedMemoryHandle handle,
                       int length, int buffer_id) override {
    DoBufferCreated(id);
  }
  void OnBufferDestroyed(VideoCaptureControllerID id, int buffer_id) override {
    DoBufferDestroyed(id);
  }
  void OnBufferReady(VideoCaptureControllerID id,
                     int buffer_id,
                     const scoped_refptr<media::VideoFrame>& frame,
                     const base::TimeTicks& timestamp) override {
    if (!frame->HasTextures()) {
      EXPECT_EQ(frame->format(), media::VideoFrame::I420);
      DoI420BufferReady(id, frame->coded_size());
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&VideoCaptureController::ReturnBuffer,
                                base::Unretained(controller_), id, this,
                                buffer_id, 0, resource_utilization_));
    } else {
      EXPECT_EQ(frame->format(), media::VideoFrame::ARGB);
      DoTextureBufferReady(id, frame->coded_size());
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&VideoCaptureController::ReturnBuffer,
                                base::Unretained(controller_), id, this,
                                buffer_id, frame->mailbox_holder(0).sync_point,
                                resource_utilization_));
    }
  }
  void OnEnded(VideoCaptureControllerID id) override {
    DoEnded(id);
    // OnEnded() must respond by (eventually) unregistering the client.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&VideoCaptureController::RemoveClient),
                   base::Unretained(controller_), id, this));
  }

  VideoCaptureController* controller_;
  double resource_utilization_;
};

// Test class.
class VideoCaptureControllerTest : public testing::Test {
 public:
  VideoCaptureControllerTest() {}
  ~VideoCaptureControllerTest() override {}

 protected:
  static const int kPoolSize = 3;

  void SetUp() override {
    controller_.reset(new VideoCaptureController(kPoolSize));
    device_ = controller_->NewDeviceClient(
        base::ThreadTaskRunnerHandle::Get());
    client_a_.reset(new MockVideoCaptureControllerEventHandler(
        controller_.get()));
    client_b_.reset(new MockVideoCaptureControllerEventHandler(
        controller_.get()));
  }

  void TearDown() override { base::RunLoop().RunUntilIdle(); }

  scoped_refptr<media::VideoFrame> WrapI420Buffer(gfx::Size dimensions,
                                                  uint8* data) {
    return media::VideoFrame::WrapExternalData(
        media::VideoFrame::I420,
        dimensions,
        gfx::Rect(dimensions),
        dimensions,
        data,
        media::VideoFrame::AllocationSize(media::VideoFrame::I420, dimensions),
        base::TimeDelta());
  }

  scoped_refptr<media::VideoFrame> WrapMailboxBuffer(
      const gpu::MailboxHolder& holder,
      const media::VideoFrame::ReleaseMailboxCB& release_cb,
      gfx::Size dimensions) {
    return media::VideoFrame::WrapNativeTexture(
        media::VideoFrame::ARGB, holder, release_cb, dimensions,
        gfx::Rect(dimensions), dimensions, base::TimeDelta());
  }

  TestBrowserThreadBundle bundle_;
  scoped_ptr<MockVideoCaptureControllerEventHandler> client_a_;
  scoped_ptr<MockVideoCaptureControllerEventHandler> client_b_;
  scoped_ptr<VideoCaptureController> controller_;
  scoped_ptr<media::VideoCaptureDevice::Client> device_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureControllerTest);
};

// A simple test of VideoCaptureController's ability to add, remove, and keep
// track of clients.
TEST_F(VideoCaptureControllerTest, AddAndRemoveClients) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);
  media::VideoCaptureParams session_200 = session_100;

  media::VideoCaptureParams session_300 = session_100;

  media::VideoCaptureParams session_400 = session_100;

  // Intentionally use the same route ID for two of the clients: the device_ids
  // are a per-VideoCaptureHost namespace, and can overlap across hosts.
  const VideoCaptureControllerID client_a_route_1(44);
  const VideoCaptureControllerID client_a_route_2(30);
  const VideoCaptureControllerID client_b_route_1(30);
  const VideoCaptureControllerID client_b_route_2(1);

  // Clients in controller: []
  ASSERT_EQ(0, controller_->GetClientCount())
      << "Client count should initially be zero.";
  controller_->AddClient(client_a_route_1,
                         client_a_.get(),
                         base::kNullProcessHandle,
                         100,
                         session_100);
  // Clients in controller: [A/1]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Adding client A/1 should bump client count.";
  controller_->AddClient(client_a_route_2,
                         client_a_.get(),
                         base::kNullProcessHandle,
                         200,
                         session_200);
  // Clients in controller: [A/1, A/2]
  ASSERT_EQ(2, controller_->GetClientCount())
      << "Adding client A/2 should bump client count.";
  controller_->AddClient(client_b_route_1,
                         client_b_.get(),
                         base::kNullProcessHandle,
                         300,
                         session_300);
  // Clients in controller: [A/1, A/2, B/1]
  ASSERT_EQ(3, controller_->GetClientCount())
      << "Adding client B/1 should bump client count.";
  ASSERT_EQ(200,
      controller_->RemoveClient(client_a_route_2, client_a_.get()))
      << "Removing client A/1 should return its session_id.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2, controller_->GetClientCount());
  ASSERT_EQ(static_cast<int>(kInvalidMediaCaptureSessionId),
      controller_->RemoveClient(client_a_route_2, client_a_.get()))
      << "Removing a nonexistant client should fail.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2, controller_->GetClientCount());
  ASSERT_EQ(300,
      controller_->RemoveClient(client_b_route_1, client_b_.get()))
      << "Removing client B/1 should return its session_id.";
  // Clients in controller: [A/1]
  ASSERT_EQ(1, controller_->GetClientCount());
  controller_->AddClient(client_b_route_2,
                         client_b_.get(),
                         base::kNullProcessHandle,
                         400,
                         session_400);
  // Clients in controller: [A/1, B/2]

  EXPECT_CALL(*client_a_, DoEnded(client_a_route_1)).Times(1);
  controller_->StopSession(100);  // Session 100 == client A/1
  Mock::VerifyAndClearExpectations(client_a_.get());
  ASSERT_EQ(2, controller_->GetClientCount())
      << "Client should be closed but still exist after StopSession.";
  // Clients in controller: [A/1 (closed, removal pending), B/2]
  base::RunLoop().RunUntilIdle();
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Client A/1 should be deleted by now.";
  controller_->StopSession(200);  // Session 200 does not exist anymore
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Stopping non-existant session 200 should be a no-op.";
  controller_->StopSession(256);  // Session 256 never existed.
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Stopping non-existant session 256 should be a no-op.";
  ASSERT_EQ(static_cast<int>(kInvalidMediaCaptureSessionId),
      controller_->RemoveClient(client_a_route_1, client_a_.get()))
      << "Removing already-removed client A/1 should fail.";
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Removing non-existant session 200 should be a no-op.";
  ASSERT_EQ(400,
      controller_->RemoveClient(client_b_route_2, client_b_.get()))
      << "Removing client B/2 should return its session_id.";
  // Clients in controller: []
  ASSERT_EQ(0, controller_->GetClientCount())
      << "Client count should return to zero after all clients are gone.";
}

static void CacheSyncPoint(uint32* called_release_sync_point,
                           uint32 release_sync_point) {
  *called_release_sync_point = release_sync_point;
}

// This test will connect and disconnect several clients while simulating an
// active capture device being started and generating frames. It runs on one
// thread and is intended to behave deterministically.
TEST_F(VideoCaptureControllerTest, NormalCaptureMultipleClients) {
// VideoCaptureController::ReturnBuffer() uses ImageTransportFactory.
#if !defined(OS_ANDROID)
  ImageTransportFactory::InitializeForUnitTests(
      scoped_ptr<ImageTransportFactory>(new NoTransportImageTransportFactory));
#endif

  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_200 = session_100;

  media::VideoCaptureParams session_300 = session_100;

  media::VideoCaptureParams session_1 = session_100;

  const gfx::Size capture_resolution(444, 200);

  // The device format needn't match the VideoCaptureParams (the camera can do
  // what it wants). Pick something random.
  media::VideoCaptureFormat device_format(
      gfx::Size(10, 10), 25, media::PIXEL_FORMAT_RGB24);

  const VideoCaptureControllerID client_a_route_1(0xa1a1a1a1);
  const VideoCaptureControllerID client_a_route_2(0xa2a2a2a2);
  const VideoCaptureControllerID client_b_route_1(0xb1b1b1b1);
  const VideoCaptureControllerID client_b_route_2(0xb2b2b2b2);

  // Start with two clients.
  controller_->AddClient(client_a_route_1,
                         client_a_.get(),
                         base::kNullProcessHandle,
                         100,
                         session_100);
  controller_->AddClient(client_b_route_1,
                         client_b_.get(),
                         base::kNullProcessHandle,
                         300,
                         session_300);
  controller_->AddClient(client_a_route_2,
                         client_a_.get(),
                         base::kNullProcessHandle,
                         200,
                         session_200);
  ASSERT_EQ(3, controller_->GetClientCount());

  // Now, simulate an incoming captured buffer from the capture device. As a
  // side effect this will cause the first buffer to be shared with clients.
  uint8 buffer_no = 1;
  ASSERT_EQ(0.0, device_->GetBufferPoolUtilization());
  scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer(
      device_->ReserveOutputBuffer(capture_resolution, media::PIXEL_FORMAT_I420,
                                   media::PIXEL_STORAGE_CPU));
  ASSERT_TRUE(buffer.get());
  ASSERT_EQ(1.0 / kPoolSize, device_->GetBufferPoolUtilization());
  memset(buffer->data(), buffer_no++, buffer->size());
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1)).Times(1);
    EXPECT_CALL(*client_a_,
                DoI420BufferReady(client_a_route_1, capture_resolution))
        .Times(1);
  }
  {
    InSequence s;
    EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1)).Times(1);
    EXPECT_CALL(*client_b_,
                DoI420BufferReady(client_b_route_1, capture_resolution))
        .Times(1);
  }
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2)).Times(1);
    EXPECT_CALL(*client_a_,
                DoI420BufferReady(client_a_route_2, capture_resolution))
        .Times(1);
  }
  scoped_refptr<media::VideoFrame> video_frame =
      WrapI420Buffer(capture_resolution, static_cast<uint8*>(buffer->data()));
  ASSERT_FALSE(video_frame->metadata()->HasKey(
      media::VideoFrameMetadata::RESOURCE_UTILIZATION));
  client_a_->resource_utilization_ = 0.5;
  client_b_->resource_utilization_ = -1.0;
  device_->OnIncomingCapturedVideoFrame(buffer.Pass(), video_frame,
                                        base::TimeTicks());

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
  // Expect VideoCaptureController set the metadata in |video_frame| to hold a
  // resource utilization of 0.5 (the largest of all reported values).
  double resource_utilization_in_metadata = -1.0;
  ASSERT_TRUE(video_frame->metadata()->GetDouble(
      media::VideoFrameMetadata::RESOURCE_UTILIZATION,
      &resource_utilization_in_metadata));
  ASSERT_EQ(0.5, resource_utilization_in_metadata);

  // Second buffer which ought to use the same shared memory buffer. In this
  // case pretend that the Buffer pointer is held by the device for a long
  // delay. This shouldn't affect anything.
  scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer2 =
      device_->ReserveOutputBuffer(capture_resolution, media::PIXEL_FORMAT_I420,
                                   media::PIXEL_STORAGE_CPU);
  ASSERT_TRUE(buffer2.get());
  memset(buffer2->data(), buffer_no++, buffer2->size());
  video_frame =
      WrapI420Buffer(capture_resolution, static_cast<uint8*>(buffer2->data()));
  ASSERT_FALSE(video_frame->metadata()->HasKey(
      media::VideoFrameMetadata::RESOURCE_UTILIZATION));
  client_a_->resource_utilization_ = 0.5;
  client_b_->resource_utilization_ = 3.14;
  device_->OnIncomingCapturedVideoFrame(buffer2.Pass(), video_frame,
                                        base::TimeTicks());

  // The buffer should be delivered to the clients in any order.
  EXPECT_CALL(*client_a_,
              DoI420BufferReady(client_a_route_1, capture_resolution))
      .Times(1);
  EXPECT_CALL(*client_b_,
              DoI420BufferReady(client_b_route_1, capture_resolution))
      .Times(1);
  EXPECT_CALL(*client_a_,
              DoI420BufferReady(client_a_route_2, capture_resolution))
      .Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
  // Expect VideoCaptureController set the metadata in |video_frame| to hold a
  // resource utilization of 3.14 (the largest of all reported values).
  resource_utilization_in_metadata = -1.0;
  ASSERT_TRUE(video_frame->metadata()->GetDouble(
      media::VideoFrameMetadata::RESOURCE_UTILIZATION,
      &resource_utilization_in_metadata));
  ASSERT_EQ(3.14, resource_utilization_in_metadata);

  // Add a fourth client now that some buffers have come through.
  controller_->AddClient(client_b_route_2,
                         client_b_.get(),
                         base::kNullProcessHandle,
                         1,
                         session_1);
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Third, fourth, and fifth buffers. Pretend they all arrive at the same time.
  for (int i = 0; i < kPoolSize; i++) {
    scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer =
        device_->ReserveOutputBuffer(capture_resolution,
                                     media::PIXEL_FORMAT_I420,
                                     media::PIXEL_STORAGE_CPU);
    ASSERT_TRUE(buffer.get());
    memset(buffer->data(), buffer_no++, buffer->size());
    video_frame =
        WrapI420Buffer(capture_resolution, static_cast<uint8*>(buffer->data()));
    device_->OnIncomingCapturedVideoFrame(buffer.Pass(), video_frame,
                                          base::TimeTicks());
  }
  // ReserveOutputBuffer ought to fail now, because the pool is depleted.
  ASSERT_FALSE(
      device_->ReserveOutputBuffer(capture_resolution, media::PIXEL_FORMAT_I420,
                                   media::PIXEL_STORAGE_CPU).get());

  // The new client needs to be told of 3 buffers; the old clients only 2.
  EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_2)).Times(kPoolSize);
  EXPECT_CALL(*client_b_,
              DoI420BufferReady(client_b_route_2, capture_resolution))
      .Times(kPoolSize);
  EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1))
      .Times(kPoolSize - 1);
  EXPECT_CALL(*client_a_,
              DoI420BufferReady(client_a_route_1, capture_resolution))
      .Times(kPoolSize);
  EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2))
      .Times(kPoolSize - 1);
  EXPECT_CALL(*client_a_,
              DoI420BufferReady(client_a_route_2, capture_resolution))
      .Times(kPoolSize);
  EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1))
      .Times(kPoolSize - 1);
  EXPECT_CALL(*client_b_,
              DoI420BufferReady(client_b_route_1, capture_resolution))
      .Times(kPoolSize);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Now test the interaction of client shutdown and buffer delivery.
  // Kill A1 via renderer disconnect (synchronous).
  controller_->RemoveClient(client_a_route_1, client_a_.get());
  // Kill B1 via session close (posts a task to disconnect).
  EXPECT_CALL(*client_b_, DoEnded(client_b_route_1)).Times(1);
  controller_->StopSession(300);
  // Queue up another buffer.
  scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer3 =
      device_->ReserveOutputBuffer(capture_resolution, media::PIXEL_FORMAT_I420,
                                   media::PIXEL_STORAGE_CPU);
  ASSERT_TRUE(buffer3.get());
  memset(buffer3->data(), buffer_no++, buffer3->size());
  video_frame =
      WrapI420Buffer(capture_resolution, static_cast<uint8*>(buffer3->data()));
  device_->OnIncomingCapturedVideoFrame(buffer3.Pass(), video_frame,
                                        base::TimeTicks());

  scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer4 =
      device_->ReserveOutputBuffer(capture_resolution, media::PIXEL_FORMAT_I420,
                                   media::PIXEL_STORAGE_CPU);
  {
    // Kill A2 via session close (posts a task to disconnect, but A2 must not
    // be sent either of these two buffers).
    EXPECT_CALL(*client_a_, DoEnded(client_a_route_2)).Times(1);
    controller_->StopSession(200);
  }
  ASSERT_TRUE(buffer4.get());
  memset(buffer4->data(), buffer_no++, buffer4->size());
  video_frame =
      WrapI420Buffer(capture_resolution, static_cast<uint8*>(buffer4->data()));
  device_->OnIncomingCapturedVideoFrame(buffer4.Pass(), video_frame,
                                        base::TimeTicks());
  // B2 is the only client left, and is the only one that should
  // get the buffer.
  EXPECT_CALL(*client_b_,
              DoI420BufferReady(client_b_route_2, capture_resolution))
      .Times(2);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Allocate all buffers from the buffer pool, half as SHM buffer and half as
  // mailbox buffers.  Make sure of different counts though.
#if defined(OS_ANDROID)
  int mailbox_buffers = 0;
#else
  int mailbox_buffers = kPoolSize / 2;
#endif
  int shm_buffers = kPoolSize - mailbox_buffers;
  if (shm_buffers == mailbox_buffers) {
    shm_buffers--;
    mailbox_buffers++;
  }

  for (int i = 0; i < shm_buffers; ++i) {
    scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer =
        device_->ReserveOutputBuffer(capture_resolution,
                                     media::PIXEL_FORMAT_I420,
                                     media::PIXEL_STORAGE_CPU);
    ASSERT_TRUE(buffer.get());
    video_frame =
        WrapI420Buffer(capture_resolution, static_cast<uint8*>(buffer->data()));
    device_->OnIncomingCapturedVideoFrame(buffer.Pass(), video_frame,
                                          base::TimeTicks());
  }
  std::vector<uint32> mailbox_syncpoints(mailbox_buffers);
  std::vector<uint32> release_syncpoints(mailbox_buffers);
  for (int i = 0; i < mailbox_buffers; ++i) {
    scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer =
        device_->ReserveOutputBuffer(capture_resolution,
                                     media::PIXEL_FORMAT_ARGB,
                                     media::PIXEL_STORAGE_TEXTURE);
    ASSERT_TRUE(buffer.get());
#if !defined(OS_ANDROID)
    mailbox_syncpoints[i] =
        ImageTransportFactory::GetInstance()->GetGLHelper()->InsertSyncPoint();
#endif
    device_->OnIncomingCapturedVideoFrame(
        buffer.Pass(),
        WrapMailboxBuffer(gpu::MailboxHolder(gpu::Mailbox::Generate(), 0,
                                             mailbox_syncpoints[i]),
                          base::Bind(&CacheSyncPoint, &release_syncpoints[i]),
                          capture_resolution),
        base::TimeTicks());
  }
  // ReserveOutputBuffers ought to fail now regardless of buffer format, because
  // the pool is depleted.
  ASSERT_FALSE(
      device_->ReserveOutputBuffer(capture_resolution, media::PIXEL_FORMAT_I420,
                                   media::PIXEL_STORAGE_CPU).get());
  ASSERT_FALSE(
      device_->ReserveOutputBuffer(capture_resolution, media::PIXEL_FORMAT_ARGB,
                                   media::PIXEL_STORAGE_TEXTURE).get());
  EXPECT_CALL(*client_b_,
              DoI420BufferReady(client_b_route_2, capture_resolution))
      .Times(shm_buffers);
  EXPECT_CALL(*client_b_,
              DoTextureBufferReady(client_b_route_2, capture_resolution))
      .Times(mailbox_buffers);
#if !defined(OS_ANDROID)
  EXPECT_CALL(*client_b_, DoBufferDestroyed(client_b_route_2));
#endif
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < mailbox_syncpoints.size(); ++i) {
    // A new release sync point must be inserted when the video frame is
    // returned to the Browser process.
    // See: MockVideoCaptureControllerEventHandler::OnMailboxBufferReady() and
    // VideoCaptureController::ReturnBuffer()
    ASSERT_NE(mailbox_syncpoints[i], release_syncpoints[i]);
  }
  Mock::VerifyAndClearExpectations(client_b_.get());

#if !defined(OS_ANDROID)
  ImageTransportFactory::Terminate();
#endif
}

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorBeforeDeviceCreation) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_200 = session_100;

  const gfx::Size capture_resolution(320, 240);

  const VideoCaptureControllerID route_id(0x99);

  // Start with one client.
  controller_->AddClient(
      route_id, client_a_.get(), base::kNullProcessHandle, 100, session_100);
  device_->OnError("Test Error");
  EXPECT_CALL(*client_a_, DoError(route_id)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(*client_b_, DoError(route_id)).Times(1);
  controller_->AddClient(
      route_id, client_b_.get(), base::kNullProcessHandle, 200, session_200);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_b_.get());

  scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer(
      device_->ReserveOutputBuffer(capture_resolution, media::PIXEL_FORMAT_I420,
                                   media::PIXEL_STORAGE_CPU));
  ASSERT_TRUE(buffer.get());
  scoped_refptr<media::VideoFrame> video_frame =
      WrapI420Buffer(capture_resolution, static_cast<uint8*>(buffer->data()));
  device_->OnIncomingCapturedVideoFrame(buffer.Pass(), video_frame,
                                        base::TimeTicks());

  base::RunLoop().RunUntilIdle();
}

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorAfterDeviceCreation) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_200 = session_100;

  const VideoCaptureControllerID route_id(0x99);

  // Start with one client.
  controller_->AddClient(
      route_id, client_a_.get(), base::kNullProcessHandle, 100, session_100);
  media::VideoCaptureFormat device_format(
      gfx::Size(10, 10), 25, media::PIXEL_FORMAT_ARGB);

  // Start the device. Then, before the first buffer, signal an error and
  // deliver the buffer. The error should be propagated to clients; the buffer
  // should not be.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  const gfx::Size dims(320, 240);
  scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer(
      device_->ReserveOutputBuffer(dims, media::PIXEL_FORMAT_I420,
                                   media::PIXEL_STORAGE_CPU));
  ASSERT_TRUE(buffer.get());

  scoped_refptr<media::VideoFrame> video_frame =
      WrapI420Buffer(dims, static_cast<uint8*>(buffer->data()));
  device_->OnError("Test Error");
  device_->OnIncomingCapturedVideoFrame(buffer.Pass(), video_frame,
                                        base::TimeTicks());

  EXPECT_CALL(*client_a_, DoError(route_id)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(*client_b_, DoError(route_id)).Times(1);
  controller_->AddClient(
      route_id, client_b_.get(), base::kNullProcessHandle, 200, session_200);
  Mock::VerifyAndClearExpectations(client_b_.get());
}

// Tests that buffer-based capture API accepts all memory-backed pixel formats.
TEST_F(VideoCaptureControllerTest, DataCaptureInEachVideoFormatInSequence) {
  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not accept all pixel formats. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes];
  // Initialize memory to satisfy DrMemory tests.
  memset(data, 0, kScratchpadSizeInBytes);
  const gfx::Size capture_resolution(10, 10);
  ASSERT_GE(kScratchpadSizeInBytes, capture_resolution.GetArea() * 4u)
      << "Scratchpad is too small to hold the largest pixel format (ARGB).";

  const int kSessionId = 100;
  for (int format = 0; format < media::PIXEL_FORMAT_MAX; ++format) {
    if (format == media::PIXEL_FORMAT_UNKNOWN)
      continue;
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        capture_resolution, 30, media::VideoPixelFormat(format));

    // Start with one client.
    const VideoCaptureControllerID route_id(0x99);
    controller_->AddClient(route_id,
                           client_a_.get(),
                           base::kNullProcessHandle,
                           kSessionId,
                           params);
    ASSERT_EQ(1, controller_->GetClientCount());
    device_->OnIncomingCapturedData(
        data,
        params.requested_format.ImageAllocationSize(),
        params.requested_format,
        0 /* clockwise_rotation */,
        base::TimeTicks());
    EXPECT_EQ(kSessionId, controller_->RemoveClient(route_id, client_a_.get()));
    Mock::VerifyAndClearExpectations(client_a_.get());
  }
}

// Test that we receive the expected resolution for a given captured frame
// resolution and rotation. Odd resolutions are also cropped.
TEST_F(VideoCaptureControllerTest, CheckRotationsAndCrops) {
  const int kSessionId = 100;
  const struct SizeAndRotation {
    gfx::Size input_resolution;
    int rotation;
    gfx::Size output_resolution;
  } kSizeAndRotations[] = {{{6, 4}, 0, {6, 4}},
                           {{6, 4}, 90, {4, 6}},
                           {{6, 4}, 180, {6, 4}},
                           {{6, 4}, 270, {4, 6}},
                           {{7, 4}, 0, {6, 4}},
                           {{7, 4}, 90, {4, 6}},
                           {{7, 4}, 180, {6, 4}},
                           {{7, 4}, 270, {4, 6}}};
  // The usual ReserveOutputBuffer() -> OnIncomingCapturedVideoFrame() cannot
  // be used since it does not resolve rotations or crops. The memory backed
  // buffer OnIncomingCapturedData() is used instead, with a dummy scratchpad
  // buffer.
  const size_t kScratchpadSizeInBytes = 400;
  unsigned char data[kScratchpadSizeInBytes] = {};

  media::VideoCaptureParams params;
  for (const auto& size_and_rotation : kSizeAndRotations) {
    ASSERT_GE(kScratchpadSizeInBytes,
              size_and_rotation.input_resolution.GetArea() * 4u)
        << "Scratchpad is too small to hold the largest pixel format (ARGB).";

    params.requested_format = media::VideoCaptureFormat(
        size_and_rotation.input_resolution, 30, media::PIXEL_FORMAT_ARGB);

    const VideoCaptureControllerID route_id(0x99);
    controller_->AddClient(route_id, client_a_.get(), base::kNullProcessHandle,
                           kSessionId, params);
    ASSERT_EQ(1, controller_->GetClientCount());

    device_->OnIncomingCapturedData(
        data,
        params.requested_format.ImageAllocationSize(),
        params.requested_format,
        size_and_rotation.rotation,
        base::TimeTicks());
    gfx::Size coded_size;
    {
      InSequence s;
      EXPECT_CALL(*client_a_, DoBufferCreated(route_id)).Times(1);
      EXPECT_CALL(*client_a_, DoI420BufferReady(route_id, _))
          .Times(1)
          .WillOnce(SaveArg<1>(&coded_size));
    }
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(coded_size.width(), size_and_rotation.output_resolution.width());
    EXPECT_EQ(coded_size.height(),
              size_and_rotation.output_resolution.height());

    EXPECT_EQ(kSessionId, controller_->RemoveClient(route_id, client_a_.get()));
    Mock::VerifyAndClearExpectations(client_a_.get());
  }
}

}  // namespace content
