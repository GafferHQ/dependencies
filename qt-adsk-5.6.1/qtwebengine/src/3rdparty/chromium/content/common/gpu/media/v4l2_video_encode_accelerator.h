// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_V4L2_VIDEO_ENCODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_V4L2_VIDEO_ENCODE_ACCELERATOR_H_

#include <linux/videodev2.h>
#include <list>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/v4l2_device.h"
#include "content/common/gpu/media/v4l2_image_processor.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class BitstreamBuffer;

}  // namespace media

namespace content {

// This class handles video encode acceleration by interfacing with a V4L2
// device exposed by the codec hardware driver. The threading model of this
// class is the same as in the V4L2VideoDecodeAccelerator (from which class this
// was designed).
// This class may try to instantiate and use a V4L2ImageProcessor for input
// format conversion, if the input format requested via Initialize() is not
// accepted by the hardware codec.
class CONTENT_EXPORT V4L2VideoEncodeAccelerator
    : public media::VideoEncodeAccelerator {
 public:
  explicit V4L2VideoEncodeAccelerator(const scoped_refptr<V4L2Device>& device);
  ~V4L2VideoEncodeAccelerator() override;

  // media::VideoEncodeAccelerator implementation.
  media::VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles()
      override;
  bool Initialize(media::VideoFrame::Format format,
                  const gfx::Size& input_visible_size,
                  media::VideoCodecProfile output_profile,
                  uint32 initial_bitrate,
                  Client* client) override;
  void Encode(const scoped_refptr<media::VideoFrame>& frame,
              bool force_keyframe) override;
  void UseOutputBitstreamBuffer(const media::BitstreamBuffer& buffer)
      override;
  void RequestEncodingParametersChange(uint32 bitrate,
                                       uint32 framerate) override;
  void Destroy() override;

 private:
  // Auto-destroy reference for BitstreamBuffer, for tracking buffers passed to
  // this instance.
  struct BitstreamBufferRef;

  // Record for codec input buffers.
  struct InputRecord {
    InputRecord();
    ~InputRecord();
    bool at_device;
    scoped_refptr<media::VideoFrame> frame;
  };

  // Record for output buffers.
  struct OutputRecord {
    OutputRecord();
    ~OutputRecord();
    bool at_device;
    linked_ptr<BitstreamBufferRef> buffer_ref;
    void* address;
    size_t length;
  };

  enum {
    kInitialFramerate = 30,
    // These are rather subjectively tuned.
    kInputBufferCount = 2,
    kOutputBufferCount = 2,
    kOutputBufferSize = (2 * 1024 * 1024),
  };

  // Internal state of the encoder.
  enum State {
    kUninitialized,  // Initialize() not yet called.
    kInitialized,    // Initialize() returned true; ready to start encoding.
    kEncoding,       // Encoding frames.
    kError,          // Error in encoder state.
  };

  //
  // Callbacks for the image processor, if one is used.
  //

  // Callback run by the image processor when a frame is ready for us to encode.
  void FrameProcessed(bool force_keyframe,
                      const scoped_refptr<media::VideoFrame>& frame);

  // Error callback for handling image processor errors.
  void ImageProcessorError();

  //
  // Encoding tasks, to be run on encode_thread_.
  //

  void EncodeTask(const scoped_refptr<media::VideoFrame>& frame,
                  bool force_keyframe);

  // Add a BitstreamBuffer to the queue of buffers ready to be used for encoder
  // output.
  void UseOutputBitstreamBufferTask(scoped_ptr<BitstreamBufferRef> buffer_ref);

  // Device destruction task.
  void DestroyTask();

  // Service I/O on the V4L2 devices.  This task should only be scheduled from
  // DevicePollTask().
  void ServiceDeviceTask();

  // Handle the device queues.
  void Enqueue();
  void Dequeue();
  // Enqueue a buffer on the corresponding queue.  Returns false on fatal error.
  bool EnqueueInputRecord();
  bool EnqueueOutputRecord();

  // Attempt to start/stop device_poll_thread_.
  bool StartDevicePoll();
  bool StopDevicePoll();

  //
  // Device tasks, to be run on device_poll_thread_.
  //

  // The device task.
  void DevicePollTask(bool poll_device);

  //
  // Safe from any thread.
  //

  // Error notification (using PostTask() to child thread, if necessary).
  void NotifyError(Error error);

  // Set the encoder_state_ to kError and notify the client (if necessary).
  void SetErrorState(Error error);

  //
  // Other utility functions.  Called on encoder_thread_, unless
  // encoder_thread_ is not yet started, in which case the child thread can call
  // these (e.g. in Initialize() or Destroy()).
  //

  // Change encoding parameters.
  void RequestEncodingParametersChangeTask(uint32 bitrate, uint32 framerate);

  // Set up formats and initialize the device for them.
  bool SetFormats(media::VideoFrame::Format input_format,
                  media::VideoCodecProfile output_profile);

  // Try to set up the device to the input format we were Initialized() with,
  // or if the device doesn't support it, use one it can support, so that we
  // can later instantiate a V4L2ImageProcessor to convert to it.
  bool NegotiateInputFormat(media::VideoFrame::Format input_format);

  // Set up the device to the output format requested in Initialize().
  bool SetOutputFormat(media::VideoCodecProfile output_profile);

  // Initialize device controls with default values.
  bool InitControls();

  // Create the buffers we need.
  bool CreateInputBuffers();
  bool CreateOutputBuffers();

  // Destroy these buffers.
  void DestroyInputBuffers();
  void DestroyOutputBuffers();

  // Set controls in |ctrls| and return true if successful.
  bool SetExtCtrls(std::vector<struct v4l2_ext_control> ctrls);

  // Our original calling task runner for the child thread.
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  gfx::Size visible_size_;
  // Input allocated size required by the device.
  gfx::Size input_allocated_size_;
  size_t output_buffer_byte_size_;

  // Formats for input frames and the output stream.
  media::VideoFrame::Format device_input_format_;
  size_t input_planes_count_;
  uint32 output_format_fourcc_;

  //
  // Encoder state, owned and operated by encoder_thread_.
  // Before encoder_thread_ has started, the encoder state is managed by
  // the child (main) thread.  After encoder_thread_ has started, the encoder
  // thread should be the only one managing these.
  //

  // Encoder state.
  State encoder_state_;

  // We need to provide the stream header with every keyframe, to allow
  // midstream decoding restarts.  Store it here.
  scoped_ptr<uint8[]> stream_header_;
  size_t stream_header_size_;

  // Video frames ready to be encoded.
  std::list<scoped_refptr<media::VideoFrame> > encoder_input_queue_;

  // Encoder device.
  scoped_refptr<V4L2Device> device_;

  // Input queue state.
  bool input_streamon_;
  // Input buffers enqueued to device.
  int input_buffer_queued_count_;
  // Input buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> free_input_buffers_;
  // Mapping of int index to input buffer record.
  std::vector<InputRecord> input_buffer_map_;
  enum v4l2_memory input_memory_type_;

  // Output queue state.
  bool output_streamon_;
  // Output buffers enqueued to device.
  int output_buffer_queued_count_;
  // Output buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> free_output_buffers_;
  // Mapping of int index to output buffer record.
  std::vector<OutputRecord> output_buffer_map_;

  // Bitstream buffers ready to be used to return encoded output, as a LIFO
  // since we don't care about ordering.
  std::vector<linked_ptr<BitstreamBufferRef> > encoder_output_queue_;

  // Image processor, if one is in use.
  scoped_ptr<V4L2ImageProcessor> image_processor_;

  // This thread services tasks posted from the VEA API entry points by the
  // child thread and device service callbacks posted from the device thread.
  base::Thread encoder_thread_;

  // The device polling thread handles notifications of V4L2 device changes.
  // TODO(sheu): replace this thread with an TYPE_IO encoder_thread_.
  base::Thread device_poll_thread_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  base::WeakPtr<Client> client_;
  scoped_ptr<base::WeakPtrFactory<Client> > client_ptr_factory_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the
  // image_processor_ back to the child thread.
  // Tasks posted onto encoder and poll threads can use base::Unretained(this),
  // as both threads will not outlive this object.
  base::WeakPtr<V4L2VideoEncodeAccelerator> weak_this_;
  base::WeakPtrFactory<V4L2VideoEncodeAccelerator> weak_this_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2VideoEncodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_V4L2_VIDEO_ENCODE_ACCELERATOR_H_
