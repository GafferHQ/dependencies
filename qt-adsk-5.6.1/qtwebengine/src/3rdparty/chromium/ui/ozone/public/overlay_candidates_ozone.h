// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OVERLAY_CANDIDATES_OZONE_H_
#define UI_OZONE_PUBLIC_OVERLAY_CANDIDATES_OZONE_H_

#include <vector>

#include "base/basictypes.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/ozone/ozone_base_export.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

// This class can be used to answer questions about possible overlay
// configurations for a particular output device. We get an instance of this
// class from SurfaceFactoryOzone given an AcceleratedWidget.
class OZONE_BASE_EXPORT OverlayCandidatesOzone {
 public:
  struct OverlaySurfaceCandidate {
    OverlaySurfaceCandidate();
    ~OverlaySurfaceCandidate();

    // Transformation to apply to layer during composition.
    gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_NONE;
    // Format of the buffer to composite.
    SurfaceFactoryOzone::BufferFormat format = SurfaceFactoryOzone::UNKNOWN;
    // Size of the buffer, in pixels.
    gfx::Size buffer_size;
    // Rect on the display to position the overlay to. Input rectangle may
    // not have integer coordinates, but when accepting for overlay, must
    // be modified by CheckOverlaySupport to output integer values.
    gfx::RectF display_rect;
    // Crop within the buffer to be placed inside |display_rect|.
    gfx::RectF crop_rect;
    // Stacking order of the overlay plane relative to the main surface,
    // which is 0. Signed to allow for "underlays".
    int plane_z_order = 0;

    // To be modified by the implementer if this candidate can go into
    // an overlay.
    bool overlay_handled = false;
  };

  typedef std::vector<OverlaySurfaceCandidate> OverlaySurfaceCandidateList;

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited. When setting |overlay_handled| to true,
  // the implementation must also snap |display_rect| to integer coordinates
  // if necessary.
  virtual void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces);

  virtual ~OverlayCandidatesOzone();
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OVERLAY_CANDIDATES_OZONE_H_
