// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/worker_pool.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_linux_dma_buffer.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_osmesa.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace gfx {

namespace {

void WaitForFence(EGLDisplay display, EGLSyncKHR fence) {
  eglClientWaitSyncKHR(display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
                       EGL_FOREVER_KHR);
}

// A thin wrapper around GLSurfaceEGL that owns the EGLNativeWindow
class GL_EXPORT GLSurfaceOzoneEGL : public NativeViewGLSurfaceEGL {
 public:
  GLSurfaceOzoneEGL(scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface,
                    AcceleratedWidget widget);

  // GLSurface:
  bool Initialize() override;
  bool Resize(const gfx::Size& size) override;
  gfx::SwapResult SwapBuffers() override;
  bool ScheduleOverlayPlane(int z_order,
                            OverlayTransform transform,
                            GLImage* image,
                            const Rect& bounds_rect,
                            const RectF& crop_rect) override;

 private:
  using NativeViewGLSurfaceEGL::Initialize;

  ~GLSurfaceOzoneEGL() override;

  bool ReinitializeNativeSurface();

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface_;
  AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOzoneEGL);
};

GLSurfaceOzoneEGL::GLSurfaceOzoneEGL(
    scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface,
    AcceleratedWidget widget)
    : NativeViewGLSurfaceEGL(ozone_surface->GetNativeWindow()),
      ozone_surface_(ozone_surface.Pass()),
      widget_(widget) {
}

bool GLSurfaceOzoneEGL::Initialize() {
  return Initialize(ozone_surface_->CreateVSyncProvider());
}

bool GLSurfaceOzoneEGL::Resize(const gfx::Size& size) {
  if (!ozone_surface_->ResizeNativeWindow(size)) {
    if (!ReinitializeNativeSurface() ||
        !ozone_surface_->ResizeNativeWindow(size))
      return false;
  }

  return NativeViewGLSurfaceEGL::Resize(size);
}

gfx::SwapResult GLSurfaceOzoneEGL::SwapBuffers() {
  gfx::SwapResult result = NativeViewGLSurfaceEGL::SwapBuffers();
  if (result != gfx::SwapResult::SWAP_ACK)
    return result;

  return ozone_surface_->OnSwapBuffers() ? gfx::SwapResult::SWAP_ACK
                                         : gfx::SwapResult::SWAP_FAILED;
}

bool GLSurfaceOzoneEGL::ScheduleOverlayPlane(int z_order,
                                             OverlayTransform transform,
                                             GLImage* image,
                                             const Rect& bounds_rect,
                                             const RectF& crop_rect) {
  return image->ScheduleOverlayPlane(widget_, z_order, transform, bounds_rect,
                                     crop_rect);
}

GLSurfaceOzoneEGL::~GLSurfaceOzoneEGL() {
  Destroy();  // EGL surface must be destroyed before SurfaceOzone
}

bool GLSurfaceOzoneEGL::ReinitializeNativeSurface() {
  scoped_ptr<ui::ScopedMakeCurrent> scoped_make_current;
  GLContext* current_context = GLContext::GetCurrent();
  bool was_current = current_context && current_context->IsCurrent(this);
  if (was_current) {
    scoped_make_current.reset(new ui::ScopedMakeCurrent(current_context, this));
  }

  Destroy();
  ozone_surface_ = ui::OzonePlatform::GetInstance()
                       ->GetSurfaceFactoryOzone()
                       ->CreateEGLSurfaceForWidget(widget_)
                       .Pass();
  if (!ozone_surface_) {
    LOG(ERROR) << "Failed to create native surface.";
    return false;
  }

  window_ = ozone_surface_->GetNativeWindow();
  if (!Initialize()) {
    LOG(ERROR) << "Failed to initialize.";
    return false;
  }

  return true;
}

class GL_EXPORT GLSurfaceOzoneSurfaceless : public SurfacelessEGL {
 public:
  GLSurfaceOzoneSurfaceless(scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface,
                            AcceleratedWidget widget);

  // GLSurface:
  bool Initialize() override;
  bool Resize(const gfx::Size& size) override;
  gfx::SwapResult SwapBuffers() override;
  bool ScheduleOverlayPlane(int z_order,
                            OverlayTransform transform,
                            GLImage* image,
                            const Rect& bounds_rect,
                            const RectF& crop_rect) override;
  bool IsOffscreen() override;
  VSyncProvider* GetVSyncProvider() override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;
  bool SwapBuffersAsync(const SwapCompletionCallback& callback) override;
  bool PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          const SwapCompletionCallback& callback) override;

 protected:
  struct Overlay {
    Overlay(int z_order,
            OverlayTransform transform,
            GLImage* image,
            const Rect& bounds_rect,
            const RectF& crop_rect);

    bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget) const;

    int z_order;
    OverlayTransform transform;
    scoped_refptr<GLImage> image;
    Rect bounds_rect;
    RectF crop_rect;
  };

  struct PendingFrame {
    PendingFrame();

    bool ScheduleOverlayPlanes(gfx::AcceleratedWidget widget);

    bool ready;
    std::vector<Overlay> overlays;
    SwapCompletionCallback callback;
  };

  ~GLSurfaceOzoneSurfaceless() override;

  void SubmitFrame();

  EGLSyncKHR InsertFence();
  void FenceRetired(EGLSyncKHR fence, PendingFrame* frame);

  void SwapCompleted(const SwapCompletionCallback& callback,
                     gfx::SwapResult result);

  // The native surface. Deleting this is allowed to free the EGLNativeWindow.
  scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface_;
  AcceleratedWidget widget_;
  scoped_ptr<VSyncProvider> vsync_provider_;
  ScopedVector<PendingFrame> unsubmitted_frames_;
  bool has_implicit_external_sync_;
  bool last_swap_buffers_result_;
  bool swap_buffers_pending_;

  base::WeakPtrFactory<GLSurfaceOzoneSurfaceless> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOzoneSurfaceless);
};

GLSurfaceOzoneSurfaceless::Overlay::Overlay(int z_order,
                                            OverlayTransform transform,
                                            GLImage* image,
                                            const Rect& bounds_rect,
                                            const RectF& crop_rect)
    : z_order(z_order),
      transform(transform),
      image(image),
      bounds_rect(bounds_rect),
      crop_rect(crop_rect) {
}

bool GLSurfaceOzoneSurfaceless::Overlay::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget) const {
  return image->ScheduleOverlayPlane(widget, z_order, transform, bounds_rect,
                                     crop_rect);
}

GLSurfaceOzoneSurfaceless::PendingFrame::PendingFrame() : ready(false) {
}

bool GLSurfaceOzoneSurfaceless::PendingFrame::ScheduleOverlayPlanes(
    gfx::AcceleratedWidget widget) {
  for (const auto& overlay : overlays)
    if (!overlay.ScheduleOverlayPlane(widget))
      return false;
  return true;
}

GLSurfaceOzoneSurfaceless::GLSurfaceOzoneSurfaceless(
    scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface,
    AcceleratedWidget widget)
    : SurfacelessEGL(gfx::Size()),
      ozone_surface_(ozone_surface.Pass()),
      widget_(widget),
      has_implicit_external_sync_(
          HasEGLExtension("EGL_ARM_implicit_external_sync")),
      last_swap_buffers_result_(true),
      swap_buffers_pending_(false),
      weak_factory_(this) {
  unsubmitted_frames_.push_back(new PendingFrame());
}

bool GLSurfaceOzoneSurfaceless::Initialize() {
  if (!SurfacelessEGL::Initialize())
    return false;
  vsync_provider_ = ozone_surface_->CreateVSyncProvider();
  if (!vsync_provider_)
    return false;
  return true;
}
bool GLSurfaceOzoneSurfaceless::Resize(const gfx::Size& size) {
  if (!ozone_surface_->ResizeNativeWindow(size))
    return false;

  return SurfacelessEGL::Resize(size);
}
gfx::SwapResult GLSurfaceOzoneSurfaceless::SwapBuffers() {
  glFlush();
  // TODO: the following should be replaced by a per surface flush as it gets
  // implemented in GL drivers.
  if (has_implicit_external_sync_) {
    EGLSyncKHR fence = InsertFence();
    if (!fence)
      return SwapResult::SWAP_FAILED;

    EGLDisplay display = GetDisplay();
    WaitForFence(display, fence);
    eglDestroySyncKHR(display, fence);
  } else if (ozone_surface_->IsUniversalDisplayLinkDevice()) {
    glFinish();
  }

  unsubmitted_frames_.back()->ScheduleOverlayPlanes(widget_);
  unsubmitted_frames_.back()->overlays.clear();

  return ozone_surface_->OnSwapBuffers() ? gfx::SwapResult::SWAP_ACK
                                         : gfx::SwapResult::SWAP_FAILED;
}
bool GLSurfaceOzoneSurfaceless::ScheduleOverlayPlane(int z_order,
                                                     OverlayTransform transform,
                                                     GLImage* image,
                                                     const Rect& bounds_rect,
                                                     const RectF& crop_rect) {
  unsubmitted_frames_.back()->overlays.push_back(
      Overlay(z_order, transform, image, bounds_rect, crop_rect));
  return true;
}
bool GLSurfaceOzoneSurfaceless::IsOffscreen() {
  return false;
}
VSyncProvider* GLSurfaceOzoneSurfaceless::GetVSyncProvider() {
  return vsync_provider_.get();
}
bool GLSurfaceOzoneSurfaceless::SupportsPostSubBuffer() {
  return true;
}
gfx::SwapResult GLSurfaceOzoneSurfaceless::PostSubBuffer(int x,
                                                         int y,
                                                         int width,
                                                         int height) {
  // The actual sub buffer handling is handled at higher layers.
  SwapBuffers();
  return gfx::SwapResult::SWAP_ACK;
}
bool GLSurfaceOzoneSurfaceless::SwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  // If last swap failed, don't try to schedule new ones.
  if (!last_swap_buffers_result_)
    return false;

  glFlush();

  SwapCompletionCallback surface_swap_callback =
      base::Bind(&GLSurfaceOzoneSurfaceless::SwapCompleted,
                 weak_factory_.GetWeakPtr(), callback);

  PendingFrame* frame = unsubmitted_frames_.back();
  frame->callback = surface_swap_callback;
  unsubmitted_frames_.push_back(new PendingFrame());

  // TODO: the following should be replaced by a per surface flush as it gets
  // implemented in GL drivers.
  if (has_implicit_external_sync_) {
    EGLSyncKHR fence = InsertFence();
    if (!fence)
      return false;

    base::Closure fence_wait_task =
        base::Bind(&WaitForFence, GetDisplay(), fence);

    base::Closure fence_retired_callback =
        base::Bind(&GLSurfaceOzoneSurfaceless::FenceRetired,
                   weak_factory_.GetWeakPtr(), fence, frame);

    base::WorkerPool::PostTaskAndReply(FROM_HERE, fence_wait_task,
                                       fence_retired_callback, false);
    return true;
  } else if (ozone_surface_->IsUniversalDisplayLinkDevice()) {
    glFinish();
  }

  frame->ready = true;
  SubmitFrame();
  return last_swap_buffers_result_;
}
bool GLSurfaceOzoneSurfaceless::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    const SwapCompletionCallback& callback) {
  return SwapBuffersAsync(callback);
}

GLSurfaceOzoneSurfaceless::~GLSurfaceOzoneSurfaceless() {
  Destroy();  // EGL surface must be destroyed before SurfaceOzone
}

void GLSurfaceOzoneSurfaceless::SubmitFrame() {
  DCHECK(!unsubmitted_frames_.empty());

  if (unsubmitted_frames_.front()->ready && !swap_buffers_pending_) {
    scoped_ptr<PendingFrame> frame(unsubmitted_frames_.front());
    unsubmitted_frames_.weak_erase(unsubmitted_frames_.begin());
    swap_buffers_pending_ = true;

    last_swap_buffers_result_ =
        frame->ScheduleOverlayPlanes(widget_) &&
        ozone_surface_->OnSwapBuffersAsync(frame->callback);
  }
}

EGLSyncKHR GLSurfaceOzoneSurfaceless::InsertFence() {
  const EGLint attrib_list[] = {EGL_SYNC_CONDITION_KHR,
                                EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM,
                                EGL_NONE};
  return eglCreateSyncKHR(GetDisplay(), EGL_SYNC_FENCE_KHR, attrib_list);
}

void GLSurfaceOzoneSurfaceless::FenceRetired(EGLSyncKHR fence,
                                             PendingFrame* frame) {
  eglDestroySyncKHR(GetDisplay(), fence);
  frame->ready = true;
  SubmitFrame();
}

void GLSurfaceOzoneSurfaceless::SwapCompleted(
    const SwapCompletionCallback& callback,
    gfx::SwapResult result) {
  callback.Run(result);
  swap_buffers_pending_ = false;

  SubmitFrame();
}

// This provides surface-like semantics implemented through surfaceless.
// A framebuffer is bound automatically.
class GL_EXPORT GLSurfaceOzoneSurfacelessSurfaceImpl
    : public GLSurfaceOzoneSurfaceless {
 public:
  GLSurfaceOzoneSurfacelessSurfaceImpl(
      scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface,
      AcceleratedWidget widget);

  // GLSurface:
  unsigned int GetBackingFrameBufferObject() override;
  bool OnMakeCurrent(GLContext* context) override;
  bool Resize(const gfx::Size& size) override;
  bool SupportsPostSubBuffer() override;
  gfx::SwapResult SwapBuffers() override;
  bool SwapBuffersAsync(const SwapCompletionCallback& callback) override;
  void Destroy() override;

 private:
  class SurfaceImage : public GLImageLinuxDMABuffer {
   public:
    SurfaceImage(const gfx::Size& size, unsigned internalformat);

    bool Initialize(scoped_refptr<ui::NativePixmap> pixmap,
                    gfx::GpuMemoryBuffer::Format format);
    bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                              int z_order,
                              gfx::OverlayTransform transform,
                              const gfx::Rect& bounds_rect,
                              const gfx::RectF& crop_rect) override;

   private:
    ~SurfaceImage() override;

    scoped_refptr<ui::NativePixmap> pixmap_;
  };

  ~GLSurfaceOzoneSurfacelessSurfaceImpl() override;

  void BindFramebuffer();
  bool CreatePixmaps();

  GLuint fbo_;
  GLuint textures_[2];
  scoped_refptr<GLImage> images_[2];
  int current_surface_;
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceOzoneSurfacelessSurfaceImpl);
};

GLSurfaceOzoneSurfacelessSurfaceImpl::SurfaceImage::SurfaceImage(
    const gfx::Size& size,
    unsigned internalformat)
    : GLImageLinuxDMABuffer(size, internalformat) {
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::SurfaceImage::Initialize(
    scoped_refptr<ui::NativePixmap> pixmap,
    gfx::GpuMemoryBuffer::Format format) {
  base::FileDescriptor handle(pixmap->GetDmaBufFd(), false);
  if (!GLImageLinuxDMABuffer::Initialize(handle, format,
                                         pixmap->GetDmaBufPitch()))
    return false;
  pixmap_ = pixmap;
  return true;
}
bool GLSurfaceOzoneSurfacelessSurfaceImpl::SurfaceImage::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect) {
  return pixmap_->ScheduleOverlayPlane(widget, z_order, transform, bounds_rect,
                                       crop_rect);
}

GLSurfaceOzoneSurfacelessSurfaceImpl::SurfaceImage::~SurfaceImage() {
}

GLSurfaceOzoneSurfacelessSurfaceImpl::GLSurfaceOzoneSurfacelessSurfaceImpl(
    scoped_ptr<ui::SurfaceOzoneEGL> ozone_surface,
    AcceleratedWidget widget)
    : GLSurfaceOzoneSurfaceless(ozone_surface.Pass(), widget),
      fbo_(0),
      current_surface_(0) {
  for (auto& texture : textures_)
    texture = 0;
}

unsigned int
GLSurfaceOzoneSurfacelessSurfaceImpl::GetBackingFrameBufferObject() {
  return fbo_;
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::OnMakeCurrent(GLContext* context) {
  if (!fbo_) {
    glGenFramebuffersEXT(1, &fbo_);
    if (!fbo_)
      return false;
    glGenTextures(arraysize(textures_), textures_);
    if (!CreatePixmaps())
      return false;
  }
  BindFramebuffer();
  glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_);
  return SurfacelessEGL::OnMakeCurrent(context);
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::Resize(const gfx::Size& size) {
  if (size == GetSize())
    return true;
  return GLSurfaceOzoneSurfaceless::Resize(size) && CreatePixmaps();
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::SupportsPostSubBuffer() {
  return false;
}

gfx::SwapResult GLSurfaceOzoneSurfacelessSurfaceImpl::SwapBuffers() {
  if (!images_[current_surface_]->ScheduleOverlayPlane(
          widget_, 0, OverlayTransform::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(GetSize()), gfx::RectF(1, 1)))
    return gfx::SwapResult::SWAP_FAILED;
  gfx::SwapResult result = GLSurfaceOzoneSurfaceless::SwapBuffers();
  if (result != gfx::SwapResult::SWAP_ACK)
    return result;
  current_surface_ ^= 1;
  BindFramebuffer();
  return gfx::SwapResult::SWAP_ACK;
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::SwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  if (!images_[current_surface_]->ScheduleOverlayPlane(
          widget_, 0, OverlayTransform::OVERLAY_TRANSFORM_NONE,
          gfx::Rect(GetSize()), gfx::RectF(1, 1)))
    return false;
  if (!GLSurfaceOzoneSurfaceless::SwapBuffersAsync(callback))
    return false;
  current_surface_ ^= 1;
  BindFramebuffer();
  return true;
}

void GLSurfaceOzoneSurfacelessSurfaceImpl::Destroy() {
  GLContext* current_context = GLContext::GetCurrent();
  DCHECK(current_context && current_context->IsCurrent(this));
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  if (fbo_) {
    glDeleteTextures(arraysize(textures_), textures_);
    for (auto& texture : textures_)
      texture = 0;
    glDeleteFramebuffersEXT(1, &fbo_);
    fbo_ = 0;
  }
  for (auto image : images_) {
    if (image)
      image->Destroy(true);
  }
}

GLSurfaceOzoneSurfacelessSurfaceImpl::~GLSurfaceOzoneSurfacelessSurfaceImpl() {
  DCHECK(!fbo_);
  for (size_t i = 0; i < arraysize(textures_); i++)
    DCHECK(!textures_[i]) << "texture " << i << " not released";
}

void GLSurfaceOzoneSurfacelessSurfaceImpl::BindFramebuffer() {
  ScopedFrameBufferBinder fb(fbo_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            textures_[current_surface_], 0);
}

bool GLSurfaceOzoneSurfacelessSurfaceImpl::CreatePixmaps() {
  if (!fbo_)
    return true;
  for (size_t i = 0; i < arraysize(textures_); i++) {
    scoped_refptr<ui::NativePixmap> pixmap =
        ui::OzonePlatform::GetInstance()
            ->GetSurfaceFactoryOzone()
            ->CreateNativePixmap(widget_, GetSize(),
                                 ui::SurfaceFactoryOzone::BGRA_8888,
                                 ui::SurfaceFactoryOzone::SCANOUT);
    if (!pixmap)
      return false;
    scoped_refptr<SurfaceImage> image =
        new SurfaceImage(GetSize(), GL_BGRA_EXT);
    if (!image->Initialize(pixmap, gfx::GpuMemoryBuffer::Format::BGRA_8888))
      return false;
    images_[i] = image;
    // Bind image to texture.
    ScopedTextureBinder binder(GL_TEXTURE_2D, textures_[i]);
    if (!images_[i]->BindTexImage(GL_TEXTURE_2D))
      return false;
  }
  return true;
}

}  // namespace

// static
bool GLSurface::InitializeOneOffInternal() {
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
      if (!GLSurfaceEGL::InitializeOneOff()) {
        LOG(ERROR) << "GLSurfaceEGL::InitializeOneOff failed.";
        return false;
      }

      return true;
    case kGLImplementationOSMesaGL:
    case kGLImplementationMockGL:
      return true;
    default:
      return false;
  }
}

// static
scoped_refptr<GLSurface> GLSurface::CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
  if (GetGLImplementation() == kGLImplementationEGLGLES2 &&
      window != kNullAcceleratedWidget &&
      GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CanShowPrimaryPlaneAsOverlay()) {
    scoped_ptr<ui::SurfaceOzoneEGL> surface_ozone =
        ui::OzonePlatform::GetInstance()
            ->GetSurfaceFactoryOzone()
            ->CreateSurfacelessEGLSurfaceForWidget(window);
    if (!surface_ozone)
      return nullptr;
    scoped_refptr<GLSurface> surface;
    surface = new GLSurfaceOzoneSurfaceless(surface_ozone.Pass(), window);
    if (surface->Initialize())
      return surface;
  }

  return nullptr;
}

// static
scoped_refptr<GLSurface> GLSurface::CreateViewGLSurface(
    gfx::AcceleratedWidget window) {
  if (GetGLImplementation() == kGLImplementationOSMesaGL) {
    scoped_refptr<GLSurface> surface(new GLSurfaceOSMesaHeadless());
    if (!surface->Initialize())
      return NULL;
    return surface;
  }
  DCHECK(GetGLImplementation() == kGLImplementationEGLGLES2);
  if (window != kNullAcceleratedWidget) {
    scoped_refptr<GLSurface> surface;
    if (GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
        ui::OzonePlatform::GetInstance()
            ->GetSurfaceFactoryOzone()
            ->CanShowPrimaryPlaneAsOverlay()) {
      scoped_ptr<ui::SurfaceOzoneEGL> surface_ozone =
          ui::OzonePlatform::GetInstance()
              ->GetSurfaceFactoryOzone()
              ->CreateSurfacelessEGLSurfaceForWidget(window);
      if (!surface_ozone)
        return NULL;
      surface = new GLSurfaceOzoneSurfacelessSurfaceImpl(surface_ozone.Pass(),
                                                         window);
    } else {
      scoped_ptr<ui::SurfaceOzoneEGL> surface_ozone =
          ui::OzonePlatform::GetInstance()
              ->GetSurfaceFactoryOzone()
              ->CreateEGLSurfaceForWidget(window);
      if (!surface_ozone)
        return NULL;

      surface = new GLSurfaceOzoneEGL(surface_ozone.Pass(), window);
    }
    if (!surface->Initialize())
      return NULL;
    return surface;
  } else {
    scoped_refptr<GLSurface> surface = new GLSurfaceStub();
    if (surface->Initialize())
      return surface;
  }
  return NULL;
}

// static
scoped_refptr<GLSurface> GLSurface::CreateOffscreenGLSurface(
    const gfx::Size& size) {
  switch (GetGLImplementation()) {
    case kGLImplementationOSMesaGL: {
      scoped_refptr<GLSurface> surface(
          new GLSurfaceOSMesa(OSMesaSurfaceFormatBGRA, size));
      if (!surface->Initialize())
        return NULL;

      return surface;
    }
    case kGLImplementationEGLGLES2: {
      scoped_refptr<GLSurface> surface;
      if (GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
          (size.width() == 0 && size.height() == 0)) {
        surface = new SurfacelessEGL(size);
      } else
        surface = new PbufferGLSurfaceEGL(size);

      if (!surface->Initialize())
        return NULL;
      return surface;
    }
    case kGLImplementationMockGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

EGLNativeDisplayType GetPlatformDefaultEGLNativeDisplay() {
  return ui::OzonePlatform::GetInstance()
      ->GetSurfaceFactoryOzone()
      ->GetNativeDisplay();
}

}  // namespace gfx
