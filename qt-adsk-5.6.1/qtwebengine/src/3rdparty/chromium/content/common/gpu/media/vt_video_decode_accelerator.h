// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <map>
#include <queue>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "content/common/gpu/media/vt.h"
#include "media/filters/h264_parser.h"
#include "media/video/h264_poc.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_context_cgl.h"

namespace content {

// Preload VideoToolbox libraries, needed for sandbox warmup.
bool InitializeVideoToolbox();

// VideoToolbox.framework implementation of the VideoDecodeAccelerator
// interface for Mac OS X (currently limited to 10.9+).
class VTVideoDecodeAccelerator : public media::VideoDecodeAccelerator {
 public:
  explicit VTVideoDecodeAccelerator(
      const base::Callback<bool(void)>& make_context_current);
  ~VTVideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation.
  bool Initialize(media::VideoCodecProfile profile, Client* client) override;
  void Decode(const media::BitstreamBuffer& bitstream) override;
  void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& pictures) override;
  void ReusePictureBuffer(int32_t picture_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool CanDecodeOnIOThread() override;

  // Called by OutputThunk() when VideoToolbox finishes decoding a frame.
  void Output(
      void* source_frame_refcon,
      OSStatus status,
      CVImageBufferRef image_buffer);

  static media::VideoDecodeAccelerator::SupportedProfiles
      GetSupportedProfiles();

 private:
  // Logged to UMA, so never reuse values. Make sure to update
  // VTVDASessionFailureType in histograms.xml to match.
  enum VTVDASessionFailureType {
    SFT_SUCCESSFULLY_INITIALIZED = 0,
    SFT_PLATFORM_ERROR = 1,
    SFT_INVALID_STREAM = 2,
    SFT_UNSUPPORTED_STREAM_PARAMETERS = 3,
    SFT_DECODE_ERROR = 4,
    SFT_UNSUPPORTED_STREAM = 5,
    // Must always be equal to largest entry logged.
    SFT_MAX = SFT_UNSUPPORTED_STREAM
  };

  enum State {
    STATE_DECODING,
    STATE_ERROR,
    STATE_DESTROYING,
  };

  enum TaskType {
    TASK_FRAME,
    TASK_FLUSH,
    TASK_RESET,
    TASK_DESTROY,
  };

  struct Frame {
    explicit Frame(int32_t bitstream_id);
    ~Frame();

    // ID of the bitstream buffer this Frame will be decoded from.
    int32_t bitstream_id;

    // Relative presentation order of this frame (see AVC spec).
    int32_t pic_order_cnt;

    // Nnumber of frames after this one in decode order that can appear before
    // before it in presentation order.
    int32_t reorder_window;

    // Size of the decoded frame.
    // TODO(sandersd): visible_rect.
    gfx::Size coded_size;

    // VideoToolbox decoded image, if decoding was successful.
    base::ScopedCFTypeRef<CVImageBufferRef> image;
  };

  struct Task {
    Task(TaskType type);
    ~Task();

    TaskType type;
    linked_ptr<Frame> frame;
  };

  //
  // Methods for interacting with VideoToolbox. Run on |decoder_thread_|.
  //

  // Compute the |pic_order_cnt| for a frame. Returns true or calls
  // NotifyError() before returning false.
  bool ComputePicOrderCnt(
      const media::H264SPS* sps,
      const media::H264SliceHeader& slice_hdr,
      Frame* frame);

  // Set up VideoToolbox using the current SPS and PPS. Returns true or calls
  // NotifyError() before returning false.
  bool ConfigureDecoder();

  // Wait for VideoToolbox to output all pending frames. Returns true or calls
  // NotifyError() before returning false.
  bool FinishDelayedFrames();

  // |frame| is owned by |pending_frames_|.
  void DecodeTask(const media::BitstreamBuffer&, Frame* frame);
  void DecodeDone(Frame* frame);

  //
  // Methods for interacting with |client_|. Run on |gpu_task_runner_|.
  //
  void NotifyError(
      Error vda_error_type,
      VTVDASessionFailureType session_failure_type);

  // |type| is the type of task that the flush will complete, one of TASK_FLUSH,
  // TASK_RESET, or TASK_DESTROY.
  void QueueFlush(TaskType type);
  void FlushTask(TaskType type);
  void FlushDone(TaskType type);

  // Try to make progress on tasks in the |task_queue_| or sending frames in the
  // |reorder_queue_|.
  void ProcessWorkQueues();

  // These methods returns true if a task was completed, false otherwise.
  bool ProcessTaskQueue();
  bool ProcessReorderQueue();
  bool ProcessFrame(const Frame& frame);
  bool SendFrame(const Frame& frame);

  //
  // GPU thread state.
  //
  base::Callback<bool(void)> make_context_current_;
  media::VideoDecodeAccelerator::Client* client_;
  State state_;

  // Queue of pending flush tasks. This is used to drop frames when a reset
  // is pending.
  std::queue<TaskType> pending_flush_tasks_;

  // Queue of tasks to complete in the GPU thread.
  std::queue<Task> task_queue_;

  // Utility class to define the order of frames in the reorder queue.
  struct FrameOrder {
    bool operator()(
        const linked_ptr<Frame>& lhs,
        const linked_ptr<Frame>& rhs) const;
  };

  // Queue of decoded frames in presentation order.
  std::priority_queue<linked_ptr<Frame>,
                      std::vector<linked_ptr<Frame>>,
                      FrameOrder> reorder_queue_;

  // Size of assigned picture buffers.
  gfx::Size picture_size_;

  // Frames that have not yet been decoded, keyed by bitstream ID; maintains
  // ownership of Frame objects while they flow through VideoToolbox.
  std::map<int32_t, linked_ptr<Frame>> pending_frames_;

  // Set of assigned bitstream IDs, so that Destroy() can release them all.
  std::set<int32_t> assigned_bitstream_ids_;

  // All picture buffers assigned to us. Used to check if reused picture buffers
  // should be added back to the available list or released. (They are not
  // released immediately because we need the reuse event to free the binding.)
  std::set<int32_t> assigned_picture_ids_;

  // Texture IDs of assigned pictures.
  std::map<int32_t, uint32_t> texture_ids_;

  // Pictures ready to be rendered to.
  std::vector<int32_t> available_picture_ids_;

  // Image buffers kept alive while they are bound to pictures.
  std::map<int32_t, base::ScopedCFTypeRef<CVImageBufferRef>> picture_bindings_;

  //
  // Decoder thread state.
  //
  VTDecompressionOutputCallbackRecord callback_;
  base::ScopedCFTypeRef<CMFormatDescriptionRef> format_;
  base::ScopedCFTypeRef<VTDecompressionSessionRef> session_;
  media::H264Parser parser_;
  gfx::Size coded_size_;

  int last_sps_id_;
  std::vector<uint8_t> last_sps_;
  std::vector<uint8_t> last_spsext_;
  int last_pps_id_;
  std::vector<uint8_t> last_pps_;
  media::H264POC poc_;

  //
  // Shared state (set up and torn down on GPU thread).
  //
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  base::ThreadChecker gpu_thread_checker_;
  base::WeakPtr<VTVideoDecodeAccelerator> weak_this_;
  base::Thread decoder_thread_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtrFactory<VTVideoDecodeAccelerator> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VTVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VT_VIDEO_DECODE_ACCELERATOR_H_
