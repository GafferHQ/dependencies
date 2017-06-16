// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the audio.
// Multiply-included message file, hence no include guard.

#include <string>

#include "base/basictypes.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "content/common/content_export.h"
#include "content/common/media/media_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "media/audio/audio_input_ipc.h"
#include "media/audio/audio_output_ipc.h"
#include "media/audio/audio_parameters.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START AudioMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioInputIPCDelegateState,
                          media::AUDIO_INPUT_IPC_DELEGATE_STATE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioOutputIPCDelegateState,
                          media::AUDIO_OUTPUT_IPC_DELEGATE_STATE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media::SwitchOutputDeviceResult,
                          media::SWITCH_OUTPUT_DEVICE_RESULT_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media::AudioParameters::Format,
                          media::AudioParameters::AUDIO_FORMAT_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media::ChannelLayout, media::CHANNEL_LAYOUT_MAX)

IPC_STRUCT_BEGIN(AudioInputHostMsg_CreateStream_Config)
  IPC_STRUCT_MEMBER(media::AudioParameters, params)
  IPC_STRUCT_MEMBER(bool, automatic_gain_control)
  IPC_STRUCT_MEMBER(uint32, shared_memory_count)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Tell the renderer process that an audio stream has been created.
// The renderer process is given a shared memory handle for the audio data
// buffer it shares with the browser process. It is also given a SyncSocket that
// it uses to communicate with the browser process about the state of the
// buffered audio data.
IPC_MESSAGE_CONTROL4(
   AudioMsg_NotifyStreamCreated,
   int /* stream id */,
   base::SharedMemoryHandle /* handle */,
   base::SyncSocket::TransitDescriptor /* socket descriptor */,
   uint32 /* length */)

// Tell the renderer process that an audio input stream has been created.
// The renderer process would be given a SyncSocket that it should read
// from from then on. It is also given number of segments in shared memory.
IPC_MESSAGE_CONTROL5(
   AudioInputMsg_NotifyStreamCreated,
   int /* stream id */,
   base::SharedMemoryHandle /* handle */,
   base::SyncSocket::TransitDescriptor /* socket descriptor */,
   uint32 /* length */,
   uint32 /* segment count */)

// Notification message sent from AudioRendererHost to renderer for state
// update after the renderer has requested a Create/Start/Close.
IPC_MESSAGE_CONTROL2(AudioMsg_NotifyStreamStateChanged,
                     int /* stream id */,
                     media::AudioOutputIPCDelegateState /* new state */)

// Notification message sent from browser to renderer for state update.
IPC_MESSAGE_CONTROL2(AudioInputMsg_NotifyStreamStateChanged,
                     int /* stream id */,
                     media::AudioInputIPCDelegateState /* new state */)

IPC_MESSAGE_CONTROL2(AudioInputMsg_NotifyStreamVolume,
                     int /* stream id */,
                     double /* volume */)

// Notification message sent from AudioRendererHost to renderer for state
// update after the renderer has requested a SwitchOutputDevice.
IPC_MESSAGE_CONTROL3(AudioMsg_NotifyOutputDeviceSwitched,
                     int /* stream id */,
                     int /* request id */,
                     media::SwitchOutputDeviceResult /* result */)

// Messages sent from the renderer to the browser.

// Request that is sent to the browser for creating an audio output stream.
// |render_frame_id| is the routing ID for the RenderFrame producing the audio
// data.
IPC_MESSAGE_CONTROL4(AudioHostMsg_CreateStream,
                     int /* stream_id */,
                     int /* render_frame_id */,
                     int /* session_id */,
                     media::AudioParameters /* params */)

// Request that is sent to the browser for creating an audio input stream.
// |render_frame_id| is the routing ID for the RenderFrame consuming the audio
// data.
IPC_MESSAGE_CONTROL4(AudioInputHostMsg_CreateStream,
                     int /* stream_id */,
                     int /* render_frame_id */,
                     int /* session_id */,
                     AudioInputHostMsg_CreateStream_Config)

// Start buffering and play the audio stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_PlayStream,
                     int /* stream_id */)

// Start recording the audio input stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioInputHostMsg_RecordStream,
                     int /* stream_id */)

// Pause the audio stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_PauseStream,
                     int /* stream_id */)

// Close an audio stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_CloseStream,
                     int /* stream_id */)

// Close an audio input stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioInputHostMsg_CloseStream,
                     int /* stream_id */)

// Set audio volume of the stream specified by stream_id.
// TODO(hclam): change this to vector if we have channel numbers other than 2.
IPC_MESSAGE_CONTROL2(AudioHostMsg_SetVolume,
                     int /* stream_id */,
                     double /* volume */)

// Set audio volume of the input stream specified by stream_id.
IPC_MESSAGE_CONTROL2(AudioInputHostMsg_SetVolume,
                     int /* stream_id */,
                     double /* volume */)

// Switch the output device of the stream specified by stream_id.
IPC_MESSAGE_CONTROL5(AudioHostMsg_SwitchOutputDevice,
                     int /* stream_id */,
                     int /* render_frame_id */,
                     std::string /* device_id */,
                     GURL /* security_origin */,
                     int /* request_id */)
