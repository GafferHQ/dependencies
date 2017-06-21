/*
 * libjingle
 * Copyright 2012 Google Inc.
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

// This file contains interfaces for MediaStream, MediaTrack and MediaSource.
// These interfaces are used for implementing MediaStream and MediaTrack as
// defined in http://dev.w3.org/2011/webrtc/editor/webrtc.html#stream-api. These
// interfaces must be used only with PeerConnection. PeerConnectionManager
// interface provides the factory methods to create MediaStream and MediaTracks.

#ifndef TALK_APP_WEBRTC_MEDIASTREAMINTERFACE_H_
#define TALK_APP_WEBRTC_MEDIASTREAMINTERFACE_H_

#include <string>
#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"

namespace cricket {

class AudioRenderer;
class VideoCapturer;
class VideoRenderer;
class VideoFrame;

}  // namespace cricket

namespace webrtc {

// Generic observer interface.
class ObserverInterface {
 public:
  virtual void OnChanged() = 0;

 protected:
  virtual ~ObserverInterface() {}
};

class NotifierInterface {
 public:
  virtual void RegisterObserver(ObserverInterface* observer) = 0;
  virtual void UnregisterObserver(ObserverInterface* observer) = 0;

  virtual ~NotifierInterface() {}
};

// Base class for sources. A MediaStreamTrack have an underlying source that
// provide media. A source can be shared with multiple tracks.
// TODO(perkj): Implement sources for local and remote audio tracks and
// remote video tracks.
class MediaSourceInterface : public rtc::RefCountInterface,
                             public NotifierInterface {
 public:
  enum SourceState {
    kInitializing,
    kLive,
    kEnded,
    kMuted
  };

  virtual SourceState state() const = 0;

 protected:
  virtual ~MediaSourceInterface() {}
};

// Information about a track.
class MediaStreamTrackInterface : public rtc::RefCountInterface,
                                  public NotifierInterface {
 public:
  enum TrackState {
    kInitializing,  // Track is beeing negotiated.
    kLive = 1,  // Track alive
    kEnded = 2,  // Track have ended
    kFailed = 3,  // Track negotiation failed.
  };

  virtual std::string kind() const = 0;
  virtual std::string id() const = 0;
  virtual bool enabled() const = 0;
  virtual TrackState state() const = 0;
  virtual bool set_enabled(bool enable) = 0;
  // These methods should be called by implementation only.
  virtual bool set_state(TrackState new_state) = 0;

 protected:
  virtual ~MediaStreamTrackInterface() {}
};

// Interface for rendering VideoFrames from a VideoTrack
class VideoRendererInterface {
 public:
  // TODO(guoweis): Remove this function.  Obsolete. The implementation of
  // VideoRendererInterface should be able to handle different frame size as
  // well as pending rotation. If it can't apply the frame rotation by itself,
  // it should call |frame|.GetCopyWithRotationApplied() to get a frame that has
  // the rotation applied.
  virtual void SetSize(int width, int height) {}

  // |frame| may have pending rotation. For clients which can't apply rotation,
  // |frame|->GetCopyWithRotationApplied() will return a frame that has the
  // rotation applied.
  virtual void RenderFrame(const cricket::VideoFrame* frame) = 0;

  // TODO(guoweis): Remove this function. This is added as a temporary solution
  // until chrome renderers can apply rotation.
  // Whether the VideoRenderer has the ability to rotate the frame before being
  // displayed. The rotation of a frame is carried by
  // VideoFrame.GetVideoRotation() and is the clockwise angle the frames must be
  // rotated in order to display the frames correctly. If returning false, the
  // frame's rotation must be applied before being delivered by RenderFrame.
  virtual bool CanApplyRotation() { return false; }

 protected:
  // The destructor is protected to prevent deletion via the interface.
  // This is so that we allow reference counted classes, where the destructor
  // should never be public, to implement the interface.
  virtual ~VideoRendererInterface() {}
};

class VideoSourceInterface;

class VideoTrackInterface : public MediaStreamTrackInterface {
 public:
  // Register a renderer that will render all frames received on this track.
  virtual void AddRenderer(VideoRendererInterface* renderer) = 0;
  // Deregister a renderer.
  virtual void RemoveRenderer(VideoRendererInterface* renderer) = 0;

  virtual VideoSourceInterface* GetSource() const = 0;

 protected:
  virtual ~VideoTrackInterface() {}
};

// AudioSourceInterface is a reference counted source used for AudioTracks.
// The same source can be used in multiple AudioTracks.
class AudioSourceInterface : public MediaSourceInterface {
 public:
  class AudioObserver {
   public:
    virtual void OnSetVolume(double volume) = 0;

   protected:
    virtual ~AudioObserver() {}
  };

  // TODO(xians): Makes all the interface pure virtual after Chrome has their
  // implementations.
  // Sets the volume to the source. |volume| is in  the range of [0, 10].
  virtual void SetVolume(double volume) {}

  // Registers/unregisters observer to the audio source.
  virtual void RegisterAudioObserver(AudioObserver* observer) {}
  virtual void UnregisterAudioObserver(AudioObserver* observer) {}
};

// Interface for receiving audio data from a AudioTrack.
class AudioTrackSinkInterface {
 public:
  virtual void OnData(const void* audio_data,
                      int bits_per_sample,
                      int sample_rate,
                      int number_of_channels,
                      int number_of_frames) = 0;
 protected:
  virtual ~AudioTrackSinkInterface() {}
};

// Interface of the audio processor used by the audio track to collect
// statistics.
class AudioProcessorInterface : public rtc::RefCountInterface {
 public:
  struct AudioProcessorStats {
    AudioProcessorStats() : typing_noise_detected(false),
                            echo_return_loss(0),
                            echo_return_loss_enhancement(0),
                            echo_delay_median_ms(0),
                            aec_quality_min(0.0),
                            echo_delay_std_ms(0) {}
    ~AudioProcessorStats() {}

    bool typing_noise_detected;
    int echo_return_loss;
    int echo_return_loss_enhancement;
    int echo_delay_median_ms;
    float aec_quality_min;
    int echo_delay_std_ms;
  };

  // Get audio processor statistics.
  virtual void GetStats(AudioProcessorStats* stats) = 0;

 protected:
  virtual ~AudioProcessorInterface() {}
};

class AudioTrackInterface : public MediaStreamTrackInterface {
 public:
  // TODO(xians): Figure out if the following interface should be const or not.
  virtual AudioSourceInterface* GetSource() const =  0;

  // Add/Remove a sink that will receive the audio data from the track.
  virtual void AddSink(AudioTrackSinkInterface* sink) = 0;
  virtual void RemoveSink(AudioTrackSinkInterface* sink) = 0;

  // Get the signal level from the audio track.
  // Return true on success, otherwise false.
  // TODO(xians): Change the interface to int GetSignalLevel() and pure virtual
  // after Chrome has the correct implementation of the interface.
  virtual bool GetSignalLevel(int* level) { return false; }

  // Get the audio processor used by the audio track. Return NULL if the track
  // does not have any processor.
  // TODO(xians): Make the interface pure virtual.
  virtual rtc::scoped_refptr<AudioProcessorInterface>
      GetAudioProcessor() { return NULL; }

  // Get a pointer to the audio renderer of this AudioTrack.
  // The pointer is valid for the lifetime of this AudioTrack.
  // TODO(xians): Remove the following interface after Chrome switches to
  // AddSink() and RemoveSink() interfaces.
  virtual cricket::AudioRenderer* GetRenderer() { return NULL; }

 protected:
  virtual ~AudioTrackInterface() {}
};

typedef std::vector<rtc::scoped_refptr<AudioTrackInterface> >
    AudioTrackVector;
typedef std::vector<rtc::scoped_refptr<VideoTrackInterface> >
    VideoTrackVector;

class MediaStreamInterface : public rtc::RefCountInterface,
                             public NotifierInterface {
 public:
  virtual std::string label() const = 0;

  virtual AudioTrackVector GetAudioTracks() = 0;
  virtual VideoTrackVector GetVideoTracks() = 0;
  virtual rtc::scoped_refptr<AudioTrackInterface>
      FindAudioTrack(const std::string& track_id) = 0;
  virtual rtc::scoped_refptr<VideoTrackInterface>
      FindVideoTrack(const std::string& track_id) = 0;

  virtual bool AddTrack(AudioTrackInterface* track) = 0;
  virtual bool AddTrack(VideoTrackInterface* track) = 0;
  virtual bool RemoveTrack(AudioTrackInterface* track) = 0;
  virtual bool RemoveTrack(VideoTrackInterface* track) = 0;

 protected:
  virtual ~MediaStreamInterface() {}
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMINTERFACE_H_
