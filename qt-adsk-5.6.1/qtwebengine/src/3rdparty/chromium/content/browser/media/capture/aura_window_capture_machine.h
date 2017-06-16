// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_AURA_WINDOW_CAPTURE_MACHINE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_AURA_WINDOW_CAPTURE_MACHINE_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "media/capture/screen_capture_device_core.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/compositor/compositor.h"

namespace cc {

class CopyOutputResult;

}  // namespace cc

namespace content {

class PowerSaveBlocker;
class ReadbackYUVInterface;

class AuraWindowCaptureMachine
    : public media::VideoCaptureMachine,
      public aura::WindowObserver,
      public ui::CompositorObserver,
      public base::SupportsWeakPtr<AuraWindowCaptureMachine> {
 public:
  AuraWindowCaptureMachine();
  ~AuraWindowCaptureMachine() override;

  // VideoCaptureMachine overrides.
  void Start(const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
             const media::VideoCaptureParams& params,
             const base::Callback<void(bool)> callback) override;
  void Stop(const base::Closure& callback) override;

  // Implements aura::WindowObserver.
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

  // Implements ui::CompositorObserver.
  void OnCompositingDidCommit(ui::Compositor* compositor) override {}
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override {}
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingAborted(ui::Compositor* compositor) override {}
  void OnCompositingLockStateChanged(ui::Compositor* compositor) override {}
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {}

  // Sets the window to use for capture.
  void SetWindow(aura::Window* window);

 private:
  bool InternalStart(
      const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
      const media::VideoCaptureParams& params);
  void InternalStop(const base::Closure& callback);

  // Captures a frame.
  // |dirty| is false for timer polls and true for compositor updates.
  void Capture(bool dirty);

  // Update capture size. Must be called on the UI thread.
  void UpdateCaptureSize();

  using CaptureFrameCallback =
      media::ThreadSafeCaptureOracle::CaptureFrameCallback;

  // Response callback for cc::Layer::RequestCopyOfOutput().
  void DidCopyOutput(
      scoped_refptr<media::VideoFrame> video_frame,
      base::TimeTicks start_time,
      const CaptureFrameCallback& capture_frame_cb,
      scoped_ptr<cc::CopyOutputResult> result);

  // A helper which does the real work for DidCopyOutput. Returns true if
  // succeeded.
  bool ProcessCopyOutputResponse(
      scoped_refptr<media::VideoFrame> video_frame,
      base::TimeTicks start_time,
      const CaptureFrameCallback& capture_frame_cb,
      scoped_ptr<cc::CopyOutputResult> result);

  // Helper function to update cursor state.
  // |region_in_frame| defines where the desktop is rendered in the captured
  // frame.
  // Returns the current cursor position in captured frame.
  gfx::Point UpdateCursorState(const gfx::Rect& region_in_frame);

  // Clears cursor state.
  void ClearCursorState();

  // The window associated with the desktop.
  aura::Window* desktop_window_;

  // The timer that kicks off period captures.
  base::Timer timer_;

  // Whether screen capturing or window capture.
  bool screen_capture_;

  // Makes all the decisions about which frames to copy, and how.
  scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy_;

  // The capture parameters for this capture.
  media::VideoCaptureParams capture_params_;

  // YUV readback pipeline.
  scoped_ptr<content::ReadbackYUVInterface> yuv_readback_pipeline_;

  // Cursor state.
  ui::Cursor last_cursor_;
  gfx::Size desktop_size_when_cursor_last_updated_;
  gfx::Point cursor_hot_point_;
  SkBitmap scaled_cursor_bitmap_;

  // TODO(jiayl): Remove power_save_blocker_ when there is an API to keep the
  // screen from sleeping for the drive-by web.
  scoped_ptr<PowerSaveBlocker> power_save_blocker_;

  DISALLOW_COPY_AND_ASSIGN(AuraWindowCaptureMachine);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_AURA_WINDOW_CAPTURE_MACHINE_H_
