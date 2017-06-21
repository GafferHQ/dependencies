/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/opensles_player.h"

#include <android/log.h>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_device/android/audio_manager.h"
#include "webrtc/modules/audio_device/android/fine_audio_buffer.h"

#define TAG "OpenSLESPlayer"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

#define RETURN_ON_ERROR(op, ...)        \
  do {                                  \
    SLresult err = (op);                \
    if (err != SL_RESULT_SUCCESS) {     \
      ALOGE("%s failed: %d", #op, err); \
      return __VA_ARGS__;               \
    }                                   \
  } while (0)

namespace webrtc {

OpenSLESPlayer::OpenSLESPlayer(AudioManager* audio_manager)
    : audio_parameters_(audio_manager->GetPlayoutAudioParameters()),
      audio_device_buffer_(NULL),
      initialized_(false),
      playing_(false),
      bytes_per_buffer_(0),
      buffer_index_(0),
      engine_(nullptr),
      player_(nullptr),
      simple_buffer_queue_(nullptr),
      volume_(nullptr) {
  ALOGD("ctor%s", GetThreadInfo().c_str());
  // Use native audio output parameters provided by the audio manager and
  // define the PCM format structure.
  pcm_format_ = CreatePCMConfiguration(audio_parameters_.channels(),
                                       audio_parameters_.sample_rate(),
                                       audio_parameters_.bits_per_sample());
  // Detach from this thread since we want to use the checker to verify calls
  // from the internal  audio thread.
  thread_checker_opensles_.DetachFromThread();
}

OpenSLESPlayer::~OpenSLESPlayer() {
  ALOGD("dtor%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
  DestroyAudioPlayer();
  DestroyMix();
  DestroyEngine();
  DCHECK(!engine_object_.Get());
  DCHECK(!engine_);
  DCHECK(!output_mix_.Get());
  DCHECK(!player_);
  DCHECK(!simple_buffer_queue_);
  DCHECK(!volume_);
}

int OpenSLESPlayer::Init() {
  ALOGD("Init%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  return 0;
}

int OpenSLESPlayer::Terminate() {
  ALOGD("Terminate%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  StopPlayout();
  return 0;
}

int OpenSLESPlayer::InitPlayout() {
  ALOGD("InitPlayout%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);
  DCHECK(!playing_);
  CreateEngine();
  CreateMix();
  initialized_ = true;
  buffer_index_ = 0;
  return 0;
}

int OpenSLESPlayer::StartPlayout() {
  ALOGD("StartPlayout%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  DCHECK(!playing_);
  // The number of lower latency audio players is limited, hence we create the
  // audio player in Start() and destroy it in Stop().
  CreateAudioPlayer();
  // Fill up audio buffers to avoid initial glitch and to ensure that playback
  // starts when mode is later changed to SL_PLAYSTATE_PLAYING.
  // TODO(henrika): we can save some delay by only making one call to
  // EnqueuePlayoutData. Most likely not worth the risk of adding a glitch.
  for (int i = 0; i < kNumOfOpenSLESBuffers; ++i) {
    EnqueuePlayoutData();
  }
  // Start streaming data by setting the play state to SL_PLAYSTATE_PLAYING.
  // For a player object, when the object is in the SL_PLAYSTATE_PLAYING
  // state, adding buffers will implicitly start playback.
  RETURN_ON_ERROR((*player_)->SetPlayState(player_, SL_PLAYSTATE_PLAYING), -1);
  playing_ = (GetPlayState() == SL_PLAYSTATE_PLAYING);
  DCHECK(playing_);
  return 0;
}

int OpenSLESPlayer::StopPlayout() {
  ALOGD("StopPlayout%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_ || !playing_) {
    return 0;
  }
  // Stop playing by setting the play state to SL_PLAYSTATE_STOPPED.
  RETURN_ON_ERROR((*player_)->SetPlayState(player_, SL_PLAYSTATE_STOPPED), -1);
  // Clear the buffer queue to flush out any remaining data.
  RETURN_ON_ERROR((*simple_buffer_queue_)->Clear(simple_buffer_queue_), -1);
#ifndef NDEBUG
  // Verify that the buffer queue is in fact cleared as it should.
  SLAndroidSimpleBufferQueueState buffer_queue_state;
  (*simple_buffer_queue_)->GetState(simple_buffer_queue_, &buffer_queue_state);
  DCHECK_EQ(0u, buffer_queue_state.count);
  DCHECK_EQ(0u, buffer_queue_state.index);
#endif
  // The number of lower latency audio players is limited, hence we create the
  // audio player in Start() and destroy it in Stop().
  DestroyAudioPlayer();
  thread_checker_opensles_.DetachFromThread();
  initialized_ = false;
  playing_ = false;
  return 0;
}

int OpenSLESPlayer::SpeakerVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int OpenSLESPlayer::MaxSpeakerVolume(uint32_t& maxVolume) const {
  return -1;
}

int OpenSLESPlayer::MinSpeakerVolume(uint32_t& minVolume) const {
  return -1;
}

int OpenSLESPlayer::SetSpeakerVolume(uint32_t volume) {
  return -1;
}

int OpenSLESPlayer::SpeakerVolume(uint32_t& volume) const {
  return -1;
}

void OpenSLESPlayer::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  ALOGD("AttachAudioBuffer");
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
  const int sample_rate_hz = audio_parameters_.sample_rate();
  ALOGD("SetPlayoutSampleRate(%d)", sample_rate_hz);
  audio_device_buffer_->SetPlayoutSampleRate(sample_rate_hz);
  const int channels = audio_parameters_.channels();
  ALOGD("SetPlayoutChannels(%d)", channels);
  audio_device_buffer_->SetPlayoutChannels(channels);
  CHECK(audio_device_buffer_);
  AllocateDataBuffers();
}

SLDataFormat_PCM OpenSLESPlayer::CreatePCMConfiguration(int channels,
                                                        int sample_rate,
                                                        int bits_per_sample) {
  ALOGD("CreatePCMConfiguration");
  CHECK_EQ(bits_per_sample, SL_PCMSAMPLEFORMAT_FIXED_16);
  SLDataFormat_PCM format;
  format.formatType = SL_DATAFORMAT_PCM;
  format.numChannels = static_cast<SLuint32>(channels);
  // Note that, the unit of sample rate is actually in milliHertz and not Hertz.
  switch (sample_rate) {
    case 8000:
      format.samplesPerSec = SL_SAMPLINGRATE_8;
      break;
    case 16000:
      format.samplesPerSec = SL_SAMPLINGRATE_16;
      break;
    case 22050:
      format.samplesPerSec = SL_SAMPLINGRATE_22_05;
      break;
    case 32000:
      format.samplesPerSec = SL_SAMPLINGRATE_32;
      break;
    case 44100:
      format.samplesPerSec = SL_SAMPLINGRATE_44_1;
      break;
    case 48000:
      format.samplesPerSec = SL_SAMPLINGRATE_48;
      break;
    default:
      CHECK(false) << "Unsupported sample rate: " << sample_rate;
  }
  format.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  format.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
  format.endianness = SL_BYTEORDER_LITTLEENDIAN;
  if (format.numChannels == 1)
    format.channelMask = SL_SPEAKER_FRONT_CENTER;
  else if (format.numChannels == 2)
    format.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
  else
    CHECK(false) << "Unsupported number of channels: " << format.numChannels;
  return format;
}

void OpenSLESPlayer::AllocateDataBuffers() {
  ALOGD("AllocateDataBuffers");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!simple_buffer_queue_);
  CHECK(audio_device_buffer_);
  bytes_per_buffer_ = audio_parameters_.GetBytesPerBuffer();
  ALOGD("native buffer size: %d", bytes_per_buffer_);
  // Create a modified audio buffer class which allows us to ask for any number
  // of samples (and not only multiple of 10ms) to match the native OpenSL ES
  // buffer size.
  fine_buffer_.reset(new FineAudioBuffer(audio_device_buffer_,
                                         bytes_per_buffer_,
                                         audio_parameters_.sample_rate()));
  // Each buffer must be of this size to avoid unnecessary memcpy while caching
  // data between successive callbacks.
  const int required_buffer_size = fine_buffer_->RequiredBufferSizeBytes();
  ALOGD("required buffer size: %d", required_buffer_size);
  for (int i = 0; i < kNumOfOpenSLESBuffers; ++i) {
    audio_buffers_[i].reset(new SLint8[required_buffer_size]);
  }
}

bool OpenSLESPlayer::CreateEngine() {
  ALOGD("CreateEngine");
  DCHECK(thread_checker_.CalledOnValidThread());
  if (engine_object_.Get())
    return true;
  DCHECK(!engine_);
  const SLEngineOption option[] = {
    {SL_ENGINEOPTION_THREADSAFE, static_cast<SLuint32>(SL_BOOLEAN_TRUE)}};
  RETURN_ON_ERROR(
      slCreateEngine(engine_object_.Receive(), 1, option, 0, NULL, NULL),
      false);
  RETURN_ON_ERROR(
      engine_object_->Realize(engine_object_.Get(), SL_BOOLEAN_FALSE), false);
  RETURN_ON_ERROR(engine_object_->GetInterface(engine_object_.Get(),
                                               SL_IID_ENGINE, &engine_),
                  false);
  return true;
}

void OpenSLESPlayer::DestroyEngine() {
  ALOGD("DestroyEngine");
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!engine_object_.Get())
    return;
  engine_ = nullptr;
  engine_object_.Reset();
}

bool OpenSLESPlayer::CreateMix() {
  ALOGD("CreateMix");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(engine_);
  if (output_mix_.Get())
    return true;

  // Create the ouput mix on the engine object. No interfaces will be used.
  RETURN_ON_ERROR((*engine_)->CreateOutputMix(engine_, output_mix_.Receive(), 0,
                                              NULL, NULL),
                  false);
  RETURN_ON_ERROR(output_mix_->Realize(output_mix_.Get(), SL_BOOLEAN_FALSE),
                  false);
  return true;
}

void OpenSLESPlayer::DestroyMix() {
  ALOGD("DestroyMix");
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!output_mix_.Get())
    return;
  output_mix_.Reset();
}

bool OpenSLESPlayer::CreateAudioPlayer() {
  ALOGD("CreateAudioPlayer");
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(engine_object_.Get());
  DCHECK(output_mix_.Get());
  if (player_object_.Get())
    return true;
  DCHECK(!player_);
  DCHECK(!simple_buffer_queue_);
  DCHECK(!volume_);

  // source: Android Simple Buffer Queue Data Locator is source.
  SLDataLocator_AndroidSimpleBufferQueue simple_buffer_queue = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
      static_cast<SLuint32>(kNumOfOpenSLESBuffers)};
  SLDataSource audio_source = {&simple_buffer_queue, &pcm_format_};

  // sink: OutputMix-based data is sink.
  SLDataLocator_OutputMix locator_output_mix = {SL_DATALOCATOR_OUTPUTMIX,
                                                output_mix_.Get()};
  SLDataSink audio_sink = {&locator_output_mix, NULL};

  // Define interfaces that we indend to use and realize.
  const SLInterfaceID interface_ids[] = {
      SL_IID_ANDROIDCONFIGURATION, SL_IID_BUFFERQUEUE, SL_IID_VOLUME};
  const SLboolean interface_required[] = {
      SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

  // Create the audio player on the engine interface.
  RETURN_ON_ERROR(
      (*engine_)->CreateAudioPlayer(
          engine_, player_object_.Receive(), &audio_source, &audio_sink,
          arraysize(interface_ids), interface_ids, interface_required),
      false);

  // Use the Android configuration interface to set platform-specific
  // parameters. Should be done before player is realized.
  SLAndroidConfigurationItf player_config;
  RETURN_ON_ERROR(
      player_object_->GetInterface(player_object_.Get(),
                                   SL_IID_ANDROIDCONFIGURATION, &player_config),
      false);
  // Set audio player configuration to SL_ANDROID_STREAM_VOICE which
  // corresponds to android.media.AudioManager.STREAM_VOICE_CALL.
  SLint32 stream_type = SL_ANDROID_STREAM_VOICE;
  RETURN_ON_ERROR(
      (*player_config)
          ->SetConfiguration(player_config, SL_ANDROID_KEY_STREAM_TYPE,
                             &stream_type, sizeof(SLint32)),
      false);

  // Realize the audio player object after configuration has been set.
  RETURN_ON_ERROR(
      player_object_->Realize(player_object_.Get(), SL_BOOLEAN_FALSE), false);

  // Get the SLPlayItf interface on the audio player.
  RETURN_ON_ERROR(
      player_object_->GetInterface(player_object_.Get(), SL_IID_PLAY, &player_),
      false);

  // Get the SLAndroidSimpleBufferQueueItf interface on the audio player.
  RETURN_ON_ERROR(
      player_object_->GetInterface(player_object_.Get(), SL_IID_BUFFERQUEUE,
                                   &simple_buffer_queue_),
      false);

  // Register callback method for the Android Simple Buffer Queue interface.
  // This method will be called when the native audio layer needs audio data.
  RETURN_ON_ERROR((*simple_buffer_queue_)
                      ->RegisterCallback(simple_buffer_queue_,
                                         SimpleBufferQueueCallback, this),
                  false);

  // Get the SLVolumeItf interface on the audio player.
  RETURN_ON_ERROR(player_object_->GetInterface(player_object_.Get(),
                                               SL_IID_VOLUME, &volume_),
                  false);

  // TODO(henrika): might not be required to set volume to max here since it
  // seems to be default on most devices. Might be required for unit tests.
  // RETURN_ON_ERROR((*volume_)->SetVolumeLevel(volume_, 0), false);

  return true;
}

void OpenSLESPlayer::DestroyAudioPlayer() {
  ALOGD("DestroyAudioPlayer");
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!player_object_.Get())
    return;
  player_object_.Reset();
  player_ = nullptr;
  simple_buffer_queue_ = nullptr;
  volume_ = nullptr;
}

// static
void OpenSLESPlayer::SimpleBufferQueueCallback(
    SLAndroidSimpleBufferQueueItf caller,
    void* context) {
  OpenSLESPlayer* stream = reinterpret_cast<OpenSLESPlayer*>(context);
  stream->FillBufferQueue();
}

void OpenSLESPlayer::FillBufferQueue() {
  DCHECK(thread_checker_opensles_.CalledOnValidThread());
  SLuint32 state = GetPlayState();
  if (state != SL_PLAYSTATE_PLAYING) {
    ALOGW("Buffer callback in non-playing state!");
    return;
  }
  EnqueuePlayoutData();
}

void OpenSLESPlayer::EnqueuePlayoutData() {
  // Read audio data from the WebRTC source using the FineAudioBuffer object
  // to adjust for differences in buffer size between WebRTC (10ms) and native
  // OpenSL ES.
  SLint8* audio_ptr = audio_buffers_[buffer_index_].get();
  fine_buffer_->GetBufferData(audio_ptr);
  // Enqueue the decoded audio buffer for playback.
  SLresult err =
      (*simple_buffer_queue_)
          ->Enqueue(simple_buffer_queue_, audio_ptr, bytes_per_buffer_);
  if (SL_RESULT_SUCCESS != err) {
    ALOGE("Enqueue failed: %d", err);
  }
  buffer_index_ = (buffer_index_ + 1) % kNumOfOpenSLESBuffers;
}

SLuint32 OpenSLESPlayer::GetPlayState() const {
  DCHECK(player_);
  SLuint32 state;
  SLresult err = (*player_)->GetPlayState(player_, &state);
  if (SL_RESULT_SUCCESS != err) {
    ALOGE("GetPlayState failed: %d", err);
  }
  return state;
}

}  // namespace webrtc
