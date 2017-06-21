// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_ENCODER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_ENCODER_HOST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "media/video/video_encode_accelerator.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/media_stream_buffer_manager.h"

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace content {

class CommandBufferProxyImpl;
class GpuChannelHost;
class RendererPpapiHost;
class VideoEncoderShim;

class CONTENT_EXPORT PepperVideoEncoderHost
    : public ppapi::host::ResourceHost,
      public media::VideoEncodeAccelerator::Client,
      public ppapi::MediaStreamBufferManager::Delegate {
 public:
  PepperVideoEncoderHost(RendererPpapiHost* host,
                         PP_Instance instance,
                         PP_Resource resource);
  ~PepperVideoEncoderHost() override;

 private:
  friend class VideoEncoderShim;

  // Shared memory buffers.
  struct ShmBuffer {
    ShmBuffer(uint32_t id, scoped_ptr<base::SharedMemory> shm);
    ~ShmBuffer();

    media::BitstreamBuffer ToBitstreamBuffer();

    // Index of the buffer in the ScopedVector. Buffers have the same id in
    // the plugin and the host.
    uint32_t id;
    scoped_ptr<base::SharedMemory> shm;
    bool in_use;
  };

  // media::VideoEncodeAccelerator implementation.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(int32 bitstream_buffer_id,
                            size_t payload_size,
                            bool key_frame) override;
  void NotifyError(media::VideoEncodeAccelerator::Error error) override;

  // ResourceHost implementation.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  int32_t OnHostMsgGetSupportedProfiles(
      ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgInitialize(ppapi::host::HostMessageContext* context,
                              PP_VideoFrame_Format input_format,
                              const PP_Size& input_visible_size,
                              PP_VideoProfile output_profile,
                              uint32_t initial_bitrate,
                              PP_HardwareAcceleration acceleration);
  int32_t OnHostMsgGetVideoFrames(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgEncode(ppapi::host::HostMessageContext* context,
                          uint32_t frame_id,
                          bool force_keyframe);
  int32_t OnHostMsgRecycleBitstreamBuffer(
      ppapi::host::HostMessageContext* context,
      uint32_t buffer_id);
  int32_t OnHostMsgRequestEncodingParametersChange(
      ppapi::host::HostMessageContext* context,
      uint32_t bitrate,
      uint32_t framerate);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context);

  // Internal methods.
  void GetSupportedProfiles(
      std::vector<PP_VideoProfileDescription>* pp_profiles);
  bool IsInitializationValid(const PP_Size& input_size,
                             PP_VideoProfile ouput_profile,
                             PP_HardwareAcceleration acceleration);
  bool EnsureGpuChannel();
  bool InitializeHardware(media::VideoFrame::Format input_format,
                          const gfx::Size& input_visible_size,
                          media::VideoCodecProfile output_profile,
                          uint32_t initial_bitrate);
  void Close();
  void AllocateVideoFrames();
  void SendGetFramesErrorReply(int32_t error);
  scoped_refptr<media::VideoFrame> CreateVideoFrame(
      uint32_t frame_id,
      const ppapi::host::ReplyMessageContext& reply_context);
  void FrameReleased(const ppapi::host::ReplyMessageContext& reply_context,
                     uint32_t frame_id);
  void NotifyPepperError(int32_t error);

  // Helper method for VideoEncoderShim.
  uint8_t* ShmHandleToAddress(int32 buffer_id);

  // Non-owning pointer.
  RendererPpapiHost* renderer_ppapi_host_;

  ScopedVector<ShmBuffer> shm_buffers_;

  // Buffer manager for shared memory that holds video frames.
  ppapi::MediaStreamBufferManager buffer_manager_;

  scoped_refptr<GpuChannelHost> channel_;
  CommandBufferProxyImpl* command_buffer_;

  scoped_ptr<media::VideoEncodeAccelerator> encoder_;

  // Whether the encoder has been successfully initialized.
  bool initialized_;

  // Saved context to answer an Initialize message from the plugin.
  ppapi::host::ReplyMessageContext initialize_reply_context_;

  // Saved context to answer a GetVideoFrames message from the plugin.
  ppapi::host::ReplyMessageContext get_video_frames_reply_context_;

  // This represents the current error state of the encoder, i.e. PP_OK
  // normally, or a Pepper error code if the encoder is uninitialized,
  // has been notified of an encoder error, has encountered some
  // other unrecoverable error, or has been closed by  the plugin.
  // This field is checked in most message handlers to decide whether
  // operations should proceed or fail.
  int32_t encoder_last_error_;

  // Size of the frames allocated for the encoder (matching hardware
  // constraints).
  gfx::Size input_coded_size_;

  // Number of frames the encoder needs.
  uint32_t frame_count_;

  // Format of the frames to give to the encoder.
  media::VideoFrame::Format media_input_format_;

  base::WeakPtrFactory<PepperVideoEncoderHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperVideoEncoderHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_VIDEO_ENCODER_HOST_H_
