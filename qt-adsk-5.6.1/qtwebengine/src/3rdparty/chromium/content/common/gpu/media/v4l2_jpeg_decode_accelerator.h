// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_V4L2_JPEG_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_V4L2_JPEG_DECODE_ACCELERATOR_H_

#include <queue>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/v4l2_device.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_frame.h"
#include "media/video/jpeg_decode_accelerator.h"

namespace content {

class CONTENT_EXPORT V4L2JpegDecodeAccelerator
    : public media::JpegDecodeAccelerator {
 public:
  V4L2JpegDecodeAccelerator(
      const scoped_refptr<V4L2Device>& device,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~V4L2JpegDecodeAccelerator() override;

  // media::JpegDecodeAccelerator implementation.
  bool Initialize(Client* client) override;
  void Decode(const media::BitstreamBuffer& bitstream_buffer,
              const scoped_refptr<media::VideoFrame>& video_frame) override;

 private:
  // Record for input/output buffers.
  struct BufferRecord {
    BufferRecord();
    ~BufferRecord();
    void* address;          // mmap() address.
    size_t length;          // mmap() length.

    // Set true during QBUF and DQBUF. |address| will be accessed by hardware.
    bool at_device;
  };

  // Job record. Jobs are processed in a FIFO order. This is separate from
  // BufferRecord of input, because a BufferRecord of input may be returned
  // before we dequeue the corresponding output buffer. It can't always be
  // associated with a BufferRecord of output immediately either, because at
  // the time of submission we may not have one available (and don't need one
  // to submit input to the device).
  struct JobRecord {
    JobRecord(media::BitstreamBuffer bitstream_buffer,
              scoped_refptr<media::VideoFrame> video_frame);
    ~JobRecord();

    // Input image buffer.
    media::BitstreamBuffer bitstream_buffer;
    // Output frame buffer.
    scoped_refptr<media::VideoFrame> out_frame;
    // Memory mapped from |bitstream_buffer|.
    scoped_ptr<base::SharedMemory> shm;
  };

  void EnqueueInput();
  void EnqueueOutput();
  void Dequeue();
  bool EnqueueInputRecord();
  bool EnqueueOutputRecord();
  bool CreateInputBuffers();
  bool CreateOutputBuffers();
  void DestroyInputBuffers();
  void DestroyOutputBuffers();
  void ResetQueues();

  // Return the number of input/output buffers enqueued to the device.
  size_t InputBufferQueuedCount();
  size_t OutputBufferQueuedCount();

  // Return true if input buffer should be re-created.
  bool ShouldRecreateInputBuffers();
  // Return true if output buffer should be re-created.
  bool ShouldRecreateOutputBuffers();
  // Create input and output buffer if needed. Return false means that an error
  // has happened.
  bool CreateBuffersIfNecessary();

  void VideoFrameReady(int32_t bitstream_buffer_id);
  void NotifyError(int32_t bitstream_buffer_id, Error error);
  void PostNotifyError(int32_t bitstream_buffer_id, Error error);

  // Run on |decoder_thread_| to enqueue the coming frame.
  void DecodeTask(scoped_ptr<JobRecord> job_record);

  // Run on |decoder_thread_| to dequeue last frame and enqueue next frame.
  // This task is triggered by DevicePollTask.
  void ServiceDeviceTask();

  // Start/Stop |device_poll_thread_|.
  void StartDevicePoll();
  bool StopDevicePoll();

  // Run on |device_poll_thread_| to wait for device events.
  void DevicePollTask();

  // Run on |decoder_thread_| to destroy input and output buffers.
  void DestroyTask();

  // The number of input buffers and output buffers.
  const size_t kBufferCount = 2;

  // Current image size used for checking the size is changed.
  gfx::Size image_coded_size_;

  // Set true when input or output buffers have to be re-allocated.
  bool recreate_input_buffers_pending_;
  bool recreate_output_buffers_pending_;

  // ChildThread's task runner.
  scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // GPU IO task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The client of this class.
  Client* client_;

  // The V4L2Device this class is operating upon.
  scoped_refptr<V4L2Device> device_;

  // Thread to communicate with the device.
  base::Thread decoder_thread_;
  // Decode task runner.
  scoped_refptr<base::SingleThreadTaskRunner> decoder_task_runner_;
  // Thread used to poll the V4L2 for events only.
  base::Thread device_poll_thread_;
  // Device poll task runner.
  scoped_refptr<base::SingleThreadTaskRunner> device_poll_task_runner_;

  // All the below members except |weak_factory_| are accessed from
  // |decoder_thread_| only (if it's running).
  std::queue<linked_ptr<JobRecord>> input_jobs_;
  std::queue<linked_ptr<JobRecord>> running_jobs_;

  // Input queue state.
  bool input_streamon_;
  // Mapping of int index to an input buffer record.
  std::vector<BufferRecord> input_buffer_map_;
  // Indices of input buffers ready to use; LIFO since we don't care about
  // ordering.
  std::vector<int> free_input_buffers_;

  // Output queue state.
  bool output_streamon_;
  // Mapping of int index to an output buffer record.
  std::vector<BufferRecord> output_buffer_map_;
  // Indices of output buffers ready to use; LIFO since we don't care about
  // ordering.
  std::vector<int> free_output_buffers_;

  // Weak factory for producing weak pointers on the child thread.
  base::WeakPtrFactory<V4L2JpegDecodeAccelerator> weak_factory_;
  // Point to |this| for use in posting tasks from the decoder thread back to
  // the ChildThread.
  base::WeakPtr<V4L2JpegDecodeAccelerator> weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(V4L2JpegDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_V4L2_JPEG_DECODE_ACCELERATOR_H_
