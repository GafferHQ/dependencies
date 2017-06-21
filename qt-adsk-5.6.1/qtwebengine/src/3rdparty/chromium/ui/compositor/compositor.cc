// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <algorithm>
#include <deque>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/switches.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/context_provider.h"
#include "cc/output/latency_info_swap_promise.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_switches.h"

namespace {

const double kDefaultRefreshRate = 60.0;
const double kTestRefreshRate = 200.0;

}  // namespace

namespace ui {

CompositorLock::CompositorLock(Compositor* compositor)
    : compositor_(compositor) {
  if (compositor_->locks_will_time_out_) {
    compositor_->task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CompositorLock::CancelLock, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(kCompositorLockTimeoutMs));
  }
}

CompositorLock::~CompositorLock() {
  CancelLock();
}

void CompositorLock::CancelLock() {
  if (!compositor_)
    return;
  compositor_->UnlockCompositor();
  compositor_ = NULL;
}

Compositor::Compositor(gfx::AcceleratedWidget widget,
                       ui::ContextFactory* context_factory,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : context_factory_(context_factory),
      root_layer_(NULL),
      widget_(widget),
      surface_id_allocator_(context_factory->CreateSurfaceIdAllocator()),
      task_runner_(task_runner),
      vsync_manager_(new CompositorVSyncManager()),
      device_scale_factor_(0.0f),
      last_started_frame_(0),
      last_ended_frame_(0),
      locks_will_time_out_(true),
      compositor_lock_(NULL),
      layer_animator_collection_(this),
      weak_ptr_factory_(this) {
  root_web_layer_ = cc::Layer::Create(Layer::UILayerSettings());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  cc::LayerTreeSettings settings;

  // When impl-side painting is enabled, this will ensure PictureLayers always
  // can have LCD text, to match the previous behaviour with ContentLayers,
  // where LCD-not-allowed notifications were ignored.
  settings.layers_always_allowed_lcd_text = true;
  settings.renderer_settings.refresh_rate =
      context_factory_->DoesCreateTestContexts() ? kTestRefreshRate
                                                 : kDefaultRefreshRate;
  settings.main_frame_before_activation_enabled = false;
  settings.renderer_settings.disable_gpu_vsync =
      command_line->HasSwitch(switches::kDisableGpuVsync);
  settings.renderer_settings.partial_swap_enabled =
      !command_line->HasSwitch(cc::switches::kUIDisablePartialSwap);
#if defined(OS_WIN)
  settings.renderer_settings.finish_rendering_on_resize = true;
#endif

  // These flags should be mirrored by renderer versions in content/renderer/.
  settings.initial_debug_state.show_debug_borders =
      command_line->HasSwitch(cc::switches::kUIShowCompositedLayerBorders);
  settings.initial_debug_state.show_fps_counter =
      command_line->HasSwitch(cc::switches::kUIShowFPSCounter);
  settings.initial_debug_state.show_layer_animation_bounds_rects =
      command_line->HasSwitch(cc::switches::kUIShowLayerAnimationBounds);
  settings.initial_debug_state.show_paint_rects =
      command_line->HasSwitch(switches::kUIShowPaintRects);
  settings.initial_debug_state.show_property_changed_rects =
      command_line->HasSwitch(cc::switches::kUIShowPropertyChangedRects);
  settings.initial_debug_state.show_surface_damage_rects =
      command_line->HasSwitch(cc::switches::kUIShowSurfaceDamageRects);
  settings.initial_debug_state.show_screen_space_rects =
      command_line->HasSwitch(cc::switches::kUIShowScreenSpaceRects);
  settings.initial_debug_state.show_replica_screen_space_rects =
      command_line->HasSwitch(cc::switches::kUIShowReplicaScreenSpaceRects);

  settings.initial_debug_state.SetRecordRenderingStats(
      command_line->HasSwitch(cc::switches::kEnableGpuBenchmarking));

  settings.use_display_lists = true;

  settings.use_zero_copy = IsUIZeroCopyEnabled();
  settings.use_one_copy = IsUIOneCopyEnabled();

  // TODO(reveman): We currently assume that the compositor will use BGRA_8888
  // if it's able to, and RGBA_8888 otherwise. Since we don't know what it will
  // use we hardcode BGRA_8888 here for now. We should instead
  // move decisions about GpuMemoryBuffer format to the browser embedder so we
  // know it here, and pass that decision to the compositor for each usage.
  // crbug.com/490362
  gfx::GpuMemoryBuffer::Format format = gfx::GpuMemoryBuffer::BGRA_8888;

  // Use PERSISTENT_MAP memory buffers to support partial tile raster for
  // software raster into GpuMemoryBuffers.
  gfx::GpuMemoryBuffer::Usage usage = gfx::GpuMemoryBuffer::PERSISTENT_MAP;
  settings.use_persistent_map_for_gpu_memory_buffers = true;

  settings.use_image_texture_target =
      context_factory_->GetImageTextureTarget(format, usage);

  // Note: gathering of pixel refs is only needed when using multiple
  // raster threads.
  settings.gather_pixel_refs = false;

  settings.use_compositor_animation_timelines =
      command_line->HasSwitch(switches::kUIEnableCompositorAnimationTimelines);

  base::TimeTicks before_create = base::TimeTicks::Now();

  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.shared_bitmap_manager = context_factory_->GetSharedBitmapManager();
  params.gpu_memory_buffer_manager =
      context_factory_->GetGpuMemoryBufferManager();
  params.task_graph_runner = context_factory_->GetTaskGraphRunner();
  params.settings = &settings;
  params.main_task_runner = task_runner_;
  host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
  UMA_HISTOGRAM_TIMES("GPU.CreateBrowserCompositor",
                      base::TimeTicks::Now() - before_create);
  host_->SetRootLayer(root_web_layer_);
  host_->set_surface_id_namespace(surface_id_allocator_->id_namespace());
  host_->SetLayerTreeHostClientReady();
}

Compositor::~Compositor() {
  TRACE_EVENT0("shutdown", "Compositor::destructor");

  CancelCompositorLock();
  DCHECK(!compositor_lock_);

  FOR_EACH_OBSERVER(CompositorObserver, observer_list_,
                    OnCompositingShuttingDown(this));

  FOR_EACH_OBSERVER(CompositorAnimationObserver, animation_observer_list_,
                    OnCompositingShuttingDown(this));

  DCHECK(begin_frame_observer_list_.empty());

  if (root_layer_)
    root_layer_->ResetCompositor();

  // Stop all outstanding draws before telling the ContextFactory to tear
  // down any contexts that the |host_| may rely upon.
  host_.reset();

  context_factory_->RemoveCompositor(this);
}

void Compositor::SetOutputSurface(
    scoped_ptr<cc::OutputSurface> output_surface) {
  host_->SetOutputSurface(output_surface.Pass());
}

void Compositor::ScheduleDraw() {
  host_->SetNeedsCommit();
}

void Compositor::SetRootLayer(Layer* root_layer) {
  if (root_layer_ == root_layer)
    return;
  if (root_layer_)
    root_layer_->ResetCompositor();
  root_layer_ = root_layer;
  root_web_layer_->RemoveAllChildren();
  if (root_layer_)
    root_layer_->SetCompositor(this, root_web_layer_);
}

void Compositor::SetHostHasTransparentBackground(
    bool host_has_transparent_background) {
  host_->set_has_transparent_background(host_has_transparent_background);
}

void Compositor::ScheduleFullRedraw() {
  // TODO(enne): Some callers (mac) call this function expecting that it
  // will also commit.  This should probably just redraw the screen
  // from damage and not commit.  ScheduleDraw/ScheduleRedraw need
  // better names.
  host_->SetNeedsRedraw();
  host_->SetNeedsCommit();
}

void Compositor::ScheduleRedrawRect(const gfx::Rect& damage_rect) {
  // TODO(enne): Make this not commit.  See ScheduleFullRedraw.
  host_->SetNeedsRedrawRect(damage_rect);
  host_->SetNeedsCommit();
}

void Compositor::FinishAllRendering() {
  host_->FinishAllRendering();
}

void Compositor::DisableSwapUntilResize() {
  host_->FinishAllRendering();
  context_factory_->ResizeDisplay(this, gfx::Size());
}

void Compositor::SetLatencyInfo(const ui::LatencyInfo& latency_info) {
  scoped_ptr<cc::SwapPromise> swap_promise(
      new cc::LatencyInfoSwapPromise(latency_info));
  host_->QueueSwapPromise(swap_promise.Pass());
}

void Compositor::SetScaleAndSize(float scale, const gfx::Size& size_in_pixel) {
  DCHECK_GT(scale, 0);
  if (!size_in_pixel.IsEmpty()) {
    size_ = size_in_pixel;
    host_->SetViewportSize(size_in_pixel);
    root_web_layer_->SetBounds(size_in_pixel);
    context_factory_->ResizeDisplay(this, size_in_pixel);
  }
  if (device_scale_factor_ != scale) {
    device_scale_factor_ = scale;
    host_->SetDeviceScaleFactor(scale);
    if (root_layer_)
      root_layer_->OnDeviceScaleFactorChanged(scale);
  }
}

void Compositor::SetBackgroundColor(SkColor color) {
  host_->set_background_color(color);
  ScheduleDraw();
}

void Compositor::SetVisible(bool visible) {
  host_->SetVisible(visible);
}

bool Compositor::IsVisible() {
  return host_->visible();
}

void Compositor::SetAuthoritativeVSyncInterval(
    const base::TimeDelta& interval) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
         cc::switches::kEnableBeginFrameScheduling)) {
    host_->SetAuthoritativeVSyncInterval(interval);
    return;
  }

  vsync_manager_->SetAuthoritativeVSyncInterval(interval);
}

scoped_refptr<CompositorVSyncManager> Compositor::vsync_manager() const {
  return vsync_manager_;
}

void Compositor::AddObserver(CompositorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void Compositor::RemoveObserver(CompositorObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool Compositor::HasObserver(const CompositorObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void Compositor::AddAnimationObserver(CompositorAnimationObserver* observer) {
  animation_observer_list_.AddObserver(observer);
  host_->SetNeedsAnimate();
}

void Compositor::RemoveAnimationObserver(
    CompositorAnimationObserver* observer) {
  animation_observer_list_.RemoveObserver(observer);
}

bool Compositor::HasAnimationObserver(
    const CompositorAnimationObserver* observer) const {
  return animation_observer_list_.HasObserver(observer);
}

void Compositor::AddBeginFrameObserver(CompositorBeginFrameObserver* observer) {
  DCHECK(std::find(begin_frame_observer_list_.begin(),
                   begin_frame_observer_list_.end(), observer) ==
         begin_frame_observer_list_.end());

  if (begin_frame_observer_list_.empty())
    host_->SetChildrenNeedBeginFrames(true);

  if (missed_begin_frame_args_.IsValid())
    observer->OnSendBeginFrame(missed_begin_frame_args_);

  begin_frame_observer_list_.push_back(observer);
}

void Compositor::RemoveBeginFrameObserver(
    CompositorBeginFrameObserver* observer) {
  auto it = std::find(begin_frame_observer_list_.begin(),
                      begin_frame_observer_list_.end(), observer);
  DCHECK(it != begin_frame_observer_list_.end());
  begin_frame_observer_list_.erase(it);

  if (begin_frame_observer_list_.empty()) {
    host_->SetChildrenNeedBeginFrames(false);
    missed_begin_frame_args_ = cc::BeginFrameArgs();
  }
}

void Compositor::BeginMainFrame(const cc::BeginFrameArgs& args) {
  FOR_EACH_OBSERVER(CompositorAnimationObserver,
                    animation_observer_list_,
                    OnAnimationStep(args.frame_time));
  if (animation_observer_list_.might_have_observers())
    host_->SetNeedsAnimate();
}

void Compositor::BeginMainFrameNotExpectedSoon() {
}

static void SendDamagedRectsRecursive(ui::Layer* layer) {
  layer->SendDamagedRects();
  for (auto* child : layer->children())
    SendDamagedRectsRecursive(child);
}

void Compositor::Layout() {
  if (!root_layer())
    return;
  SendDamagedRectsRecursive(root_layer());
}

void Compositor::RequestNewOutputSurface() {
  context_factory_->CreateOutputSurface(weak_ptr_factory_.GetWeakPtr());
}

void Compositor::DidInitializeOutputSurface() {
}

void Compositor::DidFailToInitializeOutputSurface() {
  // The OutputSurface should already be bound/initialized before being given to
  // the Compositor.
  NOTREACHED();
}

void Compositor::DidCommit() {
  DCHECK(!IsLocked());
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingDidCommit(this));
}

void Compositor::DidCommitAndDrawFrame() {
}

void Compositor::DidCompleteSwapBuffers() {
  FOR_EACH_OBSERVER(CompositorObserver, observer_list_,
                    OnCompositingEnded(this));
}

void Compositor::DidPostSwapBuffers() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  FOR_EACH_OBSERVER(CompositorObserver, observer_list_,
                    OnCompositingStarted(this, start_time));
}

void Compositor::DidAbortSwapBuffers() {
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingAborted(this));
}

void Compositor::SendBeginFramesToChildren(const cc::BeginFrameArgs& args) {
  for (auto observer : begin_frame_observer_list_)
    observer->OnSendBeginFrame(args);

  missed_begin_frame_args_ = args;
  missed_begin_frame_args_.type = cc::BeginFrameArgs::MISSED;
}

const cc::LayerTreeDebugState& Compositor::GetLayerTreeDebugState() const {
  return host_->debug_state();
}

void Compositor::SetLayerTreeDebugState(
    const cc::LayerTreeDebugState& debug_state) {
  host_->SetDebugState(debug_state);
}

const cc::RendererSettings& Compositor::GetRendererSettings() const {
  return host_->settings().renderer_settings;
}

scoped_refptr<CompositorLock> Compositor::GetCompositorLock() {
  if (!compositor_lock_) {
    compositor_lock_ = new CompositorLock(this);
    host_->SetDeferCommits(true);
    FOR_EACH_OBSERVER(CompositorObserver,
                      observer_list_,
                      OnCompositingLockStateChanged(this));
  }
  return compositor_lock_;
}

void Compositor::UnlockCompositor() {
  DCHECK(compositor_lock_);
  compositor_lock_ = NULL;
  host_->SetDeferCommits(false);
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingLockStateChanged(this));
}

void Compositor::CancelCompositorLock() {
  if (compositor_lock_)
    compositor_lock_->CancelLock();
}

}  // namespace ui
