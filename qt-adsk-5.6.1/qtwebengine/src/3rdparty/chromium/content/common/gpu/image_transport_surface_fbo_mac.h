// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_FBO_MAC_H_
#define CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_FBO_MAC_H_

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/image_transport_surface.h"
#include "ui/gl/gl_bindings.h"

namespace content {

// We are backed by an offscreen surface for the purposes of creating
// a context, but use FBOs to render to texture. The texture may be backed by
// an IOSurface, or it may be presented to the screen via a CALayer, depending
// on the StorageProvider class specified.
class ImageTransportSurfaceFBO
    : public gfx::GLSurface,
      public ImageTransportSurface,
      public GpuCommandBufferStub::DestructionObserver {
 public:
  // The interface through which storage for the color buffer of the FBO is
  // allocated.
  class StorageProvider {
   public:
    virtual ~StorageProvider() {}
    // IOSurfaces cause too much address space fragmentation if they are
    // allocated on every resize. This gets a rounded size for allocation.
    virtual gfx::Size GetRoundedSize(gfx::Size size) = 0;

    // Allocate the storage for the color buffer. The specified context is
    // current, and there is a texture bound to GL_TEXTURE_RECTANGLE_ARB.
    virtual bool AllocateColorBufferStorage(
        CGLContextObj context, const base::Closure& context_dirtied_callback,
        GLuint texture, gfx::Size size, float scale_factor) = 0;

    // Free the storage allocated in the AllocateColorBufferStorage call. The
    // GL texture that was bound has already been deleted by the caller.
    virtual void FreeColorBufferStorage() = 0;

    // Called when the frame size has changed (the buffer may not have been
    // reallocated, since its size may be rounded).
    virtual void FrameSizeChanged(
        const gfx::Size& pixel_size, float scale_factor) = 0;

    // Swap buffers, or post sub-buffer.
    virtual void SwapBuffers(const gfx::Rect& dirty_rect) = 0;

    // Indicate that the backbuffer will be written to.
    virtual void WillWriteToBackbuffer() = 0;

    // Indicate that the backbuffer has been discarded and should not be seen
    // again.
    virtual void DiscardBackbuffer() = 0;

    // Called once for every SwapBuffers call when the IPC for the present has
    // been processed by the browser. |disable_throttling| is set if the
    // browser suspects that GPU back-pressure should be disabled.
    virtual void SwapBuffersAckedByBrowser(bool disable_throttling) = 0;
  };

  ImageTransportSurfaceFBO(GpuChannelManager* manager,
                           GpuCommandBufferStub* stub,
                           gfx::PluginWindowHandle handle);

  // GLSurface implementation
  bool Initialize() override;
  void Destroy() override;
  bool DeferDraws() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  bool SupportsPostSubBuffer() override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  void* GetDisplay() override;
  bool OnMakeCurrent(gfx::GLContext* context) override;
  void NotifyWasBound() override;
  unsigned int GetBackingFrameBufferObject() override;
  bool SetBackbufferAllocation(bool allocated) override;
  void SetFrontbufferAllocation(bool allocated) override;

  // Called when the context may continue to make forward progress after a swap.
  void SendSwapBuffers(uint64 surface_handle,
                       const gfx::Size pixel_size,
                       float scale_factor);
  void SetRendererID(int renderer_id);

  const gpu::gles2::FeatureInfo* GetFeatureInfo() const;

 protected:
  // ImageTransportSurface implementation
  void OnBufferPresented(
      const AcceleratedSurfaceMsg_BufferPresented_Params& params) override;
  void OnResize(gfx::Size pixel_size, float scale_factor) override;
  void SetLatencyInfo(const std::vector<ui::LatencyInfo>&) override;
  void WakeUpGpu() override;

  // GpuCommandBufferStub::DestructionObserver implementation.
  void OnWillDestroyStub() override;

 private:
  ~ImageTransportSurfaceFBO() override;

  void AdjustBufferAllocation();
  void DestroyFramebuffer();
  void AllocateOrResizeFramebuffer(
      const gfx::Size& pixel_size, float scale_factor);
  bool SwapBuffersInternal(const gfx::Rect& dirty_rect);

  scoped_ptr<StorageProvider> storage_provider_;

  // Tracks the current buffer allocation state.
  bool backbuffer_suggested_allocation_;
  bool frontbuffer_suggested_allocation_;

  uint32 fbo_id_;
  GLuint texture_id_;
  GLuint depth_stencil_renderbuffer_id_;
  bool has_complete_framebuffer_;

  // Weak pointer to the context that this was last made current to.
  gfx::GLContext* context_;

  gfx::Size pixel_size_;
  gfx::Size rounded_pixel_size_;
  float scale_factor_;

  // Whether or not we've successfully made the surface current once.
  bool made_current_;

  // Whether a SwapBuffers IPC needs to be sent to the browser.
  bool is_swap_buffers_send_pending_;
  std::vector<ui::LatencyInfo> latency_info_;
  gfx::Rect pending_swap_pixel_damage_rect_;

  scoped_ptr<ImageTransportHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(ImageTransportSurfaceFBO);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_IMAGE_TRANSPORT_SURFACE_FBO_MAC_H_
