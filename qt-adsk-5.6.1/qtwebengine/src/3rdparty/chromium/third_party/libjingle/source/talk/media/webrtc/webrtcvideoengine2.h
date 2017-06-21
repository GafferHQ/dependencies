/*
 * libjingle
 * Copyright 2014 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_H_
#define TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_H_

#include <map>
#include <string>
#include <vector>

#include "talk/media/base/mediaengine.h"
#include "talk/media/webrtc/webrtcvideochannelfactory.h"
#include "talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "webrtc/base/cpumonitor.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/call.h"
#include "webrtc/transport.h"
#include "webrtc/video_frame.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_renderer.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
class VideoDecoder;
class VideoEncoder;
}

namespace rtc {
class CpuMonitor;
class Thread;
}  // namespace rtc

namespace cricket {

class VideoCapturer;
class VideoFrame;
class VideoProcessor;
class VideoRenderer;
class VoiceMediaChannel;
class WebRtcDecoderObserver;
class WebRtcEncoderObserver;
class WebRtcLocalStreamInfo;
class WebRtcRenderAdapter;
class WebRtcVideoChannelRecvInfo;
class WebRtcVideoChannelSendInfo;
class WebRtcVoiceEngine;
class WebRtcVoiceMediaChannel;

struct CapturedFrame;
struct Device;

// Exposed here for unittests.
std::vector<VideoCodec> DefaultVideoCodecList();

class UnsignalledSsrcHandler {
 public:
  enum Action {
    kDropPacket,
    kDeliverPacket,
  };
  virtual Action OnUnsignalledSsrc(WebRtcVideoChannel2* channel,
                                   uint32_t ssrc) = 0;
};

// TODO(pbos): Remove, use external handlers only.
class DefaultUnsignalledSsrcHandler : public UnsignalledSsrcHandler {
 public:
  DefaultUnsignalledSsrcHandler();
  Action OnUnsignalledSsrc(WebRtcVideoChannel2* channel,
                           uint32_t ssrc) override;

  VideoRenderer* GetDefaultRenderer() const;
  void SetDefaultRenderer(VideoMediaChannel* channel, VideoRenderer* renderer);

 private:
  uint32_t default_recv_ssrc_;
  VideoRenderer* default_renderer_;
};

// CallFactory, overridden for testing to verify that webrtc::Call is configured
// properly.
class WebRtcCallFactory {
 public:
  virtual ~WebRtcCallFactory();
  virtual webrtc::Call* CreateCall(const webrtc::Call::Config& config);
};

// WebRtcVideoEngine2 is used for the new native WebRTC Video API (webrtc:1667).
class WebRtcVideoEngine2 : public sigslot::has_slots<> {
 public:
  explicit WebRtcVideoEngine2(WebRtcVoiceEngine* voice_engine);
  virtual ~WebRtcVideoEngine2();

  // Used for testing to be able to check and use the webrtc::Call config.
  void SetCallFactory(WebRtcCallFactory* call_factory);

  // Basic video engine implementation.
  void Init();

  int GetCapabilities();
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config);

  WebRtcVideoChannel2* CreateChannel(const VideoOptions& options,
                                     VoiceMediaChannel* voice_channel);

  const std::vector<VideoCodec>& codecs() const;
  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const;
  void SetLogging(int min_sev, const char* filter);

  // Set a WebRtcVideoDecoderFactory for external decoding. Video engine does
  // not take the ownership of |decoder_factory|. The caller needs to make sure
  // that |decoder_factory| outlives the video engine.
  void SetExternalDecoderFactory(WebRtcVideoDecoderFactory* decoder_factory);
  // Set a WebRtcVideoEncoderFactory for external encoding. Video engine does
  // not take the ownership of |encoder_factory|. The caller needs to make sure
  // that |encoder_factory| outlives the video engine.
  virtual void SetExternalEncoderFactory(
      WebRtcVideoEncoderFactory* encoder_factory);

  bool EnableTimedRender();

  bool FindCodec(const VideoCodec& in);
  bool CanSendCodec(const VideoCodec& in,
                    const VideoCodec& current,
                    VideoCodec* out);
  // Check whether the supplied trace should be ignored.
  bool ShouldIgnoreTrace(const std::string& trace);

 private:
  std::vector<VideoCodec> GetSupportedCodecs() const;

  WebRtcVoiceEngine* voice_engine_;
  std::vector<VideoCodec> video_codecs_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;

  bool initialized_;

  WebRtcCallFactory default_call_factory_;
  WebRtcCallFactory* call_factory_;

  WebRtcVideoDecoderFactory* external_decoder_factory_;
  WebRtcVideoEncoderFactory* external_encoder_factory_;
  rtc::scoped_ptr<WebRtcVideoEncoderFactory> simulcast_encoder_factory_;
};

class WebRtcVideoChannel2 : public rtc::MessageHandler,
                            public VideoMediaChannel,
                            public webrtc::newapi::Transport,
                            public webrtc::LoadObserver {
 public:
  WebRtcVideoChannel2(WebRtcCallFactory* call_factory,
                      WebRtcVoiceEngine* voice_engine,
                      WebRtcVoiceMediaChannel* voice_channel,
                      const VideoOptions& options,
                      WebRtcVideoEncoderFactory* external_encoder_factory,
                      WebRtcVideoDecoderFactory* external_decoder_factory);
  ~WebRtcVideoChannel2();
  bool Init();

  // VideoMediaChannel implementation
  void DetachVoiceChannel() override;
  bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) override;
  bool SetSendCodecs(const std::vector<VideoCodec>& codecs) override;
  bool GetSendCodec(VideoCodec* send_codec) override;
  bool SetSendStreamFormat(uint32 ssrc, const VideoFormat& format) override;
  bool SetRender(bool render) override;
  bool SetSend(bool send) override;

  bool AddSendStream(const StreamParams& sp) override;
  bool RemoveSendStream(uint32 ssrc) override;
  bool AddRecvStream(const StreamParams& sp) override;
  bool AddRecvStream(const StreamParams& sp, bool default_stream);
  bool RemoveRecvStream(uint32 ssrc) override;
  bool SetRenderer(uint32 ssrc, VideoRenderer* renderer) override;
  bool GetStats(VideoMediaInfo* info) override;
  bool SetCapturer(uint32 ssrc, VideoCapturer* capturer) override;
  bool SendIntraFrame() override;
  bool RequestIntraFrame() override;

  void OnPacketReceived(rtc::Buffer* packet,
                        const rtc::PacketTime& packet_time) override;
  void OnRtcpReceived(rtc::Buffer* packet,
                      const rtc::PacketTime& packet_time) override;
  void OnReadyToSend(bool ready) override;
  bool MuteStream(uint32 ssrc, bool mute) override;

  // Set send/receive RTP header extensions. This must be done before creating
  // streams as it only has effect on future streams.
  bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) override;
  bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) override;
  bool SetMaxSendBandwidth(int bps) override;
  bool SetOptions(const VideoOptions& options) override;
  bool GetOptions(VideoOptions* options) const override {
    *options = options_;
    return true;
  }
  void SetInterface(NetworkInterface* iface) override;
  void UpdateAspectRatio(int ratio_w, int ratio_h) override;

  void OnMessage(rtc::Message* msg) override;

  void OnLoadUpdate(Load load) override;

  // Implemented for VideoMediaChannelTest.
  bool sending() const { return sending_; }
  uint32 GetDefaultSendChannelSsrc() { return default_send_ssrc_; }
  bool GetRenderer(uint32 ssrc, VideoRenderer** renderer);

 private:
  class WebRtcVideoReceiveStream;
  void ConfigureReceiverRtp(webrtc::VideoReceiveStream::Config* config,
                            const StreamParams& sp) const;
  bool CodecIsExternallySupported(const std::string& name) const;
  bool ValidateSendSsrcAvailability(const StreamParams& sp) const
      EXCLUSIVE_LOCKS_REQUIRED(stream_crit_);
  bool ValidateReceiveSsrcAvailability(const StreamParams& sp) const
      EXCLUSIVE_LOCKS_REQUIRED(stream_crit_);
  void DeleteReceiveStream(WebRtcVideoReceiveStream* stream)
      EXCLUSIVE_LOCKS_REQUIRED(stream_crit_);

  struct VideoCodecSettings {
    VideoCodecSettings();

    bool operator==(const VideoCodecSettings& other) const;
    bool operator!=(const VideoCodecSettings& other) const;

    VideoCodec codec;
    webrtc::FecConfig fec;
    int rtx_payload_type;
  };

  // Wrapper for the sender part, this is where the capturer is connected and
  // frames are then converted from cricket frames to webrtc frames.
  class WebRtcVideoSendStream : public sigslot::has_slots<> {
   public:
    WebRtcVideoSendStream(
        webrtc::Call* call,
        WebRtcVideoEncoderFactory* external_encoder_factory,
        const VideoOptions& options,
        int max_bitrate_bps,
        const Settable<VideoCodecSettings>& codec_settings,
        const StreamParams& sp,
        const std::vector<webrtc::RtpExtension>& rtp_extensions);
    ~WebRtcVideoSendStream();

    void SetOptions(const VideoOptions& options);
    void SetCodec(const VideoCodecSettings& codec);
    void SetRtpExtensions(
        const std::vector<webrtc::RtpExtension>& rtp_extensions);

    void InputFrame(VideoCapturer* capturer, const VideoFrame* frame);
    bool SetCapturer(VideoCapturer* capturer);
    bool SetVideoFormat(const VideoFormat& format);
    void MuteStream(bool mute);
    bool DisconnectCapturer();

    void SetApplyRotation(bool apply_rotation);

    void Start();
    void Stop();

    const std::vector<uint32>& GetSsrcs() const;
    VideoSenderInfo GetVideoSenderInfo();
    void FillBandwidthEstimationInfo(BandwidthEstimationInfo* bwe_info);

    void SetMaxBitrateBps(int max_bitrate_bps);

   private:
    // Parameters needed to reconstruct the underlying stream.
    // webrtc::VideoSendStream doesn't support setting a lot of options on the
    // fly, so when those need to be changed we tear down and reconstruct with
    // similar parameters depending on which options changed etc.
    struct VideoSendStreamParameters {
      VideoSendStreamParameters(
          const webrtc::VideoSendStream::Config& config,
          const VideoOptions& options,
          int max_bitrate_bps,
          const Settable<VideoCodecSettings>& codec_settings);
      webrtc::VideoSendStream::Config config;
      VideoOptions options;
      int max_bitrate_bps;
      Settable<VideoCodecSettings> codec_settings;
      // Sent resolutions + bitrates etc. by the underlying VideoSendStream,
      // typically changes when setting a new resolution or reconfiguring
      // bitrates.
      webrtc::VideoEncoderConfig encoder_config;
    };

    struct AllocatedEncoder {
      AllocatedEncoder(webrtc::VideoEncoder* encoder,
                       webrtc::VideoCodecType type,
                       bool external);
      webrtc::VideoEncoder* encoder;
      webrtc::VideoEncoder* external_encoder;
      webrtc::VideoCodecType type;
      bool external;
    };

    struct Dimensions {
      // Initial encoder configuration (QCIF, 176x144) frame (to ensure that
      // hardware encoders can be initialized). This gives us low memory usage
      // but also makes it so configuration errors are discovered at the time we
      // apply the settings rather than when we get the first frame (waiting for
      // the first frame to know that you gave a bad codec parameter could make
      // debugging hard).
      // TODO(pbos): Consider setting up encoders lazily.
      Dimensions() : width(176), height(144), is_screencast(false) {}
      int width;
      int height;
      bool is_screencast;
    };

    union VideoEncoderSettings {
      webrtc::VideoCodecVP8 vp8;
      webrtc::VideoCodecVP9 vp9;
    };

    static std::vector<webrtc::VideoStream> CreateVideoStreams(
        const VideoCodec& codec,
        const VideoOptions& options,
        int max_bitrate_bps,
        size_t num_streams);
    static std::vector<webrtc::VideoStream> CreateSimulcastVideoStreams(
        const VideoCodec& codec,
        const VideoOptions& options,
        int max_bitrate_bps,
        size_t num_streams);

    void* ConfigureVideoEncoderSettings(const VideoCodec& codec,
                                        const VideoOptions& options,
                                        bool is_screencast)
        EXCLUSIVE_LOCKS_REQUIRED(lock_);

    AllocatedEncoder CreateVideoEncoder(const VideoCodec& codec)
        EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void DestroyVideoEncoder(AllocatedEncoder* encoder)
        EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void SetCodecAndOptions(const VideoCodecSettings& codec,
                            const VideoOptions& options)
        EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void RecreateWebRtcStream() EXCLUSIVE_LOCKS_REQUIRED(lock_);
    webrtc::VideoEncoderConfig CreateVideoEncoderConfig(
        const Dimensions& dimensions,
        const VideoCodec& codec) const EXCLUSIVE_LOCKS_REQUIRED(lock_);
    void SetDimensions(int width, int height, bool is_screencast)
        EXCLUSIVE_LOCKS_REQUIRED(lock_);

    const std::vector<uint32> ssrcs_;
    const std::vector<SsrcGroup> ssrc_groups_;
    webrtc::Call* const call_;
    WebRtcVideoEncoderFactory* const external_encoder_factory_
        GUARDED_BY(lock_);

    rtc::CriticalSection lock_;
    webrtc::VideoSendStream* stream_ GUARDED_BY(lock_);
    VideoSendStreamParameters parameters_ GUARDED_BY(lock_);
    VideoEncoderSettings encoder_settings_ GUARDED_BY(lock_);
    AllocatedEncoder allocated_encoder_ GUARDED_BY(lock_);
    Dimensions last_dimensions_ GUARDED_BY(lock_);

    VideoCapturer* capturer_ GUARDED_BY(lock_);
    bool sending_ GUARDED_BY(lock_);
    bool muted_ GUARDED_BY(lock_);
    VideoFormat format_ GUARDED_BY(lock_);
    int old_adapt_changes_ GUARDED_BY(lock_);
  };

  // Wrapper for the receiver part, contains configs etc. that are needed to
  // reconstruct the underlying VideoReceiveStream. Also serves as a wrapper
  // between webrtc::VideoRenderer and cricket::VideoRenderer.
  class WebRtcVideoReceiveStream : public webrtc::VideoRenderer {
   public:
    WebRtcVideoReceiveStream(
        webrtc::Call* call,
        const StreamParams& sp,
        WebRtcVideoDecoderFactory* external_decoder_factory,
        bool default_stream,
        const webrtc::VideoReceiveStream::Config& config,
        const std::vector<VideoCodecSettings>& recv_codecs);
    ~WebRtcVideoReceiveStream();

    const std::vector<uint32>& GetSsrcs() const;

    void SetLocalSsrc(uint32_t local_ssrc);
    void SetNackAndRemb(bool nack_enabled, bool remb_enabled);
    void SetRecvCodecs(const std::vector<VideoCodecSettings>& recv_codecs);
    void SetRtpExtensions(const std::vector<webrtc::RtpExtension>& extensions);

    void RenderFrame(const webrtc::VideoFrame& frame,
                     int time_to_render_ms) override;
    bool IsTextureSupported() const override;
    bool IsDefaultStream() const;

    void SetRenderer(cricket::VideoRenderer* renderer);
    cricket::VideoRenderer* GetRenderer();

    VideoReceiverInfo GetVideoReceiverInfo();

   private:
    struct AllocatedDecoder {
      AllocatedDecoder(webrtc::VideoDecoder* decoder,
                       webrtc::VideoCodecType type,
                       bool external);
      webrtc::VideoDecoder* decoder;
      // Decoder wrapped into a fallback decoder to permit software fallback.
      webrtc::VideoDecoder* external_decoder;
      webrtc::VideoCodecType type;
      bool external;
    };

    void SetSize(int width, int height);
    void RecreateWebRtcStream();

    AllocatedDecoder CreateOrReuseVideoDecoder(
        std::vector<AllocatedDecoder>* old_decoder,
        const VideoCodec& codec);
    void ClearDecoders(std::vector<AllocatedDecoder>* allocated_decoders);

    webrtc::Call* const call_;
    const std::vector<uint32> ssrcs_;
    const std::vector<SsrcGroup> ssrc_groups_;

    webrtc::VideoReceiveStream* stream_;
    const bool default_stream_;
    webrtc::VideoReceiveStream::Config config_;

    WebRtcVideoDecoderFactory* const external_decoder_factory_;
    std::vector<AllocatedDecoder> allocated_decoders_;

    rtc::CriticalSection renderer_lock_;
    cricket::VideoRenderer* renderer_ GUARDED_BY(renderer_lock_);
    int last_width_ GUARDED_BY(renderer_lock_);
    int last_height_ GUARDED_BY(renderer_lock_);
    // Expands remote RTP timestamps to int64_t to be able to estimate how long
    // the stream has been running.
    rtc::TimestampWrapAroundHandler timestamp_wraparound_handler_
        GUARDED_BY(renderer_lock_);
    int64_t first_frame_timestamp_ GUARDED_BY(renderer_lock_);
    // Start NTP time is estimated as current remote NTP time (estimated from
    // RTCP) minus the elapsed time, as soon as remote NTP time is available.
    int64_t estimated_remote_start_ntp_time_ms_ GUARDED_BY(renderer_lock_);
  };

  void Construct(webrtc::Call* call, WebRtcVideoEngine2* engine);
  void SetDefaultOptions();

  bool SendRtp(const uint8_t* data, size_t len) override;
  bool SendRtcp(const uint8_t* data, size_t len) override;

  void StartAllSendStreams();
  void StopAllSendStreams();

  static std::vector<VideoCodecSettings> MapCodecs(
      const std::vector<VideoCodec>& codecs);
  std::vector<VideoCodecSettings> FilterSupportedCodecs(
      const std::vector<VideoCodecSettings>& mapped_codecs) const;

  void FillSenderStats(VideoMediaInfo* info);
  void FillReceiverStats(VideoMediaInfo* info);
  void FillBandwidthEstimationStats(const webrtc::Call::Stats& stats,
                                    VideoMediaInfo* info);

  rtc::ThreadChecker thread_checker_;

  uint32_t rtcp_receiver_report_ssrc_;
  bool sending_;
  rtc::scoped_ptr<webrtc::Call> call_;
  WebRtcCallFactory* call_factory_;

  uint32_t default_send_ssrc_;

  DefaultUnsignalledSsrcHandler default_unsignalled_ssrc_handler_;
  UnsignalledSsrcHandler* const unsignalled_ssrc_handler_;

  // Separate list of set capturers used to signal CPU adaptation. These should
  // not be locked while calling methods that take other locks to prevent
  // lock-order inversions.
  rtc::CriticalSection capturer_crit_;
  bool signal_cpu_adaptation_ GUARDED_BY(capturer_crit_);
  std::map<uint32, VideoCapturer*> capturers_ GUARDED_BY(capturer_crit_);

  rtc::CriticalSection stream_crit_;
  // Using primary-ssrc (first ssrc) as key.
  std::map<uint32, WebRtcVideoSendStream*> send_streams_
      GUARDED_BY(stream_crit_);
  std::map<uint32, WebRtcVideoReceiveStream*> receive_streams_
      GUARDED_BY(stream_crit_);
  std::set<uint32> send_ssrcs_ GUARDED_BY(stream_crit_);
  std::set<uint32> receive_ssrcs_ GUARDED_BY(stream_crit_);

  Settable<VideoCodecSettings> send_codec_;
  std::vector<webrtc::RtpExtension> send_rtp_extensions_;

  WebRtcVoiceMediaChannel* voice_channel_;
  const int voice_channel_id_;
  WebRtcVideoEncoderFactory* const external_encoder_factory_;
  WebRtcVideoDecoderFactory* const external_decoder_factory_;
  std::vector<VideoCodecSettings> recv_codecs_;
  std::vector<webrtc::RtpExtension> recv_rtp_extensions_;
  webrtc::Call::Config::BitrateConfig bitrate_config_;
  VideoOptions options_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTC_WEBRTCVIDEOENGINE2_H_
