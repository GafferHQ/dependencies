// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_device.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

#if defined(USE_DRM_ATOMIC)
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#endif

namespace ui {

namespace {

typedef base::Callback<void(uint32_t /* frame */,
                            uint32_t /* seconds */,
                            uint32_t /* useconds */,
                            uint64_t /* id */)> DrmEventHandler;

bool DrmCreateDumbBuffer(int fd,
                         const SkImageInfo& info,
                         uint32_t* handle,
                         uint32_t* stride) {
  struct drm_mode_create_dumb request;
  memset(&request, 0, sizeof(request));
  request.width = info.width();
  request.height = info.height();
  request.bpp = info.bytesPerPixel() << 3;
  request.flags = 0;

  if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &request) < 0) {
    VPLOG(2) << "Cannot create dumb buffer";
    return false;
  }

  // The driver may choose to align the last row as well. We don't care about
  // the last alignment bits since they aren't used for display purposes, so
  // just check that the expected size is <= to what the driver allocated.
  DCHECK_LE(info.getSafeSize(request.pitch), request.size);

  *handle = request.handle;
  *stride = request.pitch;
  return true;
}

bool DrmDestroyDumbBuffer(int fd, uint32_t handle) {
  struct drm_mode_destroy_dumb destroy_request;
  memset(&destroy_request, 0, sizeof(destroy_request));
  destroy_request.handle = handle;
  return !drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
}

bool ProcessDrmEvent(int fd, const DrmEventHandler& callback) {
  char buffer[1024];
  int len = read(fd, buffer, sizeof(buffer));
  if (len == 0)
    return false;

  if (len < static_cast<int>(sizeof(drm_event))) {
    PLOG(ERROR) << "Failed to read DRM event";
    return false;
  }

  int idx = 0;
  while (idx < len) {
    DCHECK_LE(static_cast<int>(sizeof(drm_event)), len - idx);
    drm_event event;
    memcpy(&event, &buffer[idx], sizeof(event));
    switch (event.type) {
      case DRM_EVENT_FLIP_COMPLETE: {
        DCHECK_LE(static_cast<int>(sizeof(drm_event_vblank)), len - idx);
        drm_event_vblank vblank;
        memcpy(&vblank, &buffer[idx], sizeof(vblank));
        callback.Run(vblank.sequence, vblank.tv_sec, vblank.tv_usec,
                     vblank.user_data);
      } break;
      case DRM_EVENT_VBLANK:
        break;
      default:
        NOTREACHED();
        break;
    }

    idx += event.length;
  }

  return true;
}

bool CanQueryForResources(int fd) {
  drm_mode_card_res resources;
  memset(&resources, 0, sizeof(resources));
  // If there is no error getting DRM resources then assume this is a
  // modesetting device.
  return !drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &resources);
}

}  // namespace

class DrmDevice::PageFlipManager
    : public base::RefCountedThreadSafe<DrmDevice::PageFlipManager> {
 public:
  PageFlipManager() : next_id_(0) {}

  void OnPageFlip(uint32_t frame,
                  uint32_t seconds,
                  uint32_t useconds,
                  uint64_t id) {
    auto it =
        std::find_if(callbacks_.begin(), callbacks_.end(), FindCallback(id));
    if (it == callbacks_.end()) {
      LOG(WARNING) << "Could not find callback for page flip id=" << id;
      return;
    }

    DrmDevice::PageFlipCallback callback = it->callback;
    callbacks_.erase(it);
    callback.Run(frame, seconds, useconds);
  }

  uint64_t GetNextId() { return next_id_++; }

  void RegisterCallback(uint64_t id,
                        const DrmDevice::PageFlipCallback& callback) {
    callbacks_.push_back({id, callback});
  }

 private:
  friend class base::RefCountedThreadSafe<DrmDevice::PageFlipManager>;
  ~PageFlipManager() {}

  struct PageFlip {
    uint64_t id;
    DrmDevice::PageFlipCallback callback;
  };

  struct FindCallback {
    FindCallback(uint64_t id) : id(id) {}

    bool operator()(const PageFlip& flip) const { return flip.id == id; }

    uint64_t id;
  };

  uint64_t next_id_;

  std::vector<PageFlip> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PageFlipManager);
};

class DrmDevice::IOWatcher
    : public base::RefCountedThreadSafe<DrmDevice::IOWatcher>,
      public base::MessagePumpLibevent::Watcher {
 public:
  IOWatcher(int fd,
            const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
            const scoped_refptr<DrmDevice::PageFlipManager>& page_flip_manager)
      : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        io_task_runner_(io_task_runner),
        page_flip_manager_(page_flip_manager),
        paused_(true),
        fd_(fd) {}

  void SetPaused(bool paused) {
    if (paused_ == paused)
      return;

    paused_ = paused;
    base::WaitableEvent done(false, false);
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&IOWatcher::SetPausedOnIO, this, &done));
    done.Wait();
  }

  void Shutdown() {
    if (!paused_)
      io_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&IOWatcher::UnregisterOnIO, this));
  }

 private:
  friend class base::RefCountedThreadSafe<IOWatcher>;

  ~IOWatcher() override {}

  void RegisterOnIO() {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        fd_, true, base::MessageLoopForIO::WATCH_READ, &controller_, this);
  }

  void UnregisterOnIO() {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    controller_.StopWatchingFileDescriptor();
  }

  void SetPausedOnIO(base::WaitableEvent* done) {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    if (paused_)
      UnregisterOnIO();
    else
      RegisterOnIO();
    done->Signal();
  }

  void OnPageFlipOnIO(uint32_t frame,
                      uint32_t seconds,
                      uint32_t useconds,
                      uint64_t id) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DrmDevice::PageFlipManager::OnPageFlip, page_flip_manager_,
                   frame, seconds, useconds, id));
  }

  // base::MessagePumpLibevent::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    TRACE_EVENT1("drm", "OnDrmEvent", "socket", fd);

    if (!ProcessDrmEvent(
            fd, base::Bind(&DrmDevice::IOWatcher::OnPageFlipOnIO, this)))
      UnregisterOnIO();
  }

  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  scoped_refptr<DrmDevice::PageFlipManager> page_flip_manager_;

  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  bool paused_;
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(IOWatcher);
};

DrmDevice::DrmDevice(const base::FilePath& device_path, base::File file)
    : device_path_(device_path),
      file_(file.Pass()),
      page_flip_manager_(new PageFlipManager()) {
}

DrmDevice::~DrmDevice() {
  if (watcher_)
    watcher_->Shutdown();
}

bool DrmDevice::Initialize(bool use_atomic) {
  // Ignore devices that cannot perform modesetting.
  if (!CanQueryForResources(file_.GetPlatformFile())) {
    VLOG(2) << "Cannot query for resources for '" << device_path_.value()
            << "'";
    return false;
  }

#if defined(USE_DRM_ATOMIC)
  // Use atomic only if the build, kernel & flags all allow it.
  if (use_atomic && SetCapability(DRM_CLIENT_CAP_ATOMIC, 1))
    plane_manager_.reset(new HardwareDisplayPlaneManagerAtomic());
#endif  // defined(USE_DRM_ATOMIC)

  if (!plane_manager_)
    plane_manager_.reset(new HardwareDisplayPlaneManagerLegacy());
  if (!plane_manager_->Initialize(this)) {
    LOG(ERROR) << "Failed to initialize the plane manager for "
               << device_path_.value();
    plane_manager_.reset();
    return false;
  }

  return true;
}

void DrmDevice::InitializeTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(!task_runner_);
  task_runner_ = task_runner;
  watcher_ =
      new IOWatcher(file_.GetPlatformFile(), task_runner_, page_flip_manager_);
}

ScopedDrmCrtcPtr DrmDevice::GetCrtc(uint32_t crtc_id) {
  DCHECK(file_.IsValid());
  return ScopedDrmCrtcPtr(drmModeGetCrtc(file_.GetPlatformFile(), crtc_id));
}

bool DrmDevice::SetCrtc(uint32_t crtc_id,
                        uint32_t framebuffer,
                        std::vector<uint32_t> connectors,
                        drmModeModeInfo* mode) {
  DCHECK(file_.IsValid());
  DCHECK(!connectors.empty());
  DCHECK(mode);

  TRACE_EVENT2("drm", "DrmDevice::SetCrtc", "crtc", crtc_id, "size",
               gfx::Size(mode->hdisplay, mode->vdisplay).ToString());
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, framebuffer, 0, 0,
                         vector_as_array(&connectors), connectors.size(), mode);
}

bool DrmDevice::SetCrtc(drmModeCrtc* crtc, std::vector<uint32_t> connectors) {
  DCHECK(file_.IsValid());
  // If there's no buffer then the CRTC was disabled.
  if (!crtc->buffer_id)
    return DisableCrtc(crtc->crtc_id);

  DCHECK(!connectors.empty());

  TRACE_EVENT1("drm", "DrmDevice::RestoreCrtc", "crtc", crtc->crtc_id);
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc->crtc_id,
                         crtc->buffer_id, crtc->x, crtc->y,
                         vector_as_array(&connectors), connectors.size(),
                         &crtc->mode);
}

bool DrmDevice::DisableCrtc(uint32_t crtc_id) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::DisableCrtc", "crtc", crtc_id);
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, 0, 0, 0, NULL, 0,
                         NULL);
}

ScopedDrmConnectorPtr DrmDevice::GetConnector(uint32_t connector_id) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::GetConnector", "connector", connector_id);
  return ScopedDrmConnectorPtr(
      drmModeGetConnector(file_.GetPlatformFile(), connector_id));
}

bool DrmDevice::AddFramebuffer(uint32_t width,
                               uint32_t height,
                               uint8_t depth,
                               uint8_t bpp,
                               uint32_t stride,
                               uint32_t handle,
                               uint32_t* framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::AddFramebuffer", "handle", handle);
  return !drmModeAddFB(file_.GetPlatformFile(), width, height, depth, bpp,
                       stride, handle, framebuffer);
}

bool DrmDevice::RemoveFramebuffer(uint32_t framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::RemoveFramebuffer", "framebuffer",
               framebuffer);
  return !drmModeRmFB(file_.GetPlatformFile(), framebuffer);
}

bool DrmDevice::PageFlip(uint32_t crtc_id,
                         uint32_t framebuffer,
                         bool is_sync,
                         const PageFlipCallback& callback) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::PageFlip", "crtc", crtc_id, "framebuffer",
               framebuffer);

  if (watcher_)
    watcher_->SetPaused(is_sync);

  // NOTE: Calling drmModeSetCrtc will immediately update the state, though
  // callbacks to already scheduled page flips will be honored by the kernel.
  uint64_t id = page_flip_manager_->GetNextId();
  if (!drmModePageFlip(file_.GetPlatformFile(), crtc_id, framebuffer,
                       DRM_MODE_PAGE_FLIP_EVENT, reinterpret_cast<void*>(id))) {
    // If successful the payload will be removed by a PageFlip event.
    page_flip_manager_->RegisterCallback(id, callback);

    // If the flip was requested synchronous or if no watcher has been installed
    // yet, then synchronously handle the page flip events.
    if (is_sync || !watcher_) {
      TRACE_EVENT1("drm", "OnDrmEvent", "socket", file_.GetPlatformFile());

      ProcessDrmEvent(
          file_.GetPlatformFile(),
          base::Bind(&PageFlipManager::OnPageFlip, page_flip_manager_));
    }

    return true;
  }

  return false;
}

bool DrmDevice::PageFlipOverlay(uint32_t crtc_id,
                                uint32_t framebuffer,
                                const gfx::Rect& location,
                                const gfx::Rect& source,
                                int overlay_plane) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::PageFlipOverlay", "crtc", crtc_id,
               "framebuffer", framebuffer);
  return !drmModeSetPlane(file_.GetPlatformFile(), overlay_plane, crtc_id,
                          framebuffer, 0, location.x(), location.y(),
                          location.width(), location.height(), source.x(),
                          source.y(), source.width(), source.height());
}

ScopedDrmFramebufferPtr DrmDevice::GetFramebuffer(uint32_t framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::GetFramebuffer", "framebuffer", framebuffer);
  return ScopedDrmFramebufferPtr(
      drmModeGetFB(file_.GetPlatformFile(), framebuffer));
}

ScopedDrmPropertyPtr DrmDevice::GetProperty(drmModeConnector* connector,
                                            const char* name) {
  TRACE_EVENT2("drm", "DrmDevice::GetProperty", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0)
      return property.Pass();
  }

  return ScopedDrmPropertyPtr();
}

bool DrmDevice::SetProperty(uint32_t connector_id,
                            uint32_t property_id,
                            uint64_t value) {
  DCHECK(file_.IsValid());
  return !drmModeConnectorSetProperty(file_.GetPlatformFile(), connector_id,
                                      property_id, value);
}

bool DrmDevice::GetCapability(uint64_t capability, uint64_t* value) {
  DCHECK(file_.IsValid());
  return !drmGetCap(file_.GetPlatformFile(), capability, value);
}

ScopedDrmPropertyBlobPtr DrmDevice::GetPropertyBlob(drmModeConnector* connector,
                                                    const char* name) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::GetPropertyBlob", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0 &&
        property->flags & DRM_MODE_PROP_BLOB)
      return ScopedDrmPropertyBlobPtr(drmModeGetPropertyBlob(
          file_.GetPlatformFile(), connector->prop_values[i]));
  }

  return ScopedDrmPropertyBlobPtr();
}

bool DrmDevice::SetCursor(uint32_t crtc_id,
                          uint32_t handle,
                          const gfx::Size& size) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::SetCursor", "handle", handle);
  return !drmModeSetCursor(file_.GetPlatformFile(), crtc_id, handle,
                           size.width(), size.height());
}

bool DrmDevice::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  DCHECK(file_.IsValid());
  return !drmModeMoveCursor(file_.GetPlatformFile(), crtc_id, point.x(),
                            point.y());
}

bool DrmDevice::CreateDumbBuffer(const SkImageInfo& info,
                                 uint32_t* handle,
                                 uint32_t* stride) {
  DCHECK(file_.IsValid());

  TRACE_EVENT0("drm", "DrmDevice::CreateDumbBuffer");
  return DrmCreateDumbBuffer(file_.GetPlatformFile(), info, handle, stride);
}

bool DrmDevice::DestroyDumbBuffer(uint32_t handle) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::DestroyDumbBuffer", "handle", handle);
  return DrmDestroyDumbBuffer(file_.GetPlatformFile(), handle);
}

bool DrmDevice::MapDumbBuffer(uint32_t handle, size_t size, void** pixels) {
  struct drm_mode_map_dumb map_request;
  memset(&map_request, 0, sizeof(map_request));
  map_request.handle = handle;
  if (drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_MODE_MAP_DUMB,
               &map_request)) {
    PLOG(ERROR) << "Cannot prepare dumb buffer for mapping";
    return false;
  }

  *pixels = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                 file_.GetPlatformFile(), map_request.offset);
  if (*pixels == MAP_FAILED) {
    PLOG(ERROR) << "Cannot mmap dumb buffer";
    return false;
  }

  return true;
}

bool DrmDevice::UnmapDumbBuffer(void* pixels, size_t size) {
  return !munmap(pixels, size);
}

bool DrmDevice::CloseBufferHandle(uint32_t handle) {
  struct drm_gem_close close_request;
  memset(&close_request, 0, sizeof(close_request));
  close_request.handle = handle;
  return !drmIoctl(file_.GetPlatformFile(), DRM_IOCTL_GEM_CLOSE,
                   &close_request);
}

bool DrmDevice::CommitProperties(drmModePropertySet* properties,
                                 uint32_t flags,
                                 bool is_sync,
                                 bool test_only,
                                 const PageFlipCallback& callback) {
#if defined(USE_DRM_ATOMIC)
  if (test_only)
    flags |= DRM_MODE_ATOMIC_TEST_ONLY;
  else
    flags |= DRM_MODE_PAGE_FLIP_EVENT;
  uint64_t id = page_flip_manager_->GetNextId();
  if (!drmModePropertySetCommit(file_.GetPlatformFile(), flags,
                                reinterpret_cast<void*>(id), properties)) {
    if (test_only)
      return true;
    page_flip_manager_->RegisterCallback(id, callback);

    // If the flip was requested synchronous or if no watcher has been installed
    // yet, then synchronously handle the page flip events.
    if (is_sync || !watcher_) {
      TRACE_EVENT1("drm", "OnDrmEvent", "socket", file_.GetPlatformFile());

      ProcessDrmEvent(
          file_.GetPlatformFile(),
          base::Bind(&PageFlipManager::OnPageFlip, page_flip_manager_));
    }
    return true;
  }
#endif  // defined(USE_DRM_ATOMIC)
  return false;
}

bool DrmDevice::SetCapability(uint64_t capability, uint64_t value) {
  DCHECK(file_.IsValid());
  return !drmSetClientCap(file_.GetPlatformFile(), capability, value);
}

bool DrmDevice::SetMaster() {
  TRACE_EVENT1("drm", "DrmDevice::SetMaster", "path", device_path_.value());
  DCHECK(file_.IsValid());
  return (drmSetMaster(file_.GetPlatformFile()) == 0);
}

bool DrmDevice::DropMaster() {
  TRACE_EVENT1("drm", "DrmDevice::DropMaster", "path", device_path_.value());
  DCHECK(file_.IsValid());
  return (drmDropMaster(file_.GetPlatformFile()) == 0);
}

bool DrmDevice::SetGammaRamp(uint32_t crtc_id,
                             const std::vector<GammaRampRGBEntry>& lut) {
  ScopedDrmCrtcPtr crtc = GetCrtc(crtc_id);

  // TODO(robert.bradford) resample the incoming ramp to match what the kernel
  // expects.
  if (static_cast<size_t>(crtc->gamma_size) != lut.size()) {
    LOG(ERROR) << "Gamma table size mismatch: supplied " << lut.size()
               << " expected " << crtc->gamma_size;
  }

  std::vector<uint16_t> r, g, b;
  r.reserve(lut.size());
  g.reserve(lut.size());
  b.reserve(lut.size());

  for (size_t i = 0; i < lut.size(); ++i) {
    r.push_back(lut[i].r);
    g.push_back(lut[i].g);
    b.push_back(lut[i].b);
  }

  DCHECK(file_.IsValid());
  TRACE_EVENT0("drm", "DrmDevice::SetGamma");
  return (drmModeCrtcSetGamma(file_.GetPlatformFile(), crtc_id, r.size(), &r[0],
                              &g[0], &b[0]) == 0);
}

}  // namespace ui
