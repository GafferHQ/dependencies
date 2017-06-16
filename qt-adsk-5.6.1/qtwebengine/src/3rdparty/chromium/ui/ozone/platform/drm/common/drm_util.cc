// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_util.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drmMode.h>

#include "ui/display/util/edid_parser.h"

#if !defined(DRM_MODE_CONNECTOR_DSI)
#define DRM_MODE_CONNECTOR_DSI 16
#endif

namespace ui {

namespace {

bool IsCrtcInUse(uint32_t crtc,
                 const ScopedVector<HardwareDisplayControllerInfo>& displays) {
  for (size_t i = 0; i < displays.size(); ++i) {
    if (crtc == displays[i]->crtc()->crtc_id)
      return true;
  }

  return false;
}

uint32_t GetCrtc(int fd,
                 drmModeConnector* connector,
                 drmModeRes* resources,
                 const ScopedVector<HardwareDisplayControllerInfo>& displays) {
  // If the connector already has an encoder try to re-use.
  if (connector->encoder_id) {
    ScopedDrmEncoderPtr encoder(drmModeGetEncoder(fd, connector->encoder_id));
    if (encoder && encoder->crtc_id && !IsCrtcInUse(encoder->crtc_id, displays))
      return encoder->crtc_id;
  }

  // Try to find an encoder for the connector.
  for (int i = 0; i < connector->count_encoders; ++i) {
    ScopedDrmEncoderPtr encoder(drmModeGetEncoder(fd, connector->encoders[i]));
    if (!encoder)
      continue;

    for (int j = 0; j < resources->count_crtcs; ++j) {
      // Check if the encoder is compatible with this CRTC
      if (!(encoder->possible_crtcs & (1 << j)) ||
          IsCrtcInUse(resources->crtcs[j], displays))
        continue;

      return resources->crtcs[j];
    }
  }

  return 0;
}

// Computes the refresh rate for the specific mode. If we have enough
// information use the mode timings to compute a more exact value otherwise
// fallback to using the mode's vertical refresh rate (the kernel computes this
// the same way, however there is a loss in precision since |vrefresh| is sent
// as an integer).
float GetRefreshRate(const drmModeModeInfo& mode) {
  if (!mode.htotal || !mode.vtotal)
    return mode.vrefresh;

  float clock = mode.clock;
  float htotal = mode.htotal;
  float vtotal = mode.vtotal;

  return (clock * 1000.0f) / (htotal * vtotal);
}

DisplayConnectionType GetDisplayType(drmModeConnector* connector) {
  switch (connector->connector_type) {
    case DRM_MODE_CONNECTOR_VGA:
      return DISPLAY_CONNECTION_TYPE_VGA;
    case DRM_MODE_CONNECTOR_DVII:
    case DRM_MODE_CONNECTOR_DVID:
    case DRM_MODE_CONNECTOR_DVIA:
      return DISPLAY_CONNECTION_TYPE_DVI;
    case DRM_MODE_CONNECTOR_LVDS:
    case DRM_MODE_CONNECTOR_eDP:
    case DRM_MODE_CONNECTOR_DSI:
      return DISPLAY_CONNECTION_TYPE_INTERNAL;
    case DRM_MODE_CONNECTOR_DisplayPort:
      return DISPLAY_CONNECTION_TYPE_DISPLAYPORT;
    case DRM_MODE_CONNECTOR_HDMIA:
    case DRM_MODE_CONNECTOR_HDMIB:
      return DISPLAY_CONNECTION_TYPE_HDMI;
    default:
      return DISPLAY_CONNECTION_TYPE_UNKNOWN;
  }
}

int GetDrmProperty(int fd,
                   drmModeConnector* connector,
                   const std::string& name,
                   ScopedDrmPropertyPtr* property) {
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr tmp(drmModeGetProperty(fd, connector->props[i]));
    if (!tmp)
      continue;

    if (name == tmp->name) {
      *property = tmp.Pass();
      return i;
    }
  }

  return -1;
}

std::string GetNameForEnumValue(drmModePropertyRes* property, uint32_t value) {
  for (int i = 0; i < property->count_enums; ++i)
    if (property->enums[i].value == value)
      return property->enums[i].name;

  return std::string();
}

ScopedDrmPropertyBlobPtr GetDrmPropertyBlob(int fd,
                                            drmModeConnector* connector,
                                            const std::string& name) {
  ScopedDrmPropertyPtr property;
  int index = GetDrmProperty(fd, connector, name, &property);
  if (index < 0)
    return nullptr;

  if (property->flags & DRM_MODE_PROP_BLOB) {
    return ScopedDrmPropertyBlobPtr(
        drmModeGetPropertyBlob(fd, connector->prop_values[index]));
  }

  return nullptr;
}

bool IsAspectPreserving(int fd, drmModeConnector* connector) {
  ScopedDrmPropertyPtr property;
  int index = GetDrmProperty(fd, connector, "scaling mode", &property);
  if (index < 0)
    return false;

  return (GetNameForEnumValue(property.get(), connector->prop_values[index]) ==
          "Full aspect");
}

}  // namespace

HardwareDisplayControllerInfo::HardwareDisplayControllerInfo(
    ScopedDrmConnectorPtr connector,
    ScopedDrmCrtcPtr crtc)
    : connector_(connector.Pass()), crtc_(crtc.Pass()) {
}

HardwareDisplayControllerInfo::~HardwareDisplayControllerInfo() {
}

ScopedVector<HardwareDisplayControllerInfo> GetAvailableDisplayControllerInfos(
    int fd) {
  ScopedDrmResourcesPtr resources(drmModeGetResources(fd));
  DCHECK(resources) << "Failed to get DRM resources";
  ScopedVector<HardwareDisplayControllerInfo> displays;

  for (int i = 0; i < resources->count_connectors; ++i) {
    ScopedDrmConnectorPtr connector(
        drmModeGetConnector(fd, resources->connectors[i]));

    if (!connector || connector->connection != DRM_MODE_CONNECTED ||
        connector->count_modes == 0)
      continue;

    uint32_t crtc_id = GetCrtc(fd, connector.get(), resources.get(), displays);
    if (!crtc_id)
      continue;

    ScopedDrmCrtcPtr crtc(drmModeGetCrtc(fd, crtc_id));
    displays.push_back(
        new HardwareDisplayControllerInfo(connector.Pass(), crtc.Pass()));
  }

  return displays.Pass();
}

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs) {
  return lhs.clock == rhs.clock && lhs.hdisplay == rhs.hdisplay &&
         lhs.vdisplay == rhs.vdisplay && lhs.vrefresh == rhs.vrefresh &&
         lhs.hsync_start == rhs.hsync_start && lhs.hsync_end == rhs.hsync_end &&
         lhs.htotal == rhs.htotal && lhs.hskew == rhs.hskew &&
         lhs.vsync_start == rhs.vsync_start && lhs.vsync_end == rhs.vsync_end &&
         lhs.vtotal == rhs.vtotal && lhs.vscan == rhs.vscan &&
         lhs.flags == rhs.flags && strcmp(lhs.name, rhs.name) == 0;
}

DisplayMode_Params CreateDisplayModeParams(const drmModeModeInfo& mode) {
  DisplayMode_Params params;
  params.size = gfx::Size(mode.hdisplay, mode.vdisplay);
  params.is_interlaced = mode.flags & DRM_MODE_FLAG_INTERLACE;
  params.refresh_rate = GetRefreshRate(mode);

  return params;
}

DisplaySnapshot_Params CreateDisplaySnapshotParams(
    HardwareDisplayControllerInfo* info,
    int fd,
    size_t display_index,
    const gfx::Point& origin) {
  DisplaySnapshot_Params params;
  params.display_id = display_index;
  params.origin = origin;
  params.physical_size =
      gfx::Size(info->connector()->mmWidth, info->connector()->mmHeight);
  params.type = GetDisplayType(info->connector());
  params.is_aspect_preserving_scaling =
      IsAspectPreserving(fd, info->connector());

  ScopedDrmPropertyBlobPtr edid_blob(
      GetDrmPropertyBlob(fd, info->connector(), "EDID"));

  if (edid_blob) {
    std::vector<uint8_t> edid(
        static_cast<uint8_t*>(edid_blob->data),
        static_cast<uint8_t*>(edid_blob->data) + edid_blob->length);

    if (!GetDisplayIdFromEDID(edid, display_index, &params.display_id,
                              &params.product_id))
      params.display_id = display_index;

    ParseOutputDeviceData(edid, nullptr, nullptr, &params.display_name, nullptr,
                          nullptr);
    ParseOutputOverscanFlag(edid, &params.has_overscan);
  } else {
    VLOG(1) << "Failed to get EDID blob for connector "
            << info->connector()->connector_id;
  }

  for (int i = 0; i < info->connector()->count_modes; ++i) {
    const drmModeModeInfo& mode = info->connector()->modes[i];
    params.modes.push_back(CreateDisplayModeParams(mode));

    if (info->crtc()->mode_valid && SameMode(info->crtc()->mode, mode)) {
      params.has_current_mode = true;
      params.current_mode = params.modes.back();
    }

    if (mode.type & DRM_MODE_TYPE_PREFERRED) {
      params.has_native_mode = true;
      params.native_mode = params.modes.back();
    }
  }

  // If no preferred mode is found then use the first one. Using the first one
  // since it should be the best mode.
  if (!params.has_native_mode && !params.modes.empty()) {
    params.has_native_mode = true;
    params.native_mode = params.modes.front();
  }

  return params;
}

}  // namespace ui
