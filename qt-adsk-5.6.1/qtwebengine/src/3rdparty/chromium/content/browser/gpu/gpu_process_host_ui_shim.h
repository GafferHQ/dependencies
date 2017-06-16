// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_UI_SHIM_H_
#define CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_UI_SHIM_H_

// This class lives on the UI thread and supports classes like the
// BackingStoreProxy, which must live on the UI thread. The IO thread
// portion of this class, the GpuProcessHost, is responsible for
// shuttling messages between the browser and GPU processes.

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "content/common/message_router.h"
#include "content/public/common/gpu_memory_stats.h"
#include "gpu/config/gpu_info.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

#if defined(OS_MACOSX)
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
#endif

namespace ui {
struct LatencyInfo;
}

namespace gfx {
class Size;
}

namespace IPC {
class Message;
}

namespace content {
void RouteToGpuProcessHostUIShimTask(int host_id, const IPC::Message& msg);

class GpuProcessHostUIShim : public IPC::Listener,
                             public IPC::Sender,
                             public base::NonThreadSafe {
 public:
  // Create a GpuProcessHostUIShim with the given ID.  The object can be found
  // using FromID with the same id.
  static GpuProcessHostUIShim* Create(int host_id);

  // Destroy the GpuProcessHostUIShim with the given host ID. This can only
  // be called on the UI thread. Only the GpuProcessHost should destroy the
  // UI shim.
  static void Destroy(int host_id, const std::string& message);

  // Destroy all remaining GpuProcessHostUIShims.
  CONTENT_EXPORT static void DestroyAll();

  CONTENT_EXPORT static GpuProcessHostUIShim* FromID(int host_id);

  // Get a GpuProcessHostUIShim instance; it doesn't matter which one.
  // Return NULL if none has been created.
  CONTENT_EXPORT static GpuProcessHostUIShim* GetOneInstance();

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener implementation.
  // The GpuProcessHost causes this to be called on the UI thread to
  // dispatch the incoming messages from the GPU process, which are
  // actually received on the IO thread.
  bool OnMessageReceived(const IPC::Message& message) override;

  CONTENT_EXPORT void RelinquishGpuResources(const base::Closure& callback);
  CONTENT_EXPORT void SimulateRemoveAllContext();
  CONTENT_EXPORT void SimulateCrash();
  CONTENT_EXPORT void SimulateHang();

 private:
  explicit GpuProcessHostUIShim(int host_id);
  ~GpuProcessHostUIShim() override;

  // Message handlers.
  bool OnControlMessageReceived(const IPC::Message& message);

  void OnLogMessage(int level, const std::string& header,
      const std::string& message);

  void OnGraphicsInfoCollected(const gpu::GPUInfo& gpu_info);

  void OnAcceleratedSurfaceInitialized(int32 surface_id, int32 route_id);
#if defined(OS_MACOSX)
  void OnAcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params);
#endif
  void OnVideoMemoryUsageStatsReceived(
      const GPUVideoMemoryUsageStats& video_memory_usage_stats);
  void OnResourcesRelinquished();
  void OnAddSubscription(int32 process_id, unsigned int target);
  void OnRemoveSubscription(int32 process_id, unsigned int target);

  // The serial number of the GpuProcessHost / GpuProcessHostUIShim pair.
  int host_id_;
  base::Closure relinquish_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_PROCESS_HOST_UI_SHIM_H_
