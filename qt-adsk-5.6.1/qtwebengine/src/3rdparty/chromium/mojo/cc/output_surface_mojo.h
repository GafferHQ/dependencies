// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CC_OUTPUT_SURFACE_MOJO_H_
#define MOJO_CC_OUTPUT_SURFACE_MOJO_H_

#include "base/macros.h"
#include "cc/output/output_surface.h"
#include "cc/surfaces/surface_id.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"

namespace mojo {

class OutputSurfaceMojoClient {
 public:
  virtual ~OutputSurfaceMojoClient() {}

  virtual void DidCreateSurface(cc::SurfaceId id) = 0;
};

class OutputSurfaceMojo : public cc::OutputSurface {
 public:
  OutputSurfaceMojo(OutputSurfaceMojoClient* client,
                    const scoped_refptr<cc::ContextProvider>& context_provider,
                    SurfacePtr surface);

  // cc::OutputSurface implementation.
  void SwapBuffers(cc::CompositorFrame* frame) override;
  bool BindToClient(cc::OutputSurfaceClient* client) override;

 protected:
  ~OutputSurfaceMojo() override;

 private:
  void SetIdNamespace(uint32_t id_namespace);

  OutputSurfaceMojoClient* output_surface_mojo_client_;
  SurfacePtr surface_;
  uint32_t id_namespace_;
  uint32_t local_id_;
  gfx::Size surface_size_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurfaceMojo);
};
}

#endif  // MOJO_CC_OUTPUT_SURFACE_MOJO_H_
