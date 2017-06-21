// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/surface_context_factory.h"

#include "cc/output/output_surface.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/view_manager/public/cpp/view.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "ui/compositor/reflector.h"
#include "ui/gl/gl_bindings.h"

namespace mandoline {
namespace {

class FakeReflector : public ui::Reflector {
 public:
  FakeReflector() {}
  ~FakeReflector() override {}
  void OnMirroringCompositorResized() override {}
  void AddMirroringLayer(ui::Layer* layer) override {}
  void RemoveMirroringLayer(ui::Layer* layer) override {}
};

}

SurfaceContextFactory::SurfaceContextFactory(mojo::Shell* shell,
                                             mojo::View* view)
    : surface_binding_(shell, view),
      next_surface_id_namespace_(1u) {
}

SurfaceContextFactory::~SurfaceContextFactory() {
}

void SurfaceContextFactory::CreateOutputSurface(
    base::WeakPtr<ui::Compositor> compositor) {
  NOTIMPLEMENTED();
  compositor->SetOutputSurface(surface_binding_.CreateOutputSurface());
}

scoped_ptr<ui::Reflector> SurfaceContextFactory::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  NOTIMPLEMENTED();
  return make_scoped_ptr(new FakeReflector);
}

void SurfaceContextFactory::RemoveReflector(ui::Reflector* reflector) {
  NOTIMPLEMENTED();
}

scoped_refptr<cc::ContextProvider>
SurfaceContextFactory::SharedMainThreadContextProvider() {
  NOTIMPLEMENTED();
  return nullptr;
}

void SurfaceContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  NOTIMPLEMENTED();
}

bool SurfaceContextFactory::DoesCreateTestContexts() {
  return false;
}

uint32 SurfaceContextFactory::GetImageTextureTarget(
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage) {
  // No GpuMemoryBuffer support, so just return GL_TEXTURE_2D.
  return GL_TEXTURE_2D;
}

cc::SharedBitmapManager* SurfaceContextFactory::GetSharedBitmapManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

gpu::GpuMemoryBufferManager*
SurfaceContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* SurfaceContextFactory::GetTaskGraphRunner() {
  return raster_thread_helper_.task_graph_runner();
}

scoped_ptr<cc::SurfaceIdAllocator>
SurfaceContextFactory::CreateSurfaceIdAllocator() {
  return make_scoped_ptr(
      new cc::SurfaceIdAllocator(next_surface_id_namespace_++));
}

void SurfaceContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                          const gfx::Size& size) {
  NOTIMPLEMENTED();
}

}  // namespace mandoline
