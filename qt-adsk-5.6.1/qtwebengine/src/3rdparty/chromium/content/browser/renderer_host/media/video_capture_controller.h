// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureController is the glue between a VideoCaptureDevice and all
// VideoCaptureHosts that have connected to it. A controller exists on behalf of
// one (and only one) VideoCaptureDevice; both are owned by the
// VideoCaptureManager.
//
// The VideoCaptureController is responsible for:
//
//   * Allocating and keeping track of shared memory buffers, and filling them
//     with I420 video frames for IPC communication between VideoCaptureHost (in
//     the browser process) and VideoCaptureMessageFilter (in the renderer
//     process).
//   * Broadcasting the events from a single VideoCaptureDevice, fanning them
//     out to multiple clients.
//   * Keeping track of the clients on behalf of the VideoCaptureManager, making
//     it possible for the Manager to delete the Controller and its Device when
//     there are no clients left.
//
// Interactions between VideoCaptureController and other classes:
//
//   * VideoCaptureController indirectly observes a VideoCaptureDevice
//     by means of its proxy, VideoCaptureDeviceClient, which implements
//     the VideoCaptureDevice::Client interface. The proxy forwards
//     observed events to the VideoCaptureController on the IO thread.
//   * A VideoCaptureController interacts with its clients (VideoCaptureHosts)
//     via the VideoCaptureControllerEventHandler interface.
//   * Conversely, a VideoCaptureControllerEventHandler (typically,
//     VideoCaptureHost) will interact directly with VideoCaptureController to
//     return leased buffers by means of the ReturnBuffer() public method of
//     VCC.
//   * VideoCaptureManager (which owns the VCC) interacts directly with
//     VideoCaptureController through its public methods, to add and remove
//     clients.
//
// VideoCaptureController is not thread safe and operates on the IO thread only.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_

#include <list>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "media/base/video_capture_types.h"
#include "media/video/capture/video_capture_device.h"

namespace content {
class VideoCaptureBufferPool;

class CONTENT_EXPORT VideoCaptureController {
 public:
  // |max_buffers| is the maximum number of video frame buffers in-flight at any
  // one time.  This value should be based on the logical capacity of the
  // capture pipeline, and not on hardware performance.  For example, tab
  // capture requires more buffers than webcam capture because the pipeline is
  // longer (it includes read-backs pending in the GPU pipeline).
  explicit VideoCaptureController(int max_buffers);
  virtual ~VideoCaptureController();

  base::WeakPtr<VideoCaptureController> GetWeakPtrForIOThread();

  // Return a new VideoCaptureDeviceClient to forward capture events to this
  // instance. Some device clients need to allocate resources for the given
  // capture |format| and/or work on Capture Thread (|capture_task_runner|).
  scoped_ptr<media::VideoCaptureDevice::Client> NewDeviceClient(
      const scoped_refptr<base::SingleThreadTaskRunner>& capture_task_runner);

  // Start video capturing and try to use the resolution specified in |params|.
  // Buffers will be shared to the client as necessary. The client will continue
  // to receive frames from the device until RemoveClient() is called.
  void AddClient(VideoCaptureControllerID id,
                 VideoCaptureControllerEventHandler* event_handler,
                 base::ProcessHandle render_process,
                 media::VideoCaptureSessionId session_id,
                 const media::VideoCaptureParams& params);

  // Stop video capture. This will take back all buffers held by by
  // |event_handler|, and |event_handler| shouldn't use those buffers any more.
  // Returns the session_id of the stopped client, or
  // kInvalidMediaCaptureSessionId if the indicated client was not registered.
  int RemoveClient(VideoCaptureControllerID id,
                   VideoCaptureControllerEventHandler* event_handler);

  // Pause or resume the video capture for specified client.
  void PauseOrResumeClient(VideoCaptureControllerID id,
                           VideoCaptureControllerEventHandler* event_handler,
                           bool pause);

  int GetClientCount() const;

  // Return the number of clients that aren't paused.
  int GetActiveClientCount() const;

  // API called directly by VideoCaptureManager in case the device is
  // prematurely closed.
  void StopSession(int session_id);

  // Return a buffer with id |buffer_id| previously given in
  // VideoCaptureControllerEventHandler::OnBufferReady. In the case that the
  // buffer was backed by a texture, |sync_point| will be waited on before
  // destroying or recycling the texture, to synchronize with texture users in
  // the renderer process. If the consumer provided resource utilization
  // feedback, this will be passed here (-1.0 indicates no feedback).
  void ReturnBuffer(VideoCaptureControllerID id,
                    VideoCaptureControllerEventHandler* event_handler,
                    int buffer_id,
                    uint32 sync_point,
                    double consumer_resource_utilization);

  const media::VideoCaptureFormat& GetVideoCaptureFormat() const;

  bool has_received_frames() const { return has_received_frames_; }

  // Worker functions on IO thread. Called by the VideoCaptureDeviceClient.
  void DoIncomingCapturedVideoFrameOnIOThread(
      scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
      const scoped_refptr<media::VideoFrame>& frame,
      const base::TimeTicks& timestamp);
  void DoErrorOnIOThread();
  void DoLogOnIOThread(const std::string& message);
  void DoBufferDestroyedOnIOThread(int buffer_id_to_drop);

 private:
  struct ControllerClient;
  typedef std::list<ControllerClient*> ControllerClients;

  // Find a client of |id| and |handler| in |clients|.
  ControllerClient* FindClient(VideoCaptureControllerID id,
                               VideoCaptureControllerEventHandler* handler,
                               const ControllerClients& clients);

  // Find a client of |session_id| in |clients|.
  ControllerClient* FindClient(int session_id,
                               const ControllerClients& clients);

  // The pool of shared-memory buffers used for capturing.
  const scoped_refptr<VideoCaptureBufferPool> buffer_pool_;

  // All clients served by this controller.
  ControllerClients controller_clients_;

  // Takes on only the states 'STARTED' and 'ERROR'. 'ERROR' is an absorbing
  // state which stops the flow of data to clients.
  VideoCaptureState state_;

  // True if the controller has received a video frame from the device.
  bool has_received_frames_;

  media::VideoCaptureFormat video_capture_format_;

  base::WeakPtrFactory<VideoCaptureController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
