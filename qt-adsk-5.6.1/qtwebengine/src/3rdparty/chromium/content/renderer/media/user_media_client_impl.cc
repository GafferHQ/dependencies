// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/user_media_client_impl.h"

#include <utility>

#include "base/hash.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrackSourcesRequest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {
namespace {

void CopyStreamConstraints(const blink::WebMediaConstraints& constraints,
                           StreamOptions::Constraints* mandatory,
                           StreamOptions::Constraints* optional) {
  blink::WebVector<blink::WebMediaConstraint> mandatory_constraints;
  constraints.getMandatoryConstraints(mandatory_constraints);
  for (size_t i = 0; i < mandatory_constraints.size(); i++) {
    mandatory->push_back(StreamOptions::Constraint(
        mandatory_constraints[i].m_name.utf8(),
        mandatory_constraints[i].m_value.utf8()));
  }

  blink::WebVector<blink::WebMediaConstraint> optional_constraints;
  constraints.getOptionalConstraints(optional_constraints);
  for (size_t i = 0; i < optional_constraints.size(); i++) {
    optional->push_back(StreamOptions::Constraint(
        optional_constraints[i].m_name.utf8(),
        optional_constraints[i].m_value.utf8()));
  }
}

static int g_next_request_id  = 0;

}  // namespace

struct UserMediaClientImpl::MediaDevicesRequestInfo {
  MediaDevicesRequestInfo(const blink::WebMediaDevicesRequest& request,
                          int audio_input_request_id,
                          int video_input_request_id,
                          int audio_output_request_id)
      : media_devices_request(request),
        audio_input_request_id(audio_input_request_id),
        video_input_request_id(video_input_request_id),
        audio_output_request_id(audio_output_request_id),
        has_audio_input_returned(false),
        has_video_input_returned(false),
        has_audio_output_returned(false) {}

  MediaDevicesRequestInfo(
      const blink::WebMediaStreamTrackSourcesRequest& request,
      int audio_input_request_id,
      int video_input_request_id)
      : sources_request(request),
        audio_input_request_id(audio_input_request_id),
        video_input_request_id(video_input_request_id),
        audio_output_request_id(-1),
        has_audio_input_returned(false),
        has_video_input_returned(false),
        has_audio_output_returned(false) {}

  bool IsSourcesRequest() {
    // We can't check isNull() on |media_devices_request| and |sources_request|,
    // because in unit tests they will always be null.
    return audio_output_request_id == -1;
  }

  blink::WebMediaDevicesRequest media_devices_request;
  blink::WebMediaStreamTrackSourcesRequest sources_request;
  int audio_input_request_id;
  int video_input_request_id;
  int audio_output_request_id;
  bool has_audio_input_returned;
  bool has_video_input_returned;
  bool has_audio_output_returned;
  StreamDeviceInfoArray audio_input_devices;
  StreamDeviceInfoArray video_input_devices;
  StreamDeviceInfoArray audio_output_devices;
};

UserMediaClientImpl::UserMediaClientImpl(
    RenderFrame* render_frame,
    PeerConnectionDependencyFactory* dependency_factory,
    scoped_ptr<MediaStreamDispatcher> media_stream_dispatcher)
    : RenderFrameObserver(render_frame),
      dependency_factory_(dependency_factory),
      media_stream_dispatcher_(media_stream_dispatcher.Pass()),
      weak_factory_(this) {
  DCHECK(dependency_factory_);
  DCHECK(media_stream_dispatcher_.get());
}

UserMediaClientImpl::~UserMediaClientImpl() {
  // Force-close all outstanding user media requests and local sources here,
  // before the outstanding WeakPtrs are invalidated, to ensure a clean
  // shutdown.
  FrameWillClose();
}

void UserMediaClientImpl::requestUserMedia(
    const blink::WebUserMediaRequest& user_media_request) {
  // Save histogram data so we can see how much GetUserMedia is used.
  // The histogram counts the number of calls to the JS API
  // webGetUserMedia.
  UpdateWebRTCMethodCount(WEBKIT_GET_USER_MEDIA);
  DCHECK(CalledOnValidThread());

  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->peer_connection_tracker()->TrackGetUserMedia(
        user_media_request);
  }

  int request_id = g_next_request_id++;
  StreamOptions options;
  GURL security_origin;
  bool enable_automatic_output_device_selection = false;

  // |user_media_request| can't be mocked. So in order to test at all we check
  // if it isNull.
  if (user_media_request.isNull()) {
    // We are in a test.
    options.audio_requested = true;
    options.video_requested = true;
  } else {
    if (user_media_request.audio()) {
      options.audio_requested = true;
      CopyStreamConstraints(user_media_request.audioConstraints(),
                            &options.mandatory_audio,
                            &options.optional_audio);

      // Check if this input device should be used to select a matching output
      // device for audio rendering.
      std::string enable;
      if (options.GetFirstAudioConstraintByName(
              kMediaStreamRenderToAssociatedSink, &enable, NULL) &&
          base::LowerCaseEqualsASCII(enable, "true")) {
        enable_automatic_output_device_selection = true;
      }
    }
    if (user_media_request.video()) {
      options.video_requested = true;
      CopyStreamConstraints(user_media_request.videoConstraints(),
                            &options.mandatory_video,
                            &options.optional_video);
    }

    security_origin = GURL(user_media_request.securityOrigin().toString());
    DCHECK(render_frame()->GetWebFrame() ==
               static_cast<blink::WebFrame*>(
                   user_media_request.ownerDocument().frame()));
  }

  DVLOG(1) << "UserMediaClientImpl::requestUserMedia(" << request_id << ", [ "
           << "audio=" << (options.audio_requested)
           << " select associated sink: "
           << enable_automatic_output_device_selection
           << ", video=" << (options.video_requested) << " ], "
           << security_origin.spec() << ")";

  std::string audio_device_id;
  bool mandatory_audio;
  options.GetFirstAudioConstraintByName(kMediaStreamSourceInfoId,
                                        &audio_device_id, &mandatory_audio);
  std::string video_device_id;
  bool mandatory_video;
  options.GetFirstVideoConstraintByName(kMediaStreamSourceInfoId,
                                        &video_device_id, &mandatory_video);

  WebRtcLogMessage(base::StringPrintf(
      "MSI::requestUserMedia. request_id=%d"
      ", audio source id=%s mandatory= %s "
      ", video source id=%s mandatory= %s",
      request_id,
      audio_device_id.c_str(),
      mandatory_audio ? "true":"false",
      video_device_id.c_str(),
      mandatory_video ? "true":"false"));

  user_media_requests_.push_back(
      new UserMediaRequestInfo(request_id, user_media_request,
                               enable_automatic_output_device_selection));

  media_stream_dispatcher_->GenerateStream(
      request_id,
      weak_factory_.GetWeakPtr(),
      options,
      security_origin);
}

void UserMediaClientImpl::cancelUserMediaRequest(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK(CalledOnValidThread());
  UserMediaRequestInfo* request = FindUserMediaRequestInfo(user_media_request);
  if (request) {
    // We can't abort the stream generation process.
    // Instead, erase the request. Once the stream is generated we will stop the
    // stream if the request does not exist.
    LogUserMediaRequestWithNoResult(MEDIA_STREAM_REQUEST_EXPLICITLY_CANCELLED);
    DeleteUserMediaRequestInfo(request);
  }
}

void UserMediaClientImpl::requestMediaDevices(
    const blink::WebMediaDevicesRequest& media_devices_request) {
  UpdateWebRTCMethodCount(WEBKIT_GET_MEDIA_DEVICES);
  DCHECK(CalledOnValidThread());

  int audio_input_request_id = g_next_request_id++;
  int video_input_request_id = g_next_request_id++;
  int audio_output_request_id = g_next_request_id++;

  // |media_devices_request| can't be mocked, so in tests it will be empty (the
  // underlying pointer is null). In order to use this function in a test we
  // need to check if it isNull.
  GURL security_origin;
  if (!media_devices_request.isNull())
    security_origin = GURL(media_devices_request.securityOrigin().toString());

  DVLOG(1) << "UserMediaClientImpl::requestMediaDevices("
           << audio_input_request_id
           << ", " << video_input_request_id << ", " << audio_output_request_id
           << ", " << security_origin.spec() << ")";

  media_devices_requests_.push_back(new MediaDevicesRequestInfo(
      media_devices_request,
      audio_input_request_id,
      video_input_request_id,
      audio_output_request_id));

  media_stream_dispatcher_->EnumerateDevices(
      audio_input_request_id,
      weak_factory_.GetWeakPtr(),
      MEDIA_DEVICE_AUDIO_CAPTURE,
      security_origin);

  media_stream_dispatcher_->EnumerateDevices(
      video_input_request_id,
      weak_factory_.GetWeakPtr(),
      MEDIA_DEVICE_VIDEO_CAPTURE,
      security_origin);

  media_stream_dispatcher_->EnumerateDevices(
      audio_output_request_id,
      weak_factory_.GetWeakPtr(),
      MEDIA_DEVICE_AUDIO_OUTPUT,
      security_origin);
}

void UserMediaClientImpl::cancelMediaDevicesRequest(
    const blink::WebMediaDevicesRequest& media_devices_request) {
  DCHECK(CalledOnValidThread());
  MediaDevicesRequestInfo* request =
      FindMediaDevicesRequestInfo(media_devices_request);
  if (!request)
    return;
  CancelAndDeleteMediaDevicesRequest(request);
}

void UserMediaClientImpl::requestSources(
    const blink::WebMediaStreamTrackSourcesRequest& sources_request) {
  // We don't call UpdateWebRTCMethodCount() here to track the API count in UMA
  // stats. This is instead counted in MediaStreamTrack::getSources in blink.
  DCHECK(CalledOnValidThread());

  int audio_input_request_id = g_next_request_id++;
  int video_input_request_id = g_next_request_id++;

  // |sources_request| can't be mocked, so in tests it will be empty (the
  // underlying pointer is null). In order to use this function in a test we
  // need to check if it isNull.
  GURL security_origin;
  if (!sources_request.isNull())
    security_origin = GURL(sources_request.origin().utf8());

  DVLOG(1) << "UserMediaClientImpl::requestSources("
           << audio_input_request_id
           << ", " << video_input_request_id
           << ", " << security_origin.spec() << ")";

  media_devices_requests_.push_back(new MediaDevicesRequestInfo(
      sources_request,
      audio_input_request_id,
      video_input_request_id));

  media_stream_dispatcher_->EnumerateDevices(
      audio_input_request_id,
      weak_factory_.GetWeakPtr(),
      MEDIA_DEVICE_AUDIO_CAPTURE,
      security_origin);

  media_stream_dispatcher_->EnumerateDevices(
      video_input_request_id,
      weak_factory_.GetWeakPtr(),
      MEDIA_DEVICE_VIDEO_CAPTURE,
      security_origin);
}

// Callback from MediaStreamDispatcher.
// The requested stream have been generated by the MediaStreamDispatcher.
void UserMediaClientImpl::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnStreamGenerated stream:" << label;

  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(request_id);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    DVLOG(1) << "Request ID not found";
    OnStreamGeneratedForCancelledRequest(audio_array, video_array);
    return;
  }
  request_info->generated = true;

  // WebUserMediaRequest don't have an implementation in unit tests.
  // Therefore we need to check for isNull here and initialize the
  // constraints.
  blink::WebUserMediaRequest* request = &(request_info->request);
  blink::WebMediaConstraints audio_constraints;
  blink::WebMediaConstraints video_constraints;
  if (request->isNull()) {
    audio_constraints.initialize();
    video_constraints.initialize();
  } else {
    audio_constraints = request->audioConstraints();
    video_constraints = request->videoConstraints();
  }

  blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
      audio_array.size());
  CreateAudioTracks(audio_array, audio_constraints, &audio_track_vector,
                    request_info);

  blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
      video_array.size());
  CreateVideoTracks(video_array, video_constraints, &video_track_vector,
                    request_info);

  blink::WebString webkit_id = base::UTF8ToUTF16(label);
  blink::WebMediaStream* web_stream = &(request_info->web_stream);

  web_stream->initialize(webkit_id, audio_track_vector,
                         video_track_vector);
  web_stream->setExtraData(
      new MediaStream(
          *web_stream));

  // Wait for the tracks to be started successfully or to fail.
  request_info->CallbackOnTracksStarted(
      base::Bind(&UserMediaClientImpl::OnCreateNativeTracksCompleted,
                 weak_factory_.GetWeakPtr()));
}

void UserMediaClientImpl::OnStreamGeneratedForCancelledRequest(
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  // Only stop the device if the device is not used in another MediaStream.
  for (StreamDeviceInfoArray::const_iterator device_it = audio_array.begin();
       device_it != audio_array.end(); ++device_it) {
    if (!FindLocalSource(*device_it))
      media_stream_dispatcher_->StopStreamDevice(*device_it);
  }

  for (StreamDeviceInfoArray::const_iterator device_it = video_array.begin();
       device_it != video_array.end(); ++device_it) {
    if (!FindLocalSource(*device_it))
      media_stream_dispatcher_->StopStreamDevice(*device_it);
  }
}

void UserMediaClientImpl::FinalizeEnumerateDevices(
    MediaDevicesRequestInfo* request) {
  // All devices are ready for copying. We use a hashed audio output device id
  // as the group id for input and output audio devices. If an input device
  // doesn't have an associated output device, we use the input device's own id.
  // We don't support group id for video devices, that's left empty.
  blink::WebVector<blink::WebMediaDeviceInfo>
      devices(request->audio_input_devices.size() +
              request->video_input_devices.size() +
              request->audio_output_devices.size());
  for (size_t i = 0; i  < request->audio_input_devices.size(); ++i) {
    const MediaStreamDevice& device = request->audio_input_devices[i].device;
    DCHECK_EQ(device.type, MEDIA_DEVICE_AUDIO_CAPTURE);
    std::string group_id = base::UintToString(base::Hash(
        !device.matched_output_device_id.empty() ?
            device.matched_output_device_id :
            device.id));
    devices[i].initialize(
        blink::WebString::fromUTF8(device.id),
        blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput,
        blink::WebString::fromUTF8(device.name),
        blink::WebString::fromUTF8(group_id));
  }
  size_t offset = request->audio_input_devices.size();
  for (size_t i = 0; i  < request->video_input_devices.size(); ++i) {
    const MediaStreamDevice& device = request->video_input_devices[i].device;
    DCHECK_EQ(device.type, MEDIA_DEVICE_VIDEO_CAPTURE);
    devices[offset + i].initialize(
        blink::WebString::fromUTF8(device.id),
        blink::WebMediaDeviceInfo::MediaDeviceKindVideoInput,
        blink::WebString::fromUTF8(device.name),
        blink::WebString());
  }
  offset += request->video_input_devices.size();
  for (size_t i = 0; i  < request->audio_output_devices.size(); ++i) {
    const MediaStreamDevice& device = request->audio_output_devices[i].device;
    DCHECK_EQ(device.type, MEDIA_DEVICE_AUDIO_OUTPUT);
    devices[offset + i].initialize(
        blink::WebString::fromUTF8(device.id),
        blink::WebMediaDeviceInfo::MediaDeviceKindAudioOutput,
        blink::WebString::fromUTF8(device.name),
        blink::WebString::fromUTF8(base::UintToString(base::Hash(device.id))));
  }

  EnumerateDevicesSucceded(&request->media_devices_request, devices);
}

void UserMediaClientImpl::FinalizeEnumerateSources(
    MediaDevicesRequestInfo* request) {
  blink::WebVector<blink::WebSourceInfo>
      sources(request->audio_input_devices.size() +
              request->video_input_devices.size());
  for (size_t i = 0; i  < request->audio_input_devices.size(); ++i) {
    const MediaStreamDevice& device = request->audio_input_devices[i].device;
    DCHECK_EQ(device.type, MEDIA_DEVICE_AUDIO_CAPTURE);
    std::string group_id = base::UintToString(base::Hash(
        !device.matched_output_device_id.empty() ?
            device.matched_output_device_id :
            device.id));
    sources[i].initialize(blink::WebString::fromUTF8(device.id),
                          blink::WebSourceInfo::SourceKindAudio,
                          blink::WebString::fromUTF8(device.name),
                          blink::WebSourceInfo::VideoFacingModeNone);
  }
  size_t offset = request->audio_input_devices.size();
  for (size_t i = 0; i  < request->video_input_devices.size(); ++i) {
    const MediaStreamDevice& device = request->video_input_devices[i].device;
    DCHECK_EQ(device.type, MEDIA_DEVICE_VIDEO_CAPTURE);
    blink::WebSourceInfo::VideoFacingMode video_facing;
    switch (device.video_facing) {
      case MEDIA_VIDEO_FACING_USER:
        video_facing = blink::WebSourceInfo::VideoFacingModeUser;
        break;
      case MEDIA_VIDEO_FACING_ENVIRONMENT:
        video_facing = blink::WebSourceInfo::VideoFacingModeEnvironment;
        break;
      default:
        video_facing = blink::WebSourceInfo::VideoFacingModeNone;
    }
    sources[offset + i].initialize(blink::WebString::fromUTF8(device.id),
                                   blink::WebSourceInfo::SourceKindVideo,
                                   blink::WebString::fromUTF8(device.name),
                                   video_facing);
  }

  EnumerateSourcesSucceded(&request->sources_request, sources);
}

// Callback from MediaStreamDispatcher.
// The requested stream failed to be generated.
void UserMediaClientImpl::OnStreamGenerationFailed(
    int request_id,
    MediaStreamRequestResult result) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnStreamGenerationFailed("
           << request_id << ")";
  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(request_id);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    DVLOG(1) << "Request ID not found";
    return;
  }

  GetUserMediaRequestFailed(request_info->request, result, "");
  DeleteUserMediaRequestInfo(request_info);
}

// Callback from MediaStreamDispatcher.
// The browser process has stopped a device used by a MediaStream.
void UserMediaClientImpl::OnDeviceStopped(
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnDeviceStopped("
           << "{device_id = " << device_info.device.id << "})";

  const blink::WebMediaStreamSource* source_ptr = FindLocalSource(device_info);
  if (!source_ptr) {
    // This happens if the same device is used in several guM requests or
    // if a user happen stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    return;
  }
  // By creating |source| it is guaranteed that the blink::WebMediaStreamSource
  // object is valid during the cleanup.
  blink::WebMediaStreamSource source(*source_ptr);
  StopLocalSource(source, false);

  for (LocalStreamSources::iterator device_it = local_sources_.begin();
       device_it != local_sources_.end(); ++device_it) {
    if (device_it->id() == source.id()) {
      local_sources_.erase(device_it);
      break;
    }
  }
}

void UserMediaClientImpl::InitializeSourceObject(
    const StreamDeviceInfo& device,
    blink::WebMediaStreamSource::Type type,
    const blink::WebMediaConstraints& constraints,
    blink::WebMediaStreamSource* webkit_source) {
  const blink::WebMediaStreamSource* existing_source =
      FindLocalSource(device);
  if (existing_source) {
    *webkit_source = *existing_source;
    DVLOG(1) << "Source already exist. Reusing source with id "
             << webkit_source->id().utf8();
    return;
  }

  webkit_source->initialize(
      base::UTF8ToUTF16(device.device.id),
      type,
      base::UTF8ToUTF16(device.device.name),
      false /* remote */, true /* readonly */);

  DVLOG(1) << "Initialize source object :"
           << "id = " << webkit_source->id().utf8()
           << ", name = " << webkit_source->name().utf8();

  if (type == blink::WebMediaStreamSource::TypeVideo) {
    webkit_source->setExtraData(
        CreateVideoSource(
            device,
            base::Bind(&UserMediaClientImpl::OnLocalSourceStopped,
                       weak_factory_.GetWeakPtr())));
  } else {
    DCHECK_EQ(blink::WebMediaStreamSource::TypeAudio, type);
    MediaStreamAudioSource* audio_source(
        new MediaStreamAudioSource(
            RenderFrameObserver::routing_id(),
            device,
            base::Bind(&UserMediaClientImpl::OnLocalSourceStopped,
                       weak_factory_.GetWeakPtr()),
            dependency_factory_));
    webkit_source->setExtraData(audio_source);
  }
  local_sources_.push_back(*webkit_source);
}

MediaStreamVideoSource* UserMediaClientImpl::CreateVideoSource(
    const StreamDeviceInfo& device,
    const MediaStreamSource::SourceStoppedCallback& stop_callback) {
  content::MediaStreamVideoCapturerSource* ret =
      new content::MediaStreamVideoCapturerSource(
          stop_callback,
          make_scoped_ptr(new VideoCapturerDelegate(device)));
  ret->SetDeviceInfo(device);
  return ret;
}

void UserMediaClientImpl::CreateVideoTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
    UserMediaRequestInfo* request) {
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  for (size_t i = 0; i < devices.size(); ++i) {
    blink::WebMediaStreamSource webkit_source;
    InitializeSourceObject(devices[i],
                           blink::WebMediaStreamSource::TypeVideo,
                           constraints,
                           &webkit_source);
    (*webkit_tracks)[i] =
        request->CreateAndStartVideoTrack(webkit_source, constraints);
  }
}

void UserMediaClientImpl::CreateAudioTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
    UserMediaRequestInfo* request) {
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  // Log the device names for this request.
  for (StreamDeviceInfoArray::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    WebRtcLogMessage(base::StringPrintf(
        "Generated media stream for request id %d contains audio device name"
        " \"%s\"",
        request->request_id,
        it->device.name.c_str()));
  }

  StreamDeviceInfoArray overridden_audio_array = devices;
  if (!request->enable_automatic_output_device_selection) {
    // If the GetUserMedia request did not explicitly set the constraint
    // kMediaStreamRenderToAssociatedSink, the output device parameters must
    // be removed.
    for (StreamDeviceInfoArray::iterator it = overridden_audio_array.begin();
         it != overridden_audio_array.end(); ++it) {
      it->device.matched_output_device_id = "";
      it->device.matched_output = MediaStreamDevice::AudioDeviceParameters();
    }
  }

  for (size_t i = 0; i < overridden_audio_array.size(); ++i) {
    blink::WebMediaStreamSource webkit_source;
    InitializeSourceObject(overridden_audio_array[i],
                           blink::WebMediaStreamSource::TypeAudio,
                           constraints,
                           &webkit_source);
    (*webkit_tracks)[i].initialize(webkit_source);
    request->StartAudioTrack((*webkit_tracks)[i], constraints);
  }
}

void UserMediaClientImpl::OnCreateNativeTracksCompleted(
    UserMediaRequestInfo* request,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DVLOG(1) << "UserMediaClientImpl::OnCreateNativeTracksComplete("
           << "{request_id = " << request->request_id << "} "
           << "{result = " << result << "})";

  if (result == content::MEDIA_DEVICE_OK)
    GetUserMediaRequestSucceeded(request->web_stream, request->request);
  else
    GetUserMediaRequestFailed(request->request, result, result_name);

  DeleteUserMediaRequestInfo(request);
}

void UserMediaClientImpl::OnDevicesEnumerated(
    int request_id,
    const StreamDeviceInfoArray& device_array) {
  DVLOG(1) << "UserMediaClientImpl::OnDevicesEnumerated(" << request_id << ")";

  MediaDevicesRequestInfo* request = FindMediaDevicesRequestInfo(request_id);
  DCHECK(request);

  if (request_id == request->audio_input_request_id) {
    request->has_audio_input_returned = true;
    DCHECK(request->audio_input_devices.empty());
    request->audio_input_devices = device_array;
  } else if (request_id == request->video_input_request_id) {
    request->has_video_input_returned = true;
    DCHECK(request->video_input_devices.empty());
    request->video_input_devices = device_array;
  } else {
    DCHECK_EQ(request->audio_output_request_id, request_id);
    request->has_audio_output_returned = true;
    DCHECK(request->audio_output_devices.empty());
    request->audio_output_devices = device_array;
  }

  if (!request->has_audio_input_returned ||
      !request->has_video_input_returned ||
      (!request->IsSourcesRequest() && !request->has_audio_output_returned)) {
    // Wait for the rest of the devices to complete.
    return;
  }

  if (request->IsSourcesRequest())
    FinalizeEnumerateSources(request);
  else
    FinalizeEnumerateDevices(request);

  CancelAndDeleteMediaDevicesRequest(request);
}

void UserMediaClientImpl::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  DVLOG(1) << "UserMediaClientImpl::OnDeviceOpened("
           << request_id << ", " << label << ")";
  NOTIMPLEMENTED();
}

void UserMediaClientImpl::OnDeviceOpenFailed(int request_id) {
  DVLOG(1) << "UserMediaClientImpl::VideoDeviceOpenFailed("
           << request_id << ")";
  NOTIMPLEMENTED();
}

void UserMediaClientImpl::GetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest request_info) {
  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl is destroyed if the JavaScript code request the
  // frame to be destroyed within the scope of the callback. Therefore,
  // post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded,
                 weak_factory_.GetWeakPtr(), stream, request_info));
}

void UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest request_info) {
  DVLOG(1) << "UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded";
  LogUserMediaRequestResult(MEDIA_DEVICE_OK);
  request_info.requestSucceeded(stream);
}

void UserMediaClientImpl::GetUserMediaRequestFailed(
    blink::WebUserMediaRequest request_info,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl is destroyed if the JavaScript code request the
  // frame to be destroyed within the scope of the callback. Therefore,
  // post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&UserMediaClientImpl::DelayedGetUserMediaRequestFailed,
                 weak_factory_.GetWeakPtr(), request_info, result,
                 result_name));
}

void UserMediaClientImpl::DelayedGetUserMediaRequestFailed(
    blink::WebUserMediaRequest request_info,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  LogUserMediaRequestResult(result);
  switch (result) {
    case MEDIA_DEVICE_OK:
    case NUM_MEDIA_REQUEST_RESULTS:
      NOTREACHED();
      return;
    case MEDIA_DEVICE_PERMISSION_DENIED:
      request_info.requestDenied();
      return;
    case MEDIA_DEVICE_PERMISSION_DISMISSED:
      request_info.requestFailedUASpecific("PermissionDismissedError");
      return;
    case MEDIA_DEVICE_INVALID_STATE:
      request_info.requestFailedUASpecific("InvalidStateError");
      return;
    case MEDIA_DEVICE_NO_HARDWARE:
      request_info.requestFailedUASpecific("DevicesNotFoundError");
      return;
    case MEDIA_DEVICE_INVALID_SECURITY_ORIGIN:
      request_info.requestFailedUASpecific("InvalidSecurityOriginError");
      return;
    case MEDIA_DEVICE_TAB_CAPTURE_FAILURE:
      request_info.requestFailedUASpecific("TabCaptureError");
      return;
    case MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE:
      request_info.requestFailedUASpecific("ScreenCaptureError");
      return;
    case MEDIA_DEVICE_CAPTURE_FAILURE:
      request_info.requestFailedUASpecific("DeviceCaptureError");
      return;
    case MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED:
      request_info.requestFailedConstraint(result_name);
      return;
    case MEDIA_DEVICE_TRACK_START_FAILURE:
      request_info.requestFailedUASpecific("TrackStartError");
      return;
    case MEDIA_DEVICE_NOT_SUPPORTED:
      request_info.requestFailedUASpecific("MediaDeviceNotSupported");
      return;
    case MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN:
      request_info.requestFailedUASpecific("MediaDeviceFailedDueToShutdown");
      return;
  }
  NOTREACHED();
  request_info.requestFailed();
}

void UserMediaClientImpl::EnumerateDevicesSucceded(
    blink::WebMediaDevicesRequest* request,
    blink::WebVector<blink::WebMediaDeviceInfo>& devices) {
  request->requestSucceeded(devices);
}

void UserMediaClientImpl::EnumerateSourcesSucceded(
    blink::WebMediaStreamTrackSourcesRequest* request,
    blink::WebVector<blink::WebSourceInfo>& sources) {
  request->requestSucceeded(sources);
}

const blink::WebMediaStreamSource* UserMediaClientImpl::FindLocalSource(
    const StreamDeviceInfo& device) const {
  for (LocalStreamSources::const_iterator it = local_sources_.begin();
       it != local_sources_.end(); ++it) {
    MediaStreamSource* const source =
        static_cast<MediaStreamSource*>(it->extraData());
    const StreamDeviceInfo& active_device = source->device_info();
    if (active_device.device.id == device.device.id &&
        active_device.device.type == device.device.type &&
        active_device.session_id == device.session_id) {
      return &(*it);
    }
  }
  return NULL;
}

UserMediaClientImpl::UserMediaRequestInfo*
UserMediaClientImpl::FindUserMediaRequestInfo(int request_id) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it)->request_id == request_id)
      return (*it);
  }
  return NULL;
}

UserMediaClientImpl::UserMediaRequestInfo*
UserMediaClientImpl::FindUserMediaRequestInfo(
    const blink::WebUserMediaRequest& request) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it)->request == request)
      return (*it);
  }
  return NULL;
}

void UserMediaClientImpl::DeleteUserMediaRequestInfo(
    UserMediaRequestInfo* request) {
  UserMediaRequests::iterator it = user_media_requests_.begin();
  for (; it != user_media_requests_.end(); ++it) {
    if ((*it) == request) {
      user_media_requests_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void UserMediaClientImpl::DeleteAllUserMediaRequests() {
  UserMediaRequests::iterator request_it = user_media_requests_.begin();
  while (request_it != user_media_requests_.end()) {
    DVLOG(1) << "UserMediaClientImpl@" << this
             << "::DeleteAllUserMediaRequests: "
             << "Cancel user media request " << (*request_it)->request_id;
    // If the request is not generated, it means that a request
    // has been sent to the MediaStreamDispatcher to generate a stream
    // but MediaStreamDispatcher has not yet responded and we need to cancel
    // the request.
    if (!(*request_it)->generated) {
      DCHECK(!(*request_it)->HasPendingSources());
      media_stream_dispatcher_->CancelGenerateStream(
          (*request_it)->request_id, weak_factory_.GetWeakPtr());
      LogUserMediaRequestWithNoResult(MEDIA_STREAM_REQUEST_NOT_GENERATED);
    } else {
      DCHECK((*request_it)->HasPendingSources());
      LogUserMediaRequestWithNoResult(
          MEDIA_STREAM_REQUEST_PENDING_MEDIA_TRACKS);
    }
    request_it = user_media_requests_.erase(request_it);
  }
}

UserMediaClientImpl::MediaDevicesRequestInfo*
UserMediaClientImpl::FindMediaDevicesRequestInfo(
    int request_id) {
  MediaDevicesRequests::iterator it = media_devices_requests_.begin();
  for (; it != media_devices_requests_.end(); ++it) {
    if ((*it)->audio_input_request_id == request_id ||
        (*it)->video_input_request_id == request_id ||
        (*it)->audio_output_request_id == request_id) {
      return (*it);
    }
  }
  return NULL;
}

UserMediaClientImpl::MediaDevicesRequestInfo*
UserMediaClientImpl::FindMediaDevicesRequestInfo(
    const blink::WebMediaDevicesRequest& request) {
  MediaDevicesRequests::iterator it = media_devices_requests_.begin();
  for (; it != media_devices_requests_.end(); ++it) {
    if ((*it)->media_devices_request == request)
      return (*it);
  }
  return NULL;
}

void UserMediaClientImpl::CancelAndDeleteMediaDevicesRequest(
    MediaDevicesRequestInfo* request) {
  MediaDevicesRequests::iterator it = media_devices_requests_.begin();
  for (; it != media_devices_requests_.end(); ++it) {
    if ((*it) == request) {
      // Cancel device enumeration.
      media_stream_dispatcher_->StopEnumerateDevices(
          request->audio_input_request_id, weak_factory_.GetWeakPtr());
      media_stream_dispatcher_->StopEnumerateDevices(
          request->video_input_request_id, weak_factory_.GetWeakPtr());
      media_stream_dispatcher_->StopEnumerateDevices(
          request->audio_output_request_id, weak_factory_.GetWeakPtr());

      media_devices_requests_.erase(it);
      return;
    }
  }
  NOTREACHED();
}

void UserMediaClientImpl::FrameWillClose() {
  // Cancel all outstanding UserMediaRequests.
  DeleteAllUserMediaRequests();

  // Loop through all current local sources and stop the sources.
  LocalStreamSources::iterator sources_it = local_sources_.begin();
  while (sources_it != local_sources_.end()) {
    StopLocalSource(*sources_it, true);
    sources_it = local_sources_.erase(sources_it);
  }
}

void UserMediaClientImpl::OnLocalSourceStopped(
    const blink::WebMediaStreamSource& source) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnLocalSourceStopped";

  bool device_found = false;
  for (LocalStreamSources::iterator device_it = local_sources_.begin();
       device_it != local_sources_.end(); ++device_it) {
    if (device_it->id()  == source.id()) {
      device_found = true;
      local_sources_.erase(device_it);
      break;
    }
  }
  CHECK(device_found);

  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.extraData());
  media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());
}

void UserMediaClientImpl::StopLocalSource(
    const blink::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.extraData());
  DVLOG(1) << "UserMediaClientImpl::StopLocalSource("
           << "{device_id = " << source_impl->device_info().device.id << "})";

  if (notify_dispatcher)
    media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());

  source_impl->ResetSourceStoppedCallback();
  source_impl->StopSource();
}

UserMediaClientImpl::UserMediaRequestInfo::UserMediaRequestInfo(
    int request_id,
    const blink::WebUserMediaRequest& request,
    bool enable_automatic_output_device_selection)
    : request_id(request_id),
      generated(false),
      enable_automatic_output_device_selection(
          enable_automatic_output_device_selection),
      request(request),
      request_result_(MEDIA_DEVICE_OK),
      request_result_name_("") {
}

UserMediaClientImpl::UserMediaRequestInfo::~UserMediaRequestInfo() {
  DVLOG(1) << "~UserMediaRequestInfo";
}

void UserMediaClientImpl::UserMediaRequestInfo::StartAudioTrack(
    const blink::WebMediaStreamTrack& track,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(track.source().type() == blink::WebMediaStreamSource::TypeAudio);
  MediaStreamAudioSource* native_source =
      static_cast <MediaStreamAudioSource*>(track.source().extraData());
  DCHECK(native_source);

  sources_.push_back(track.source());
  sources_waiting_for_callback_.push_back(native_source);
  native_source->AddTrack(
      track, constraints, base::Bind(
          &UserMediaClientImpl::UserMediaRequestInfo::OnTrackStarted,
          AsWeakPtr()));
}

blink::WebMediaStreamTrack
UserMediaClientImpl::UserMediaRequestInfo::CreateAndStartVideoTrack(
    const blink::WebMediaStreamSource& source,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(source.type() == blink::WebMediaStreamSource::TypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  sources_.push_back(source);
  sources_waiting_for_callback_.push_back(native_source);
  return MediaStreamVideoTrack::CreateVideoTrack(
      native_source, constraints, base::Bind(
          &UserMediaClientImpl::UserMediaRequestInfo::OnTrackStarted,
          AsWeakPtr()),
      true);
}

void UserMediaClientImpl::UserMediaRequestInfo::CallbackOnTracksStarted(
    const ResourcesReady& callback) {
  DCHECK(ready_callback_.is_null());
  ready_callback_ = callback;
  CheckAllTracksStarted();
}

void UserMediaClientImpl::UserMediaRequestInfo::OnTrackStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DVLOG(1) << "OnTrackStarted result " << result;
  std::vector<MediaStreamSource*>::iterator it =
      std::find(sources_waiting_for_callback_.begin(),
                sources_waiting_for_callback_.end(),
                source);
  DCHECK(it != sources_waiting_for_callback_.end());
  sources_waiting_for_callback_.erase(it);
  // All tracks must be started successfully. Otherwise the request is a
  // failure.
  if (result != MEDIA_DEVICE_OK) {
    request_result_ = result;
    request_result_name_ = result_name;
  }

  CheckAllTracksStarted();
}

void UserMediaClientImpl::UserMediaRequestInfo::CheckAllTracksStarted() {
  if (!ready_callback_.is_null() && sources_waiting_for_callback_.empty()) {
    ready_callback_.Run(this, request_result_, request_result_name_);
  }
}

bool UserMediaClientImpl::UserMediaRequestInfo::IsSourceUsed(
    const blink::WebMediaStreamSource& source) const {
  for (std::vector<blink::WebMediaStreamSource>::const_iterator source_it =
           sources_.begin();
       source_it != sources_.end(); ++source_it) {
    if (source_it->id() == source.id())
      return true;
  }
  return false;
}

void UserMediaClientImpl::UserMediaRequestInfo::RemoveSource(
    const blink::WebMediaStreamSource& source) {
  for (std::vector<blink::WebMediaStreamSource>::iterator it =
           sources_.begin();
       it != sources_.end(); ++it) {
    if (source.id() == it->id()) {
      sources_.erase(it);
      return;
    }
  }
}

bool UserMediaClientImpl::UserMediaRequestInfo::HasPendingSources() const {
  return !sources_waiting_for_callback_.empty();
}

}  // namespace content
