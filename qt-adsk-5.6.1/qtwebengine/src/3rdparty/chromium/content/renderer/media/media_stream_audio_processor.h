// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_H_

#include "base/atomicops.h"
#include "base/files/file.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/renderer/media/aec_dump_message_filter.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/base/audio_converter.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace blink {
class WebMediaConstraints;
}

namespace media {
class AudioBus;
class AudioFifo;
class AudioParameters;
}  // namespace media

namespace webrtc {
class TypingDetection;
}

namespace content {

class EchoInformation;
class MediaStreamAudioBus;
class MediaStreamAudioFifo;
class RTCMediaConstraints;

using webrtc::AudioProcessorInterface;

// This class owns an object of webrtc::AudioProcessing which contains signal
// processing components like AGC, AEC and NS. It enables the components based
// on the getUserMedia constraints, processes the data and outputs it in a unit
// of 10 ms data chunk.
class CONTENT_EXPORT MediaStreamAudioProcessor :
    NON_EXPORTED_BASE(public WebRtcPlayoutDataSource::Sink),
    NON_EXPORTED_BASE(public AudioProcessorInterface),
    NON_EXPORTED_BASE(public AecDumpMessageFilter::AecDumpDelegate) {
 public:
  // |playout_data_source| is used to register this class as a sink to the
  // WebRtc playout data for processing AEC. If clients do not enable AEC,
  // |playout_data_source| won't be used.
  MediaStreamAudioProcessor(const blink::WebMediaConstraints& constraints,
                            int effects,
                            WebRtcPlayoutDataSource* playout_data_source);

  // Called when the format of the capture data has changed.
  // Called on the main render thread. The caller is responsible for stopping
  // the capture thread before calling this method.
  // After this method, the capture thread will be changed to a new capture
  // thread.
  void OnCaptureFormatChanged(const media::AudioParameters& source_params);

  // Pushes capture data in |audio_source| to the internal FIFO. Each call to
  // this method should be followed by calls to ProcessAndConsumeData() while
  // it returns false, to pull out all available data.
  // Called on the capture audio thread.
  void PushCaptureData(const media::AudioBus& audio_source,
                       base::TimeDelta capture_delay);

  // Processes a block of 10 ms data from the internal FIFO, returning true if
  // |processed_data| contains the result. Returns false and does not modify the
  // outputs if the internal FIFO has insufficient data. The caller does NOT own
  // the object pointed to by |*processed_data|.
  // |capture_delay| is an adjustment on the |capture_delay| value provided in
  // the last call to PushCaptureData().
  // |new_volume| receives the new microphone volume from the AGC.
  // The new microphone volume range is [0, 255], and the value will be 0 if
  // the microphone volume should not be adjusted.
  // Called on the capture audio thread.
  bool ProcessAndConsumeData(
      int volume,
      bool key_pressed,
      media::AudioBus** processed_data,
      base::TimeDelta* capture_delay,
      int* new_volume);

  // Stops the audio processor, no more AEC dump or render data after calling
  // this method.
  void Stop();

  // The audio formats of the capture input to and output from the processor.
  // Must only be called on the main render or audio capture threads.
  const media::AudioParameters& InputFormat() const;
  const media::AudioParameters& OutputFormat() const;

  // Accessor to check if the audio processing is enabled or not.
  bool has_audio_processing() const { return audio_processing_ != NULL; }

  // AecDumpMessageFilter::AecDumpDelegate implementation.
  // Called on the main render thread.
  void OnAecDumpFile(const IPC::PlatformFileForTransit& file_handle) override;
  void OnDisableAecDump() override;
  void OnIpcClosing() override;

 protected:
  ~MediaStreamAudioProcessor() override;

 private:
  friend class MediaStreamAudioProcessorTest;
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioProcessorTest,
                           GetAecDumpMessageFilter);

  // WebRtcPlayoutDataSource::Sink implementation.
  void OnPlayoutData(media::AudioBus* audio_bus,
                     int sample_rate,
                     int audio_delay_milliseconds) override;
  void OnPlayoutDataSourceChanged() override;

  // webrtc::AudioProcessorInterface implementation.
  // This method is called on the libjingle thread.
  void GetStats(AudioProcessorStats* stats) override;

  // Helper to initialize the WebRtc AudioProcessing.
  void InitializeAudioProcessingModule(
      const blink::WebMediaConstraints& constraints, int effects);
  void ConfigureBeamforming(webrtc::Config* config,
                            const std::string& geometry_str) const;

  // Parses the array geometry from the URL string formatted as
  // "x1 y1 z1 ... xn yn zn" for an n-microphone array.
  // Returns a zero-sized vector if |geometry_str| isn't a parseable geometry.
  std::vector<webrtc::Point> ParseArrayGeometry(
      const std::string& geometry_str) const;

  // Helper to initialize the capture converter.
  void InitializeCaptureFifo(const media::AudioParameters& input_format);

  // Helper to initialize the render converter.
  void InitializeRenderFifoIfNeeded(int sample_rate,
                                    int number_of_channels,
                                    int frames_per_buffer);

  // Called by ProcessAndConsumeData().
  // Returns the new microphone volume in the range of |0, 255].
  // When the volume does not need to be updated, it returns 0.
  int ProcessData(const float* const* process_ptrs,
                  int process_frames,
                  base::TimeDelta capture_delay,
                  int volume,
                  bool key_pressed,
                  float* const* output_ptrs);

  // Cached value for the render delay latency. This member is accessed by
  // both the capture audio thread and the render audio thread.
  base::subtle::Atomic32 render_delay_ms_;

  // Module to handle processing and format conversion.
  scoped_ptr<webrtc::AudioProcessing> audio_processing_;

  // FIFO to provide 10 ms capture chunks.
  scoped_ptr<MediaStreamAudioFifo> capture_fifo_;
  // Receives processing output.
  scoped_ptr<MediaStreamAudioBus> output_bus_;

  // FIFO to provide 10 ms render chunks when the AEC is enabled.
  scoped_ptr<MediaStreamAudioFifo> render_fifo_;

  // These are mutated on the main render thread in OnCaptureFormatChanged().
  // The caller guarantees this does not run concurrently with accesses on the
  // capture audio thread.
  media::AudioParameters input_format_;
  media::AudioParameters output_format_;
  // Only used on the render audio thread.
  media::AudioParameters render_format_;

  // Raw pointer to the WebRtcPlayoutDataSource, which is valid for the
  // lifetime of RenderThread.
  WebRtcPlayoutDataSource* playout_data_source_;

  // Used to DCHECK that some methods are called on the main render thread.
  base::ThreadChecker main_thread_checker_;
  // Used to DCHECK that some methods are called on the capture audio thread.
  base::ThreadChecker capture_thread_checker_;
  // Used to DCHECK that some methods are called on the render audio thread.
  base::ThreadChecker render_thread_checker_;

  // Flag to enable stereo channel mirroring.
  bool audio_mirroring_;

  scoped_ptr<webrtc::TypingDetection> typing_detector_;
  // This flag is used to show the result of typing detection.
  // It can be accessed by the capture audio thread and by the libjingle thread
  // which calls GetStats().
  base::subtle::Atomic32 typing_detected_;

  // Communication with browser for AEC dump.
  scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter_;

  // Flag to avoid executing Stop() more than once.
  bool stopped_;

  // Object for logging echo information when the AEC is enabled. Accessible by
  // the libjingle thread through GetStats().
  scoped_ptr<EchoInformation> echo_information_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioProcessor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_H_
