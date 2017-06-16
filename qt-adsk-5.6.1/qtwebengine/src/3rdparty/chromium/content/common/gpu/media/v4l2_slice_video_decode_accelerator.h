// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_V4L2_SLICE_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_V4L2_SLICE_VIDEO_DECODE_ACCELERATOR_H_

#include <linux/videodev2.h>
#include <queue>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/h264_decoder.h"
#include "content/common/gpu/media/v4l2_device.h"
#include "content/common/gpu/media/vp8_decoder.h"
#include "media/video/video_decode_accelerator.h"

namespace content {

// An implementation of VideoDecodeAccelerator that utilizes the V4L2 slice
// level codec API for decoding. The slice level API provides only a low-level
// decoding functionality and requires userspace to provide support for parsing
// the input stream and managing decoder state across frames.
class CONTENT_EXPORT V4L2SliceVideoDecodeAccelerator
    : public media::VideoDecodeAccelerator {
 public:
  class V4L2DecodeSurface;

  V4L2SliceVideoDecodeAccelerator(
      const scoped_refptr<V4L2Device>& device,
      EGLDisplay egl_display,
      EGLContext egl_context,
      const base::WeakPtr<Client>& io_client_,
      const base::Callback<bool(void)>& make_context_current,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~V4L2SliceVideoDecodeAccelerator() override;

  // media::VideoDecodeAccelerator implementation.
  bool Initialize(media::VideoCodecProfile profile,
                  VideoDecodeAccelerator::Client* client) override;
  void Decode(const media::BitstreamBuffer& bitstream_buffer) override;
  void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32 picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool CanDecodeOnIOThread() override;

  static media::VideoDecodeAccelerator::SupportedProfiles
      GetSupportedProfiles();

 private:
  class V4L2H264Accelerator;
  class V4L2VP8Accelerator;

  // Record for input buffers.
  struct InputRecord {
    InputRecord();
    int32 input_id;
    void* address;
    size_t length;
    size_t bytes_used;
    bool at_device;
  };

  // Record for output buffers.
  struct OutputRecord {
    OutputRecord();
    bool at_device;
    bool at_client;
    int32 picture_id;
    EGLImageKHR egl_image;
    EGLSyncKHR egl_sync;
    bool cleared;
  };

  // See http://crbug.com/255116.
  // Input bitstream buffer size for up to 1080p streams.
  const size_t kInputBufferMaxSizeFor1080p = 1024 * 1024;
  // Input bitstream buffer size for up to 4k streams.
  const size_t kInputBufferMaxSizeFor4k = 4 * kInputBufferMaxSizeFor1080p;
  const size_t kNumInputBuffers = 16;

  //
  // Below methods are used by accelerator implementations.
  //
  // Append slice data in |data| of size |size| to pending hardware
  // input buffer with |index|. This buffer will be submitted for decode
  // on the next DecodeSurface(). Return true on success.
  bool SubmitSlice(int index, const uint8_t* data, size_t size);

  // Submit controls in |ext_ctrls| to hardware. Return true on success.
  bool SubmitExtControls(struct v4l2_ext_controls* ext_ctrls);

  // Decode of |dec_surface| is ready to be submitted and all codec-specific
  // settings are set in hardware.
  void DecodeSurface(const scoped_refptr<V4L2DecodeSurface>& dec_surface);

  // |dec_surface| is ready to be outputted once decode is finished.
  // This can be called before decode is actually done in hardware, and this
  // method is responsible for maintaining the ordering, i.e. the surfaces will
  // be outputted in the same order as SurfaceReady calls. To do so, the
  // surfaces are put on decoder_display_queue_ and sent to output in that
  // order once all preceding surfaces are sent.
  void SurfaceReady(const scoped_refptr<V4L2DecodeSurface>& dec_surface);

  //
  // Internal methods of this class.
  //
  // Recycle a V4L2 input buffer with |index| after dequeuing from device.
  void ReuseInputBuffer(int index);

  // Recycle V4L2 output buffer with |index|. Used as surface release callback.
  void ReuseOutputBuffer(int index);

  // Queue a |dec_surface| to device for decoding.
  void Enqueue(const scoped_refptr<V4L2DecodeSurface>& dec_surface);

  // Dequeue any V4L2 buffers available and process.
  void Dequeue();

  // V4L2 QBUF helpers.
  bool EnqueueInputRecord(int index, uint32_t config_store);
  bool EnqueueOutputRecord(int index);

  // Set input and output formats in hardware.
  bool SetupFormats();

  // Create input and output buffers.
  bool CreateInputBuffers();
  bool CreateOutputBuffers();

  // Destroy input buffers.
  void DestroyInputBuffers();

  // Destroy output buffers and release associated resources (textures,
  // EGLImages). If |dismiss| is true, also dismissing the associated
  // PictureBuffers.
  bool DestroyOutputs(bool dismiss);

  // Used by DestroyOutputs.
  bool DestroyOutputBuffers();

  // Dismiss all |picture_buffer_ids| via Client::DismissPictureBuffer()
  // and signal |done| after finishing.
  void DismissPictures(std::vector<int32> picture_buffer_ids,
                       base::WaitableEvent* done);

  // Task to finish initialization on decoder_thread_.
  void InitializeTask();

  // Surface set change (resolution change) flow.
  // If we have no surfaces allocated, just allocate them and return.
  // Otherwise mark us as pending for surface set change.
  void InitiateSurfaceSetChange();
  // If a surface set change is pending and we are ready, stop the device,
  // destroy outputs, releasing resources and dismissing pictures as required,
  // followed by allocating a new set for the new resolution/DPB size
  // as provided by decoder. Finally, try to resume decoding.
  void FinishSurfaceSetChangeIfNeeded();

  void NotifyError(Error error);
  void DestroyTask();

  // Sets the state to kError and notifies client if needed.
  void SetErrorState(Error error);

  // Flush flow when requested by client.
  // When Flush() is called, it posts a FlushTask, which checks the input queue.
  // If nothing is pending for decode on decoder_input_queue_, we call
  // InitiateFlush() directly. Otherwise, we push a dummy BitstreamBufferRef
  // onto the decoder_input_queue_ to schedule a flush. When we reach it later
  // on, we call InitiateFlush() to perform it at the correct time.
  void FlushTask();
  // Tell the decoder to flush all frames, reset it and mark us as scheduled
  // for flush, so that we can finish it once all pending decodes are finished.
  void InitiateFlush();
  // If all pending frames are decoded and we are waiting to flush, perform it.
  // This will send all pending pictures to client and notify the client that
  // flush is complete and puts us in a state ready to resume.
  void FinishFlushIfNeeded();

  // Reset flow when requested by client.
  // Drop all inputs and reset the decoder and mark us as pending for reset.
  void ResetTask();
  // If all pending frames are decoded and we are waiting to reset, perform it.
  // This drops all pending outputs (client is not interested anymore),
  // notifies the client we are done and puts us in a state ready to resume.
  void FinishResetIfNeeded();

  // Process pending events if any.
  void ProcessPendingEventsIfNeeded();

  // Performed on decoder_thread_ as a consequence of poll() on decoder_thread_
  // returning an event.
  void ServiceDeviceTask();

  // Schedule poll if we have any buffers queued and the poll thread
  // is not stopped (on surface set change).
  void SchedulePollIfNeeded();

  // Attempt to start/stop device_poll_thread_.
  bool StartDevicePoll();
  bool StopDevicePoll(bool keep_input_state);

  // Ran on device_poll_thread_ to wait for device events.
  void DevicePollTask(bool poll_device);

  enum State {
    // We are in this state until Initialize() returns successfully.
    // We can't post errors to the client in this state yet.
    kUninitialized,
    // Initialize() returned successfully.
    kInitialized,
    // This state allows making progress decoding more input stream.
    kDecoding,
    // Transitional state when we are not decoding any more stream, but are
    // performing flush, reset, resolution change or are destroying ourselves.
    kIdle,
    // Error state, set when sending NotifyError to client.
    kError,
  };

  // Buffer id for flush buffer, queued by FlushTask().
  const int kFlushBufferId = -2;

  // Handler for Decode() on decoder_thread_.
  void DecodeTask(const media::BitstreamBuffer& bitstream_buffer);

  // Schedule a new DecodeBufferTask if we are decoding.
  void ScheduleDecodeBufferTaskIfNeeded();

  // Main decoder loop. Keep decoding the current buffer in decoder_, asking
  // for more stream via TrySetNewBistreamBuffer() if decoder_ requests so,
  // and handle other returns from it appropriately.
  void DecodeBufferTask();

  // Check decoder_input_queue_ for any available buffers to decode and
  // set the decoder_current_bitstream_buffer_ to the next buffer if one is
  // available, taking it off the queue. Also set the current stream pointer
  // in decoder_, and return true.
  // Return false if no buffers are pending on decoder_input_queue_.
  bool TrySetNewBistreamBuffer();

  // Auto-destruction reference for EGLSync (for message-passing).
  struct EGLSyncKHRRef;
  void ReusePictureBufferTask(int32 picture_buffer_id,
                              scoped_ptr<EGLSyncKHRRef> egl_sync_ref);

  // Called to actually send |dec_surface| to the client, after it is decoded
  // preserving the order in which it was scheduled via SurfaceReady().
  void OutputSurface(const scoped_refptr<V4L2DecodeSurface>& dec_surface);

  // Goes over the |decoder_display_queue_| and sends all buffers from the
  // front of the queue that are already decoded to the client, in order.
  void TryOutputSurfaces();

  // Creates a new decode surface or returns nullptr if one is not available.
  scoped_refptr<V4L2DecodeSurface> CreateSurface();

  // Send decoded pictures to PictureReady.
  void SendPictureReady();

  // Callback that indicates a picture has been cleared.
  void PictureCleared();

  size_t input_planes_count_;
  size_t output_planes_count_;

  // GPU Child thread task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // IO thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the decoder or
  // device worker threads back to the child thread.
  base::WeakPtr<V4L2SliceVideoDecodeAccelerator> weak_this_;

  // To expose client callbacks from VideoDecodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  scoped_ptr<base::WeakPtrFactory<VideoDecodeAccelerator::Client>>
      client_ptr_factory_;
  base::WeakPtr<VideoDecodeAccelerator::Client> client_;
  // Callbacks to |io_client_| must be executed on |io_task_runner_|.
  base::WeakPtr<Client> io_client_;

  // V4L2 device in use.
  scoped_refptr<V4L2Device> device_;

  // Thread to communicate with the device on.
  base::Thread decoder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> decoder_thread_task_runner_;

  // Thread used to poll the device for events.
  base::Thread device_poll_thread_;

  // Input queue state.
  bool input_streamon_;
  // Number of input buffers enqueued to the device.
  int input_buffer_queued_count_;
  // Input buffers ready to use; LIFO since we don't care about ordering.
  std::list<int> free_input_buffers_;
  // Mapping of int index to an input buffer record.
  std::vector<InputRecord> input_buffer_map_;

  // Output queue state.
  bool output_streamon_;
  // Number of output buffers enqueued to the device.
  int output_buffer_queued_count_;
  // Output buffers ready to use.
  std::list<int> free_output_buffers_;
  // Mapping of int index to an output buffer record.
  std::vector<OutputRecord> output_buffer_map_;

  media::VideoCodecProfile video_profile_;
  uint32_t output_format_fourcc_;
  gfx::Size visible_size_;
  gfx::Size coded_size_;

  struct BitstreamBufferRef;
  // Input queue of stream buffers coming from the client.
  std::queue<linked_ptr<BitstreamBufferRef>> decoder_input_queue_;
  // BitstreamBuffer currently being processed.
  scoped_ptr<BitstreamBufferRef> decoder_current_bitstream_buffer_;

  // Queue storing decode surfaces ready to be output as soon as they are
  // decoded. The surfaces must be output in order they are queued.
  std::queue<scoped_refptr<V4L2DecodeSurface>> decoder_display_queue_;

  // Decoder state.
  State state_;

  // If any of these are true, we are waiting for the device to finish decoding
  // all previously-queued frames, so we can finish the flush/reset/surface
  // change flows. These can stack.
  bool decoder_flushing_;
  bool decoder_resetting_;
  bool surface_set_change_pending_;

  // Hardware accelerators.
  // TODO(posciak): Try to have a superclass here if possible.
  scoped_ptr<V4L2H264Accelerator> h264_accelerator_;
  scoped_ptr<V4L2VP8Accelerator> vp8_accelerator_;

  // Codec-specific software decoder in use.
  scoped_ptr<AcceleratedVideoDecoder> decoder_;

  // Surfaces queued to device to keep references to them while decoded.
  using V4L2DecodeSurfaceByOutputId =
      std::map<int, scoped_refptr<V4L2DecodeSurface>>;
  V4L2DecodeSurfaceByOutputId surfaces_at_device_;

  // Surfaces sent to client to keep references to them while displayed.
  using V4L2DecodeSurfaceByPictureBufferId =
      std::map<int32, scoped_refptr<V4L2DecodeSurface>>;
  V4L2DecodeSurfaceByPictureBufferId surfaces_at_display_;

  // Record for decoded pictures that can be sent to PictureReady.
  struct PictureRecord;
  // Pictures that are ready but not sent to PictureReady yet.
  std::queue<PictureRecord> pending_picture_ready_;

  // The number of pictures that are sent to PictureReady and will be cleared.
  int picture_clearing_count_;

  // Used by the decoder thread to wait for AssignPictureBuffers to arrive
  // to avoid races with potential Reset requests.
  base::WaitableEvent pictures_assigned_;

  // Make the GL context current callback.
  base::Callback<bool(void)> make_context_current_;

  // EGL state
  EGLDisplay egl_display_;
  EGLContext egl_context_;

  // The WeakPtrFactory for |weak_this_|.
  base::WeakPtrFactory<V4L2SliceVideoDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2SliceVideoDecodeAccelerator);
};

class V4L2H264Picture;
class V4L2VP8Picture;

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_V4L2_SLICE_VIDEO_DECODE_ACCELERATOR_H_
