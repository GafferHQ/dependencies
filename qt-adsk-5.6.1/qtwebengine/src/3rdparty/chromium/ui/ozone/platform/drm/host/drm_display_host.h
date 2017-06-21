// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_

#include "base/memory/scoped_ptr.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/platform/drm/host/channel_observer.h"

namespace ui {

struct DisplaySnapshot_Params;
class DisplaySnapshot;
class DrmGpuPlatformSupportHost;

class DrmDisplayHost : public ChannelObserver {
 public:
  DrmDisplayHost(DrmGpuPlatformSupportHost* sender,
                 const DisplaySnapshot_Params& params,
                 bool is_dummy);
  ~DrmDisplayHost() override;

  DisplaySnapshot* snapshot() const { return snapshot_.get(); }

  void UpdateDisplaySnapshot(const DisplaySnapshot_Params& params);
  void Configure(const DisplayMode* mode,
                 const gfx::Point& origin,
                 const ConfigureCallback& callback);
  void GetHDCPState(const GetHDCPStateCallback& callback);
  void SetHDCPState(HDCPState state, const SetHDCPStateCallback& callback);
  void SetGammaRamp(const std::vector<GammaRampRGBEntry>& lut);

  // Called when the IPC from the GPU process arrives to answer the above
  // commands.
  void OnDisplayConfigured(bool status);
  void OnHDCPStateReceived(bool status, HDCPState state);
  void OnHDCPStateUpdated(bool status);

  // ChannelObserver:
  void OnChannelEstablished() override;
  void OnChannelDestroyed() override;

 private:
  // Calls all the callbacks with failure.
  void ClearCallbacks();

  DrmGpuPlatformSupportHost* sender_;  // Not owned.

  scoped_ptr<DisplaySnapshot> snapshot_;

  // Used during startup to signify that any display configuration should be
  // synchronous and succeed.
  bool is_dummy_;

  ConfigureCallback configure_callback_;
  GetHDCPStateCallback get_hdcp_callback_;
  SetHDCPStateCallback set_hdcp_callback_;

  DISALLOW_COPY_AND_ASSIGN(DrmDisplayHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DISPLAY_HOST_H_
