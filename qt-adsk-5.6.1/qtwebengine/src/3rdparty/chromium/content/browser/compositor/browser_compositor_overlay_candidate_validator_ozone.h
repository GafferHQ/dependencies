// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_
#define CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/compositor/browser_compositor_overlay_candidate_validator.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class OverlayCandidatesOzone;
}

namespace content {

class CONTENT_EXPORT BrowserCompositorOverlayCandidateValidatorOzone
    : public BrowserCompositorOverlayCandidateValidator {
 public:
  BrowserCompositorOverlayCandidateValidatorOzone(
      gfx::AcceleratedWidget widget,
      scoped_ptr<ui::OverlayCandidatesOzone> overlay_candidates);
  ~BrowserCompositorOverlayCandidateValidatorOzone() override;

  // cc::OverlayCandidateValidator implementation.
  void CheckOverlaySupport(cc::OverlayCandidateList* surfaces) override;

  // BrowserCompositorOverlayCandidateValidator implementation.
  void SetSoftwareMirrorMode(bool enabled) override;

 private:
  gfx::AcceleratedWidget widget_;
  scoped_ptr<ui::OverlayCandidatesOzone> overlay_candidates_;
  bool software_mirror_active_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOverlayCandidateValidatorOzone);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OVERLAY_CANDIDATE_VALIDATOR_OZONE_H_
