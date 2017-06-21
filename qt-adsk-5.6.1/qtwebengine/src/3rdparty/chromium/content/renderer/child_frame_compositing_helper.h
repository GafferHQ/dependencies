// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
#define CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SharedMemory;
}

namespace cc {
struct SurfaceId;
struct SurfaceSequence;

class CompositorFrame;
class Layer;
class SolidColorLayer;
class SurfaceLayer;
class DelegatedFrameProvider;
class DelegatedFrameResourceCollection;
class DelegatedRendererLayer;
}

namespace blink {
class WebFrame;
class WebPluginContainer;
class WebLayer;
}

namespace gfx {
class Rect;
class Size;
}

struct FrameHostMsg_CompositorFrameSwappedACK_Params;
struct FrameHostMsg_BuffersSwappedACK_Params;
struct FrameHostMsg_ReclaimCompositorResources_Params;

namespace content {

class BrowserPlugin;
class BrowserPluginManager;
class RenderFrameProxy;
class ThreadSafeSender;

class CONTENT_EXPORT ChildFrameCompositingHelper
    : public base::RefCounted<ChildFrameCompositingHelper>,
      public cc::DelegatedFrameResourceCollectionClient {
 public:
  static ChildFrameCompositingHelper* CreateForBrowserPlugin(
      const base::WeakPtr<BrowserPlugin>& browser_plugin);
  static ChildFrameCompositingHelper* CreateForRenderFrameProxy(
      RenderFrameProxy* render_frame_proxy);

  void DidCommitCompositorFrame();
  void EnableCompositing(bool);
  void OnContainerDestroy();
  void OnCompositorFrameSwapped(scoped_ptr<cc::CompositorFrame> frame,
                                int route_id,
                                uint32 output_surface_id,
                                int host_id,
                                base::SharedMemoryHandle handle);
  void OnSetSurface(const cc::SurfaceId& surface_id,
                    const gfx::Size& frame_size,
                    float scale_factor,
                    const cc::SurfaceSequence& sequence);
  void UpdateVisibility(bool);
  void ChildFrameGone();

  // cc::DelegatedFrameProviderClient implementation.
  void UnusedResourcesAreAvailable() override;
  void SetContentsOpaque(bool);

 protected:
  // Friend RefCounted so that the dtor can be non-public.
  friend class base::RefCounted<ChildFrameCompositingHelper>;

 private:
  ChildFrameCompositingHelper(
      const base::WeakPtr<BrowserPlugin>& browser_plugin,
      blink::WebFrame* frame,
      RenderFrameProxy* render_frame_proxy,
      int host_routing_id);

  virtual ~ChildFrameCompositingHelper();

  BrowserPluginManager* GetBrowserPluginManager();
  blink::WebPluginContainer* GetContainer();
  int GetInstanceID();

  void SendCompositorFrameSwappedACKToBrowser(
      FrameHostMsg_CompositorFrameSwappedACK_Params& params);
  void SendReclaimCompositorResourcesToBrowser(
      FrameHostMsg_ReclaimCompositorResources_Params& params);
  void CheckSizeAndAdjustLayerProperties(const gfx::Size& new_size,
                                         float device_scale_factor,
                                         cc::Layer* layer);
  void SendReturnedDelegatedResources();
  static void SatisfyCallback(scoped_refptr<ThreadSafeSender> sender,
                              int host_routing_id,
                              cc::SurfaceSequence sequence);
  static void SatisfyCallbackBrowserPlugin(
      scoped_refptr<ThreadSafeSender> sender,
      int host_routing_id,
      int browser_plugin_instance_id,
      cc::SurfaceSequence sequence);
  static void RequireCallback(scoped_refptr<ThreadSafeSender> sender,
                              int host_routing_id,
                              cc::SurfaceId id,
                              cc::SurfaceSequence sequence);
  static void RequireCallbackBrowserPlugin(
      scoped_refptr<ThreadSafeSender> sender,
      int host_routing_id,
      int browser_plugin_instance_id,
      cc::SurfaceId id,
      cc::SurfaceSequence sequence);

  int host_routing_id_;
  int last_route_id_;
  uint32 last_output_surface_id_;
  int last_host_id_;
  bool ack_pending_;
  bool opaque_;

  gfx::Size buffer_size_;

  // The lifetime of this weak pointer should be greater than the lifetime of
  // other member objects, as they may access this pointer during their
  // destruction.
  base::WeakPtr<BrowserPlugin> browser_plugin_;
  RenderFrameProxy* render_frame_proxy_;

  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection_;
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider_;

  // For cc::Surface support.
  scoped_refptr<cc::SurfaceLayer> surface_layer_;

  scoped_refptr<cc::SolidColorLayer> background_layer_;
  scoped_refptr<cc::DelegatedRendererLayer> delegated_layer_;
  scoped_ptr<blink::WebLayer> web_layer_;
  blink::WebFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(ChildFrameCompositingHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
