// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_surface.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/platform/drm/test/mock_drm_device.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;
const uint32_t kDefaultCrtc = 1;
const uint32_t kDefaultConnector = 2;
const size_t kPlanesPerCrtc = 1;
const uint32_t kDefaultCursorSize = 64;

}  // namespace

class DrmSurfaceTest : public testing::Test {
 public:
  DrmSurfaceTest() {}

  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<ui::MockDrmDevice> drm_;
  scoped_ptr<ui::DrmBufferGenerator> buffer_generator_;
  scoped_ptr<ui::ScreenManager> screen_manager_;
  scoped_ptr<ui::DrmDeviceManager> drm_device_manager_;
  scoped_ptr<ui::DrmSurface> surface_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DrmSurfaceTest);
};

void DrmSurfaceTest::SetUp() {
  message_loop_.reset(new base::MessageLoopForUI);
  std::vector<uint32_t> crtcs;
  crtcs.push_back(kDefaultCrtc);
  drm_ = new ui::MockDrmDevice(false, crtcs, kPlanesPerCrtc);
  buffer_generator_.reset(new ui::DrmBufferGenerator());
  screen_manager_.reset(new ui::ScreenManager(buffer_generator_.get()));
  screen_manager_->AddDisplayController(drm_, kDefaultCrtc, kDefaultConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kDefaultCrtc, kDefaultConnector, gfx::Point(), kDefaultMode);

  drm_device_manager_.reset(new ui::DrmDeviceManager(nullptr));
  scoped_ptr<ui::DrmWindow> window(new ui::DrmWindow(
      kDefaultWidgetHandle, drm_device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->OnBoundsChanged(
      gfx::Rect(gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay)));
  screen_manager_->AddWindow(kDefaultWidgetHandle, window.Pass());

  surface_.reset(
      new ui::DrmSurface(screen_manager_->GetWindow(kDefaultWidgetHandle)));
  surface_->ResizeCanvas(
      gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay));
}

void DrmSurfaceTest::TearDown() {
  surface_.reset();
  scoped_ptr<ui::DrmWindow> window =
      screen_manager_->RemoveWindow(kDefaultWidgetHandle);
  window->Shutdown();
  drm_ = nullptr;
  message_loop_.reset();
}

TEST_F(DrmSurfaceTest, CheckFBIDOnSwap) {
  surface_->PresentCanvas(gfx::Rect());
  drm_->RunCallbacks();
  // Framebuffer ID 1 is allocated in SetUp for the buffer used to modeset.
  EXPECT_EQ(3u, drm_->current_framebuffer());
  surface_->PresentCanvas(gfx::Rect());
  drm_->RunCallbacks();
  EXPECT_EQ(2u, drm_->current_framebuffer());
}

TEST_F(DrmSurfaceTest, CheckSurfaceContents) {
  SkPaint paint;
  paint.setColor(SK_ColorWHITE);
  SkRect rect =
      SkRect::MakeWH(kDefaultMode.hdisplay / 2, kDefaultMode.vdisplay / 2);
  surface_->GetSurface()->getCanvas()->drawRect(rect, paint);
  surface_->PresentCanvas(
      gfx::Rect(0, 0, kDefaultMode.hdisplay / 2, kDefaultMode.vdisplay / 2));
  drm_->RunCallbacks();

  SkBitmap image;
  std::vector<skia::RefPtr<SkSurface>> framebuffers;
  for (const auto& buffer : drm_->buffers()) {
    // Skip cursor buffers.
    if (buffer->width() == kDefaultCursorSize &&
        buffer->height() == kDefaultCursorSize)
      continue;

    framebuffers.push_back(buffer);
  }

  // Buffer 0 is the modesetting buffer, buffer 1 is the frontbuffer and buffer
  // 2 is the backbuffer.
  EXPECT_EQ(3u, framebuffers.size());

  image.setInfo(framebuffers[2]->getCanvas()->imageInfo());
  EXPECT_TRUE(framebuffers[2]->getCanvas()->readPixels(&image, 0, 0));

  EXPECT_EQ(kDefaultMode.hdisplay, image.width());
  EXPECT_EQ(kDefaultMode.vdisplay, image.height());

  // Make sure the updates are correctly propagated to the native surface.
  for (int i = 0; i < image.height(); ++i) {
    for (int j = 0; j < image.width(); ++j) {
      if (j < kDefaultMode.hdisplay / 2 && i < kDefaultMode.vdisplay / 2)
        EXPECT_EQ(SK_ColorWHITE, image.getColor(j, i));
      else
        EXPECT_EQ(SK_ColorBLACK, image.getColor(j, i));
    }
  }
}

TEST_F(DrmSurfaceTest, CheckSurfaceContentsAfter2QueuedPresents) {
  gfx::Rect rect;
  // Present an empty buffer but don't respond with the page flip event since we
  // want to make sure the following presents will aggregate correctly.
  surface_->PresentCanvas(rect);

  SkPaint paint;
  paint.setColor(SK_ColorWHITE);
  rect.SetRect(0, 0, kDefaultMode.hdisplay / 2, kDefaultMode.vdisplay / 2);
  surface_->GetSurface()->getCanvas()->drawRect(RectToSkRect(rect), paint);
  surface_->PresentCanvas(rect);

  paint.setColor(SK_ColorRED);
  rect.SetRect(0, kDefaultMode.vdisplay / 2, kDefaultMode.hdisplay / 2,
               kDefaultMode.vdisplay / 2);
  surface_->GetSurface()->getCanvas()->drawRect(RectToSkRect(rect), paint);
  surface_->PresentCanvas(rect);

  drm_->RunCallbacks();

  SkBitmap image;
  std::vector<skia::RefPtr<SkSurface>> framebuffers;
  for (const auto& buffer : drm_->buffers()) {
    // Skip cursor buffers.
    if (buffer->width() == kDefaultCursorSize &&
        buffer->height() == kDefaultCursorSize)
      continue;

    framebuffers.push_back(buffer);
  }

  // Buffer 0 is the modesetting buffer, buffer 1 is the backbuffer and buffer
  // 2 is the frontbuffer.
  EXPECT_EQ(3u, framebuffers.size());

  image.setInfo(framebuffers[1]->getCanvas()->imageInfo());
  EXPECT_TRUE(framebuffers[1]->getCanvas()->readPixels(&image, 0, 0));

  EXPECT_EQ(kDefaultMode.hdisplay, image.width());
  EXPECT_EQ(kDefaultMode.vdisplay, image.height());

  // Make sure the updates are correctly propagated to the native surface.
  for (int i = 0; i < image.height(); ++i) {
    for (int j = 0; j < image.width(); ++j) {
      if (j < kDefaultMode.hdisplay / 2 && i < kDefaultMode.vdisplay / 2)
        EXPECT_EQ(SK_ColorWHITE, image.getColor(j, i));
      else if (j < kDefaultMode.hdisplay / 2)
        EXPECT_EQ(SK_ColorRED, image.getColor(j, i));
      else
        EXPECT_EQ(SK_ColorBLACK, image.getColor(j, i));
    }
  }
}
