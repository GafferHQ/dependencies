// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureManager is used to open/close, start/stop, enumerate available
// video capture devices, and manage VideoCaptureController's.
// All functions are expected to be called from Browser::IO thread. Some helper
// functions (*OnDeviceThread) will dispatch operations to the device thread.
// VideoCaptureManager will open OS dependent instances of VideoCaptureDevice.
// A device can only be opened once.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_checker.h"
#include "base/timer/elapsed_timer.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "media/base/video_capture_types.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_device_factory.h"
#include "media/video/capture/video_capture_device_info.h"

namespace content {
class VideoCaptureController;
class VideoCaptureControllerEventHandler;

// VideoCaptureManager opens/closes and start/stops video capture devices.
class CONTENT_EXPORT VideoCaptureManager : public MediaStreamProvider {
 public:
  // Callback used to signal the completion of a controller lookup.
  typedef base::Callback<
      void(const base::WeakPtr<VideoCaptureController>&)> DoneCB;

  explicit VideoCaptureManager(
      scoped_ptr<media::VideoCaptureDeviceFactory> factory);

  void Unregister();

  // Implements MediaStreamProvider.
  void Register(MediaStreamProviderListener* listener,
                const scoped_refptr<base::SingleThreadTaskRunner>&
                    device_task_runner) override;

  void EnumerateDevices(MediaStreamType stream_type) override;

  int Open(const StreamDeviceInfo& device) override;

  void Close(int capture_session_id) override;

  // Called by VideoCaptureHost to locate a capture device for |capture_params|,
  // adding the Host as a client of the device's controller if successful. The
  // value of |session_id| controls which device is selected;
  // this value should be a session id previously returned by Open().
  //
  // If the device is not already started (i.e., no other client is currently
  // capturing from this device), this call will cause a VideoCaptureController
  // and VideoCaptureDevice to be created, possibly asynchronously.
  //
  // On success, the controller is returned via calling |done_cb|, indicating
  // that the client was successfully added. A NULL controller is passed to
  // the callback on failure.
  void StartCaptureForClient(media::VideoCaptureSessionId session_id,
                             const media::VideoCaptureParams& capture_params,
                             base::ProcessHandle client_render_process,
                             VideoCaptureControllerID client_id,
                             VideoCaptureControllerEventHandler* client_handler,
                             const DoneCB& done_cb);

  // Called by VideoCaptureHost to remove |client_handler|. If this is the last
  // client of the device, the |controller| and its VideoCaptureDevice may be
  // destroyed. The client must not access |controller| after calling this
  // function.
  void StopCaptureForClient(VideoCaptureController* controller,
                            VideoCaptureControllerID client_id,
                            VideoCaptureControllerEventHandler* client_handler,
                            bool aborted_due_to_error);

  // Called by VideoCaptureHost to pause to update video buffer specified by
  // |client_id| and |client_handler|. If all clients of |controller| are
  // paused, the corresponding device will be closed.
  void PauseCaptureForClient(
      VideoCaptureController* controller,
      VideoCaptureControllerID client_id,
      VideoCaptureControllerEventHandler* client_handler);

  // Called by VideoCaptureHost to resume to update video buffer specified by
  // |client_id| and |client_handler|. The |session_id| and |params| should be
  // same as those used in StartCaptureForClient().
  // If this is first active client of |controller|, device will be allocated
  // and it will take a little time to resume.
  // Allocating device could failed if other app holds the camera, the error
  // will be notified through VideoCaptureControllerEventHandler::OnError().
  void ResumeCaptureForClient(
      media::VideoCaptureSessionId session_id,
      const media::VideoCaptureParams& params,
      VideoCaptureController* controller,
      VideoCaptureControllerID client_id,
      VideoCaptureControllerEventHandler* client_handler);

  // Retrieves all capture supported formats for a particular device. Returns
  // false if the |capture_session_id| is not found. The supported formats are
  // cached during device(s) enumeration, and depending on the underlying
  // implementation, could be an empty list.
  bool GetDeviceSupportedFormats(
      media::VideoCaptureSessionId capture_session_id,
      media::VideoCaptureFormats* supported_formats);

  // Retrieves the format(s) currently in use.  Returns false if the
  // |capture_session_id| is not found. Returns true and |formats_in_use|
  // otherwise. |formats_in_use| is empty if the device is not in use.
  bool GetDeviceFormatsInUse(media::VideoCaptureSessionId capture_session_id,
                             media::VideoCaptureFormats* formats_in_use);

  // Sets the platform-dependent window ID for the desktop capture notification
  // UI for the given session.
  void SetDesktopCaptureWindowId(media::VideoCaptureSessionId session_id,
                                 gfx::NativeViewId window_id);

  // Gets a weak reference to the device factory, used for tests.
  media::VideoCaptureDeviceFactory* video_capture_device_factory() const {
    return video_capture_device_factory_.get();
  }

#if defined(OS_WIN)
  void set_device_task_runner(
      const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner) {
    device_task_runner_ = device_task_runner;
  }
#endif

  // Returns the SingleThreadTaskRunner where devices are enumerated on and
  // started.
  scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner() {
    return device_task_runner_;
  }
 private:
  ~VideoCaptureManager() override;
  class DeviceEntry;

  // Checks to see if |entry| has no clients left on its controller. If so,
  // remove it from the list of devices, and delete it asynchronously. |entry|
  // may be freed by this function.
  void DestroyDeviceEntryIfNoClients(DeviceEntry* entry);

  // Helpers to report an event to our Listener.
  void OnOpened(MediaStreamType type,
                media::VideoCaptureSessionId capture_session_id);
  void OnClosed(MediaStreamType type,
                media::VideoCaptureSessionId capture_session_id);
  void OnDevicesInfoEnumerated(
      MediaStreamType stream_type,
      base::ElapsedTimer* timer,
      const media::VideoCaptureDeviceInfos& new_devices_info_cache);

  // Finds a DeviceEntry by its device ID and type, if it is already opened.
  DeviceEntry* GetDeviceEntryForMediaStreamDevice(
      const MediaStreamDevice& device_info);

  // Finds a DeviceEntry entry for the indicated session, creating a fresh one
  // if necessary. Returns NULL if the session id is invalid.
  DeviceEntry* GetOrCreateDeviceEntry(
      media::VideoCaptureSessionId capture_session_id);

  // Finds the DeviceEntry that owns a particular controller pointer.
  DeviceEntry* GetDeviceEntryForController(
      const VideoCaptureController* controller) const;

  bool IsOnDeviceThread() const;

  // Consolidates the cached devices list with the list of currently connected
  // devices in the system |names_snapshot|. Retrieves the supported formats of
  // the new devices and sends the new cache to OnDevicesInfoEnumerated().
  void ConsolidateDevicesInfoOnDeviceThread(
      base::Callback<void(const media::VideoCaptureDeviceInfos&)>
          on_devices_enumerated_callback,
      MediaStreamType stream_type,
      const media::VideoCaptureDeviceInfos& old_device_info_cache,
      scoped_ptr<media::VideoCaptureDevice::Names> names_snapshot);

  // Starting a capture device can take 1-2 seconds.
  // To avoid multiple unnecessary start/stop commands to the OS, each start
  // request is queued in |device_start_queue_|.
  // QueueStartDevice creates a new entry in |device_start_queue_| and posts a
  // request to start the device on the device thread unless there is
  // another request pending start.
  void QueueStartDevice(media::VideoCaptureSessionId session_id,
                        DeviceEntry* entry,
                        const media::VideoCaptureParams& params);
  void OnDeviceStarted(int serial_id,
                       scoped_ptr<media::VideoCaptureDevice> device);
  void DoStopDevice(DeviceEntry* entry);
  void HandleQueuedStartRequest();

  // Creates and Starts a new VideoCaptureDevice. The resulting
  // VideoCaptureDevice is returned to the IO-thread and stored in
  // a DeviceEntry in |devices_|. Ownership of |client| passes to
  // the device.
  scoped_ptr<media::VideoCaptureDevice> DoStartDeviceOnDeviceThread(
      media::VideoCaptureSessionId session_id,
      const std::string& device_id,
      MediaStreamType stream_type,
      const media::VideoCaptureParams& params,
      scoped_ptr<media::VideoCaptureDevice::Client> client);

  // Stops and destroys the VideoCaptureDevice held in
  // |device|.
  void DoStopDeviceOnDeviceThread(scoped_ptr<media::VideoCaptureDevice> device);

  media::VideoCaptureDeviceInfo* FindDeviceInfoById(
      const std::string& id,
      media::VideoCaptureDeviceInfos& device_vector);

  void MaybePostDesktopCaptureWindowId(media::VideoCaptureSessionId session_id);
  void SetDesktopCaptureWindowIdOnDeviceThread(
      media::VideoCaptureDevice* device,
      gfx::NativeViewId window_id);

  // The message loop of media stream device thread, where VCD's live.
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  media::VideoCaptureSessionId new_capture_session_id_;

  typedef std::map<media::VideoCaptureSessionId, MediaStreamDevice> SessionMap;
  // An entry is kept in this map for every session that has been created via
  // the Open() entry point. The keys are session_id's. This map is used to
  // determine which device to use when StartCaptureForClient() occurs. Used
  // only on the IO thread.
  SessionMap sessions_;

  // An entry, kept in a map, that owns a VideoCaptureDevice and its associated
  // VideoCaptureController. VideoCaptureManager owns all VideoCaptureDevices
  // and VideoCaptureControllers and is responsible for deleting the instances
  // when they are not used any longer.
  //
  // The set of currently started VideoCaptureDevice and VideoCaptureController
  // objects is only accessed from IO thread.
  class DeviceEntry {
   public:
    DeviceEntry(MediaStreamType stream_type,
                const std::string& id,
                scoped_ptr<VideoCaptureController> controller);
    ~DeviceEntry();

    const int serial_id;
    const MediaStreamType stream_type;
    const std::string id;

    VideoCaptureController* video_capture_controller();
    media::VideoCaptureDevice* video_capture_device();

    void SetVideoCaptureDevice(scoped_ptr<media::VideoCaptureDevice> device);
    scoped_ptr<media::VideoCaptureDevice> ReleaseVideoCaptureDevice();

   private:
    // The controller.
    scoped_ptr<VideoCaptureController> video_capture_controller_;

    // The capture device.
    scoped_ptr<media::VideoCaptureDevice> video_capture_device_;

    base::ThreadChecker thread_checker_;
  };

  typedef ScopedVector<DeviceEntry> DeviceEntries;
  // Currently opened devices. The device may or may not be started.
  DeviceEntries devices_;

  // Class used for queuing request for starting a device.
  class CaptureDeviceStartRequest {
   public:
    CaptureDeviceStartRequest(
        int serial_id,
        media::VideoCaptureSessionId session_id,
        const media::VideoCaptureParams& params);
    int serial_id() const { return serial_id_;}
    media::VideoCaptureSessionId session_id() const { return session_id_; }
    media::VideoCaptureParams params() const { return params_; }

    // Set to true if the device should be stopped before it has successfully
    // been started.
    bool abort_start() const { return abort_start_; }
    void set_abort_start() { abort_start_ = true; }

   private:
    const int serial_id_;
    const media::VideoCaptureSessionId session_id_;
    const media::VideoCaptureParams params_;
    // Set to true if the device should be stopped before it has successfully
    // been started.
    bool abort_start_;
  };

  typedef std::list<CaptureDeviceStartRequest> DeviceStartQueue;
  DeviceStartQueue device_start_queue_;

  // Device creation factory injected on construction from MediaStreamManager or
  // from the test harness.
  scoped_ptr<media::VideoCaptureDeviceFactory> video_capture_device_factory_;

  // Local cache of the enumerated video capture devices' names and capture
  // supported formats. A snapshot of the current devices and their capabilities
  // is composed in VideoCaptureDeviceFactory::EnumerateDeviceNames() and
  // ConsolidateDevicesInfoOnDeviceThread(), and this snapshot is used to update
  // this list in OnDevicesInfoEnumerated(). GetDeviceSupportedFormats() will
  // use this list if the device is not started, otherwise it will retrieve the
  // active device capture format from the VideoCaptureController associated.
  media::VideoCaptureDeviceInfos devices_info_cache_;

  // Map used by DesktopCapture.
  std::map<media::VideoCaptureSessionId, gfx::NativeViewId>
      notification_window_ids_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
