// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "content/renderer/media/media_stream_source.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

class MediaStreamVideoTrack;
class VideoTrackAdapter;

// MediaStreamVideoSource is an interface used for sending video frames to a
// MediaStreamVideoTrack.
// http://dev.w3.org/2011/webrtc/editor/getusermedia.html
// The purpose of this base class is to be able to implement different
// MediaStreaVideoSources such as local video capture, video sources received
// on a PeerConnection or a source created in NaCl.
// All methods calls will be done from the main render thread.
//
// When the first track is added to the source by calling AddTrack, the
// MediaStreamVideoSource implementation calls GetCurrentSupportedFormats.
// The source implementation must call OnSupportedFormats.
// MediaStreamVideoSource then match the constraints provided in AddTrack with
// the formats and call StartSourceImpl. The source implementation must call
// OnStartDone when the underlying source has been started or failed to start.
class CONTENT_EXPORT MediaStreamVideoSource
    : public MediaStreamSource,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  MediaStreamVideoSource();
  virtual ~MediaStreamVideoSource();

  // Returns the MediaStreamVideoSource object owned by |source|.
  static MediaStreamVideoSource* GetVideoSource(
      const blink::WebMediaStreamSource& source);

  // Puts |track| in the registered tracks list.
  void AddTrack(MediaStreamVideoTrack* track,
                const VideoCaptureDeliverFrameCB& frame_callback,
                const blink::WebMediaConstraints& constraints,
                const ConstraintsCallback& callback);
  void RemoveTrack(MediaStreamVideoTrack* track);

  // Return true if |name| is a constraint supported by MediaStreamVideoSource.
  static bool IsConstraintSupported(const std::string& name);

  // Returns the task runner where video frames will be delivered on.
  base::SingleThreadTaskRunner* io_task_runner() const;

  // Constraint keys used by a video source.
  // Specified by draft-alvestrand-constraints-resolution-00b
  static const char kMinAspectRatio[];  // minAspectRatio
  static const char kMaxAspectRatio[];  // maxAspectRatio
  static const char kMaxWidth[];  // maxWidth
  static const char kMinWidth[];  // minWidth
  static const char kMaxHeight[];  // maxHeight
  static const char kMinHeight[];  // minHeight
  static const char kMaxFrameRate[];  // maxFrameRate
  static const char kMinFrameRate[];  // minFrameRate

  enum {
    // Default resolution. If no constraints are specified and the delegate
    // support it, this is the resolution that will be used.
    kDefaultWidth = 640,
    kDefaultHeight = 480,

    kDefaultFrameRate = 30,
    kUnknownFrameRate = 0,
  };

 protected:
  void DoStopSource() override;

  // Sets ready state and notifies the ready state to all registered tracks.
  virtual void SetReadyState(blink::WebMediaStreamSource::ReadyState state);

  // Sets muted state and notifies it to all registered tracks.
  virtual void SetMutedState(bool state);

  // An implementation must fetch the formats that can currently be used by
  // the source and call OnSupportedFormats when done.
  // |max_requested_height| and |max_requested_width| is the max height and
  // width set as a mandatory constraint if set when calling
  // MediaStreamVideoSource::AddTrack. If max height and max width is not set
  // |max_requested_height| and |max_requested_width| are 0.
  virtual void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) = 0;

  // An implementation must start capturing frames using the requested
  // |format|. The fulfilled |constraints| are provided as additional context,
  // and may be used to modify the behavior of the source. When the source has
  // started or the source failed to start OnStartDone must be called. An
  // implementation must call |frame_callback| on the IO thread with the
  // captured frames.
  virtual void StartSourceImpl(
      const media::VideoCaptureFormat& format,
      const blink::WebMediaConstraints& constraints,
      const VideoCaptureDeliverFrameCB& frame_callback) = 0;
  void OnStartDone(MediaStreamRequestResult result);

  // An implementation must immediately stop capture video frames and must not
  // call OnSupportedFormats after this method has been called. After this
  // method has been called, MediaStreamVideoSource may be deleted.
  virtual void StopSourceImpl() = 0;

  enum State {
    NEW,
    RETRIEVING_CAPABILITIES,
    STARTING,
    STARTED,
    ENDED
  };
  State state() const { return state_; }

 private:
  void OnSupportedFormats(const media::VideoCaptureFormats& formats);

  // Finds the first WebMediaConstraints in |requested_constraints_| that allows
  // the use of one of the |formats|.  |best_format| and |fulfilled_constraints|
  // are set to the results of this search-and-match operation.  Returns false
  // if no WebMediaConstraints allow the use any of the |formats|.
  bool FindBestFormatWithConstraints(
      const media::VideoCaptureFormats& formats,
      media::VideoCaptureFormat* best_format,
      blink::WebMediaConstraints* fulfilled_constraints);

  // Trigger all cached callbacks from AddTrack. AddTrack is successful
  // if the capture delegate has started and the constraints provided in
  // AddTrack match the format that was used to start the device.
  // Note that it must be ok to delete the MediaStreamVideoSource object
  // in the context of the callback. If gUM fail, the implementation will
  // simply drop the references to the blink source and track which will lead
  // to that this object is deleted.
  void FinalizeAddTrack();

  State state_;

  media::VideoCaptureFormat current_format_;

  struct RequestedConstraints {
    RequestedConstraints(MediaStreamVideoTrack* track,
                         const VideoCaptureDeliverFrameCB& frame_callback,
                         const blink::WebMediaConstraints& constraints,
                         const ConstraintsCallback& callback);
    ~RequestedConstraints();

    MediaStreamVideoTrack* track;
    VideoCaptureDeliverFrameCB frame_callback;
    blink::WebMediaConstraints constraints;
    ConstraintsCallback callback;
  };
  std::vector<RequestedConstraints> requested_constraints_;

  media::VideoCaptureFormats supported_formats_;

  // |track_adapter_| delivers video frames to the tracks on the IO-thread.
  const scoped_refptr<VideoTrackAdapter> track_adapter_;

  // Tracks that currently are connected to this source.
  std::vector<MediaStreamVideoTrack*> tracks_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaStreamVideoSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_SOURCE_H_
