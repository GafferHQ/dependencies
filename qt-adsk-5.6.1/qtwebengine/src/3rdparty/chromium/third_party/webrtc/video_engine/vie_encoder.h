/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_ENCODER_H_
#define WEBRTC_VIDEO_ENGINE_VIE_ENCODER_H_

#include <map>
#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_allocator.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/typedefs.h"
#include "webrtc/video/video_capture_input.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {

class Config;
class CriticalSectionWrapper;
class EncodedImageCallback;
class PacedSender;
class PayloadRouter;
class ProcessThread;
class QMVideoSettingsCallback;
class SendStatisticsProxy;
class ViEBitrateObserver;
class ViEEffectFilter;
class ViEEncoderObserver;
class VideoCodingModule;

// This class declares an abstract interface for a user defined observer. It is
// up to the VideoEngine user to implement a derived class which implements the
// observer class. The observer is registered using RegisterEncoderObserver()
// and deregistered using DeregisterEncoderObserver().
class ViEEncoderObserver {
 public:
  // This method is called once per second with the current encoded frame rate
  // and bit rate.
  virtual void OutgoingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate) = 0;

  // This method is called whenever the state of the SuspendBelowMinBitrate
  // changes, i.e., when |is_suspended| toggles.
  virtual void SuspendChange(int video_channel, bool is_suspended) = 0;

 protected:
  virtual ~ViEEncoderObserver() {}
};

class ViEEncoder : public RtcpIntraFrameObserver,
                   public VideoEncoderRateObserver,
                   public VCMPacketizationCallback,
                   public VCMSendStatisticsCallback,
                   public VideoCaptureCallback {
 public:
  friend class ViEBitrateObserver;

  ViEEncoder(int32_t channel_id,
             uint32_t number_of_cores,
             const Config& config,
             ProcessThread& module_process_thread,
             PacedSender* pacer,
             BitrateAllocator* bitrate_allocator,
             BitrateController* bitrate_controller,
             bool disable_default_encoder);
  ~ViEEncoder();

  bool Init();

  // This function is assumed to be called before any frames are delivered and
  // only once.
  // Ideally this would be done in Init, but the dependencies between ViEEncoder
  // and ViEChannel makes it really hard to do in a good way.
  void StartThreadsAndSetSharedMembers(
      rtc::scoped_refptr<PayloadRouter> send_payload_router,
      VCMProtectionCallback* vcm_protection_callback);

  // This function must be called before the corresponding ViEChannel is
  // deleted.
  void StopThreadsAndRemoveSharedMembers();

  void SetNetworkTransmissionState(bool is_transmitting);

  // Returns the id of the owning channel.
  int Owner() const;

  // Drops incoming packets before they get to the encoder.
  void Pause();
  void Restart();

  // Codec settings.
  uint8_t NumberOfCodecs();
  int32_t GetCodec(uint8_t list_index, VideoCodec* video_codec);
  int32_t RegisterExternalEncoder(VideoEncoder* encoder,
                                  uint8_t pl_type,
                                  bool internal_source);
  int32_t DeRegisterExternalEncoder(uint8_t pl_type);
  int32_t SetEncoder(const VideoCodec& video_codec);
  int32_t GetEncoder(VideoCodec* video_codec);

  int32_t GetCodecConfigParameters(
    unsigned char config_parameters[kConfigParameterSize],
    unsigned char& config_parameters_size);

  // Scale or crop/pad image.
  int32_t ScaleInputImage(bool enable);

  // Implementing VideoCaptureCallback.
  void DeliverFrame(VideoFrame video_frame) override;

  int32_t SendKeyFrame();
  int32_t SendCodecStatistics(uint32_t* num_key_frames,
                              uint32_t* num_delta_frames);

  uint32_t LastObservedBitrateBps() const;
  int CodecTargetBitrate(uint32_t* bitrate) const;
  // Loss protection.
  int32_t UpdateProtectionMethod(bool nack, bool fec);
  bool nack_enabled() const { return nack_enabled_; }

  // Buffering mode.
  void SetSenderBufferingMode(int target_delay_ms);

  // Implements VideoEncoderRateObserver.
  void OnSetRates(uint32_t bitrate_bps, int framerate) override;

  // Implements VCMPacketizationCallback.
  int32_t SendData(uint8_t payload_type,
                   const EncodedImage& encoded_image,
                   const RTPFragmentationHeader& fragmentation_header,
                   const RTPVideoHeader* rtp_video_hdr) override;

  // Implements VideoSendStatisticsCallback.
  int32_t SendStatistics(const uint32_t bit_rate,
                         const uint32_t frame_rate) override;

  int32_t RegisterCodecObserver(ViEEncoderObserver* observer);

  // Implements RtcpIntraFrameObserver.
  void OnReceivedIntraFrameRequest(uint32_t ssrc) override;
  void OnReceivedSLI(uint32_t ssrc, uint8_t picture_id) override;
  void OnReceivedRPSI(uint32_t ssrc, uint64_t picture_id) override;
  void OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc) override;

  // Sets SSRCs for all streams.
  bool SetSsrcs(const std::vector<uint32_t>& ssrcs);

  void SetMinTransmitBitrate(int min_transmit_bitrate_kbps);

  // Lets the sender suspend video when the rate drops below
  // |threshold_bps|, and turns back on when the rate goes back up above
  // |threshold_bps| + |window_bps|.
  void SuspendBelowMinBitrate();

  // New-style callbacks, used by VideoSendStream.
  void RegisterPreEncodeCallback(I420FrameCallback* pre_encode_callback);
  void RegisterPostEncodeImageCallback(
        EncodedImageCallback* post_encode_callback);

  void RegisterSendStatisticsProxy(SendStatisticsProxy* send_statistics_proxy);

  int channel_id() const { return channel_id_; }

  int GetPaddingNeededBps(int bitrate_bps) const;

 protected:
  // Called by BitrateObserver.
  void OnNetworkChanged(uint32_t bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t round_trip_time_ms);

 private:
  bool EncoderPaused() const EXCLUSIVE_LOCKS_REQUIRED(data_cs_);
  void TraceFrameDropStart() EXCLUSIVE_LOCKS_REQUIRED(data_cs_);
  void TraceFrameDropEnd() EXCLUSIVE_LOCKS_REQUIRED(data_cs_);

  void UpdateHistograms();

  const int channel_id_;
  const uint32_t number_of_cores_;
  const bool disable_default_encoder_;

  const rtc::scoped_ptr<VideoProcessingModule> vpm_;
  const rtc::scoped_ptr<QMVideoSettingsCallback> qm_callback_;
  const rtc::scoped_ptr<VideoCodingModule> vcm_;
  rtc::scoped_refptr<PayloadRouter> send_payload_router_;

  rtc::scoped_ptr<CriticalSectionWrapper> callback_cs_;
  rtc::scoped_ptr<CriticalSectionWrapper> data_cs_;
  rtc::scoped_ptr<BitrateObserver> bitrate_observer_;

  PacedSender* const pacer_;
  BitrateAllocator* const bitrate_allocator_;
  BitrateController* const bitrate_controller_;

  // The time we last received an input frame or encoded frame. This is used to
  // track when video is stopped long enough that we also want to stop sending
  // padding.
  int64_t time_of_last_frame_activity_ms_ GUARDED_BY(data_cs_);
  bool send_padding_ GUARDED_BY(data_cs_);
  int min_transmit_bitrate_kbps_ GUARDED_BY(data_cs_);
  uint32_t last_observed_bitrate_bps_ GUARDED_BY(data_cs_);
  int target_delay_ms_ GUARDED_BY(data_cs_);
  bool network_is_transmitting_ GUARDED_BY(data_cs_);
  bool encoder_paused_ GUARDED_BY(data_cs_);
  bool encoder_paused_and_dropped_frame_ GUARDED_BY(data_cs_);
  std::map<unsigned int, int64_t> time_last_intra_request_ms_
      GUARDED_BY(data_cs_);

  bool fec_enabled_;
  bool nack_enabled_;

  ViEEncoderObserver* codec_observer_ GUARDED_BY(callback_cs_);
  ProcessThread& module_process_thread_;

  bool has_received_sli_ GUARDED_BY(data_cs_);
  uint8_t picture_id_sli_ GUARDED_BY(data_cs_);
  bool has_received_rpsi_ GUARDED_BY(data_cs_);
  uint64_t picture_id_rpsi_ GUARDED_BY(data_cs_);
  std::map<uint32_t, int> ssrc_streams_ GUARDED_BY(data_cs_);

  bool video_suspended_ GUARDED_BY(data_cs_);
  I420FrameCallback* pre_encode_callback_ GUARDED_BY(callback_cs_);
  const int64_t start_ms_;

  SendStatisticsProxy* send_statistics_proxy_ GUARDED_BY(callback_cs_);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_ENCODER_H_
