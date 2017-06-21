/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_types.h"

#include <algorithm>  // std::max

#include "webrtc/base/checks.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/source/encoded_frame.h"
#include "webrtc/modules/video_coding/main/source/video_coding_impl.h"
#include "webrtc/modules/video_coding/utility/include/quality_scaler.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {
namespace vcm {

VideoSender::VideoSender(Clock* clock,
                         EncodedImageCallback* post_encode_callback,
                         VideoEncoderRateObserver* encoder_rate_observer,
                         VCMQMSettingsCallback* qm_settings_callback)
    : clock_(clock),
      process_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      _sendCritSect(CriticalSectionWrapper::CreateCriticalSection()),
      _encoder(nullptr),
      _encodedFrameCallback(post_encode_callback),
      _nextFrameTypes(1, kVideoFrameDelta),
      _mediaOpt(clock_),
      _sendStatsCallback(nullptr),
      _codecDataBase(encoder_rate_observer),
      frame_dropper_enabled_(true),
      _sendStatsTimer(1000, clock_),
      current_codec_(),
      qm_settings_callback_(qm_settings_callback),
      protection_callback_(nullptr) {
  encoder_params_ = {0, 0, 0, 0, false};
  // Allow VideoSender to be created on one thread but used on another, post
  // construction. This is currently how this class is being used by at least
  // one external project (diffractor).
  _mediaOpt.EnableQM(qm_settings_callback_ != nullptr);
  _mediaOpt.Reset();
  main_thread_.DetachFromThread();
}

VideoSender::~VideoSender() {
  delete _sendCritSect;
}

int32_t VideoSender::Process() {
  int32_t returnValue = VCM_OK;

  if (_sendStatsTimer.TimeUntilProcess() == 0) {
    _sendStatsTimer.Processed();
    CriticalSectionScoped cs(process_crit_sect_.get());
    if (_sendStatsCallback != nullptr) {
      uint32_t bitRate = _mediaOpt.SentBitRate();
      uint32_t frameRate = _mediaOpt.SentFrameRate();
      _sendStatsCallback->SendStatistics(bitRate, frameRate);
    }
  }

  {
    rtc::CritScope cs(&params_lock_);
    // Force an encoder parameters update, so that incoming frame rate is
    // updated even if bandwidth hasn't changed.
    encoder_params_.input_frame_rate = _mediaOpt.InputFrameRate();
    encoder_params_.updated = true;
  }

  return returnValue;
}

int64_t VideoSender::TimeUntilNextProcess() {
  return _sendStatsTimer.TimeUntilProcess();
}

// Register the send codec to be used.
int32_t VideoSender::RegisterSendCodec(const VideoCodec* sendCodec,
                                       uint32_t numberOfCores,
                                       uint32_t maxPayloadSize) {
  DCHECK(main_thread_.CalledOnValidThread());
  CriticalSectionScoped cs(_sendCritSect);
  if (sendCodec == nullptr) {
    return VCM_PARAMETER_ERROR;
  }

  bool ret = _codecDataBase.SetSendCodec(
      sendCodec, numberOfCores, maxPayloadSize, &_encodedFrameCallback);

  // Update encoder regardless of result to make sure that we're not holding on
  // to a deleted instance.
  _encoder = _codecDataBase.GetEncoder();
  // Cache the current codec here so they can be fetched from this thread
  // without requiring the _sendCritSect lock.
  current_codec_ = *sendCodec;

  if (!ret) {
    LOG(LS_ERROR) << "Failed to initialize set encoder with payload name '"
                  << sendCodec->plName << "'.";
    return VCM_CODEC_ERROR;
  }

  int numLayers = (sendCodec->codecType != kVideoCodecVP8)
                      ? 1
                      : sendCodec->codecSpecific.VP8.numberOfTemporalLayers;
  // If we have screensharing and we have layers, we disable frame dropper.
  bool disable_frame_dropper =
      numLayers > 1 && sendCodec->mode == kScreensharing;
  if (disable_frame_dropper) {
    _mediaOpt.EnableFrameDropper(false);
  } else if (frame_dropper_enabled_) {
    _mediaOpt.EnableFrameDropper(true);
  }
  _nextFrameTypes.clear();
  _nextFrameTypes.resize(VCM_MAX(sendCodec->numberOfSimulcastStreams, 1),
                         kVideoFrameDelta);

  _mediaOpt.SetEncodingData(sendCodec->codecType,
                            sendCodec->maxBitrate * 1000,
                            sendCodec->startBitrate * 1000,
                            sendCodec->width,
                            sendCodec->height,
                            sendCodec->maxFramerate,
                            numLayers,
                            maxPayloadSize);
  return VCM_OK;
}

const VideoCodec& VideoSender::GetSendCodec() const {
  DCHECK(main_thread_.CalledOnValidThread());
  return current_codec_;
}

int32_t VideoSender::SendCodecBlocking(VideoCodec* currentSendCodec) const {
  CriticalSectionScoped cs(_sendCritSect);
  if (currentSendCodec == nullptr) {
    return VCM_PARAMETER_ERROR;
  }
  return _codecDataBase.SendCodec(currentSendCodec) ? 0 : -1;
}

VideoCodecType VideoSender::SendCodecBlocking() const {
  CriticalSectionScoped cs(_sendCritSect);
  return _codecDataBase.SendCodec();
}

// Register an external decoder object.
// This can not be used together with external decoder callbacks.
int32_t VideoSender::RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                             uint8_t payloadType,
                                             bool internalSource /*= false*/) {
  DCHECK(main_thread_.CalledOnValidThread());

  CriticalSectionScoped cs(_sendCritSect);

  if (externalEncoder == nullptr) {
    bool wasSendCodec = false;
    const bool ret =
        _codecDataBase.DeregisterExternalEncoder(payloadType, &wasSendCodec);
    if (wasSendCodec) {
      // Make sure the VCM doesn't use the de-registered codec
      _encoder = nullptr;
    }
    return ret ? 0 : -1;
  }
  _codecDataBase.RegisterExternalEncoder(
      externalEncoder, payloadType, internalSource);
  return 0;
}

// Get codec config parameters
int32_t VideoSender::CodecConfigParameters(uint8_t* buffer,
                                           int32_t size) const {
  CriticalSectionScoped cs(_sendCritSect);
  if (_encoder != nullptr) {
    return _encoder->CodecConfigParameters(buffer, size);
  }
  return VCM_UNINITIALIZED;
}

// TODO(andresp): Make const once media_opt is thread-safe and this has a
// pointer to it.
int32_t VideoSender::SentFrameCount(VCMFrameCount* frameCount) {
  *frameCount = _mediaOpt.SentFrameCount();
  return VCM_OK;
}

// Get encode bitrate
int VideoSender::Bitrate(unsigned int* bitrate) const {
  DCHECK(main_thread_.CalledOnValidThread());
  // Since we're running on the thread that's the only thread known to modify
  // the value of _encoder, we don't need to grab the lock here.

  // return the bit rate which the encoder is set to
  if (!_encoder) {
    return VCM_UNINITIALIZED;
  }
  *bitrate = _encoder->BitRate();
  return 0;
}

// Get encode frame rate
int VideoSender::FrameRate(unsigned int* framerate) const {
  DCHECK(main_thread_.CalledOnValidThread());
  // Since we're running on the thread that's the only thread known to modify
  // the value of _encoder, we don't need to grab the lock here.

  // input frame rate, not compensated
  if (!_encoder) {
    return VCM_UNINITIALIZED;
  }
  *framerate = _encoder->FrameRate();
  return 0;
}

int32_t VideoSender::SetChannelParameters(uint32_t target_bitrate,
                                          uint8_t lossRate,
                                          int64_t rtt) {
  uint32_t target_rate =
      _mediaOpt.SetTargetRates(target_bitrate, lossRate, rtt,
                               protection_callback_, qm_settings_callback_);

  uint32_t input_frame_rate = _mediaOpt.InputFrameRate();

  rtc::CritScope cs(&params_lock_);
  encoder_params_ =
      EncoderParameters{target_rate, lossRate, rtt, input_frame_rate, true};

  return VCM_OK;
}

int32_t VideoSender::UpdateEncoderParameters() {
  EncoderParameters params;
  {
    rtc::CritScope cs(&params_lock_);
    params = encoder_params_;
    encoder_params_.updated = false;
  }

  if (!params.updated || params.target_bitrate == 0)
    return VCM_OK;

  CriticalSectionScoped sendCs(_sendCritSect);
  int32_t ret = VCM_UNINITIALIZED;
  static_assert(VCM_UNINITIALIZED < 0, "VCM_UNINITIALIZED must be negative.");

  if (params.input_frame_rate == 0) {
    // No frame rate estimate available, use default.
    params.input_frame_rate = current_codec_.maxFramerate;
  }
  if (_encoder != nullptr) {
    ret = _encoder->SetChannelParameters(params.loss_rate, params.rtt);
    if (ret >= 0) {
      ret = _encoder->SetRates(params.target_bitrate, params.input_frame_rate);
    }
  }
  return ret;
}

int32_t VideoSender::RegisterTransportCallback(
    VCMPacketizationCallback* transport) {
  CriticalSectionScoped cs(_sendCritSect);
  _encodedFrameCallback.SetMediaOpt(&_mediaOpt);
  _encodedFrameCallback.SetTransportCallback(transport);
  return VCM_OK;
}

// Register video output information callback which will be called to deliver
// information about the video stream produced by the encoder, for instance the
// average frame rate and bit rate.
int32_t VideoSender::RegisterSendStatisticsCallback(
    VCMSendStatisticsCallback* sendStats) {
  CriticalSectionScoped cs(process_crit_sect_.get());
  _sendStatsCallback = sendStats;
  return VCM_OK;
}

// Register a video protection callback which will be called to deliver the
// requested FEC rate and NACK status (on/off).
// Note: this callback is assumed to only be registered once and before it is
// used in this class.
int32_t VideoSender::RegisterProtectionCallback(
    VCMProtectionCallback* protection_callback) {
  DCHECK(protection_callback == nullptr || protection_callback_ == nullptr);
  protection_callback_ = protection_callback;
  return VCM_OK;
}

// Enable or disable a video protection method.
void VideoSender::SetVideoProtection(bool enable,
                                     VCMVideoProtection videoProtection) {
  CriticalSectionScoped cs(_sendCritSect);
  switch (videoProtection) {
    case kProtectionNone:
      _mediaOpt.EnableProtectionMethod(enable, media_optimization::kNone);
      break;
    case kProtectionNack:
    case kProtectionNackSender:
      _mediaOpt.EnableProtectionMethod(enable, media_optimization::kNack);
      break;
    case kProtectionNackFEC:
      _mediaOpt.EnableProtectionMethod(enable, media_optimization::kNackFec);
      break;
    case kProtectionFEC:
      _mediaOpt.EnableProtectionMethod(enable, media_optimization::kFec);
      break;
    case kProtectionNackReceiver:
    case kProtectionKeyOnLoss:
    case kProtectionKeyOnKeyLoss:
      // Ignore receiver modes.
      return;
  }
}
// Add one raw video frame to the encoder, blocking.
int32_t VideoSender::AddVideoFrame(const VideoFrame& videoFrame,
                                   const VideoContentMetrics* contentMetrics,
                                   const CodecSpecificInfo* codecSpecificInfo) {
  UpdateEncoderParameters();
  CriticalSectionScoped cs(_sendCritSect);
  if (_encoder == nullptr) {
    return VCM_UNINITIALIZED;
  }
  // TODO(holmer): Add support for dropping frames per stream. Currently we
  // only have one frame dropper for all streams.
  if (_nextFrameTypes[0] == kFrameEmpty) {
    return VCM_OK;
  }
  if (_mediaOpt.DropFrame()) {
    _encoder->OnDroppedFrame();
    return VCM_OK;
  }
  _mediaOpt.UpdateContentData(contentMetrics);
  // TODO(pbos): Make sure setting send codec is synchronized with video
  // processing so frame size always matches.
  if (!_codecDataBase.MatchesCurrentResolution(videoFrame.width(),
                                               videoFrame.height())) {
    LOG(LS_ERROR) << "Incoming frame doesn't match set resolution. Dropping.";
    return VCM_PARAMETER_ERROR;
  }
  VideoFrame converted_frame = videoFrame;
  if (converted_frame.native_handle() && !_encoder->SupportsNativeHandle()) {
    // This module only supports software encoding.
    // TODO(pbos): Offload conversion from the encoder thread.
    converted_frame = converted_frame.ConvertNativeToI420Frame();
    CHECK(!converted_frame.IsZeroSize())
        << "Frame conversion failed, won't be able to encode frame.";
  }
  int32_t ret =
      _encoder->Encode(converted_frame, codecSpecificInfo, _nextFrameTypes);
  if (ret < 0) {
    LOG(LS_ERROR) << "Failed to encode frame. Error code: " << ret;
    return ret;
  }
  for (size_t i = 0; i < _nextFrameTypes.size(); ++i) {
    _nextFrameTypes[i] = kVideoFrameDelta;  // Default frame type.
  }
  return VCM_OK;
}

int32_t VideoSender::IntraFrameRequest(int stream_index) {
  CriticalSectionScoped cs(_sendCritSect);
  if (stream_index < 0 ||
      static_cast<unsigned int>(stream_index) >= _nextFrameTypes.size()) {
    return -1;
  }
  _nextFrameTypes[stream_index] = kVideoFrameKey;
  if (_encoder != nullptr && _encoder->InternalSource()) {
    // Try to request the frame if we have an external encoder with
    // internal source since AddVideoFrame never will be called.
    if (_encoder->RequestFrame(_nextFrameTypes) == WEBRTC_VIDEO_CODEC_OK) {
      _nextFrameTypes[stream_index] = kVideoFrameDelta;
    }
  }
  return VCM_OK;
}

int32_t VideoSender::EnableFrameDropper(bool enable) {
  CriticalSectionScoped cs(_sendCritSect);
  frame_dropper_enabled_ = enable;
  _mediaOpt.EnableFrameDropper(enable);
  return VCM_OK;
}

void VideoSender::SuspendBelowMinBitrate() {
  DCHECK(main_thread_.CalledOnValidThread());
  int threshold_bps;
  if (current_codec_.numberOfSimulcastStreams == 0) {
    threshold_bps = current_codec_.minBitrate * 1000;
  } else {
    threshold_bps = current_codec_.simulcastStream[0].minBitrate * 1000;
  }
  // Set the hysteresis window to be at 10% of the threshold, but at least
  // 10 kbps.
  int window_bps = std::max(threshold_bps / 10, 10000);
  _mediaOpt.SuspendBelowMinBitrate(threshold_bps, window_bps);
}

bool VideoSender::VideoSuspended() const {
  CriticalSectionScoped cs(_sendCritSect);
  return _mediaOpt.IsVideoSuspended();
}
}  // namespace vcm
}  // namespace webrtc
