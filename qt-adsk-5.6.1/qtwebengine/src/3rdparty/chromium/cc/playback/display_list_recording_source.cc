// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_list_recording_source.h"

#include <algorithm>

#include "cc/base/histograms.h"
#include "cc/base/region.h"
#include "cc/layers/content_layer_client.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/display_list_raster_source.h"
#include "skia/ext/analysis_canvas.h"

namespace {

// Layout pixel buffer around the visible layer rect to record.  Any base
// picture that intersects the visible layer rect expanded by this distance
// will be recorded.
const int kPixelDistanceToRecord = 8000;
// We don't perform solid color analysis on images that have more than 10 skia
// operations.
const int kOpCountThatIsOkToAnalyze = 10;

// This is the distance, in layer space, by which the recorded viewport has to
// change before causing a paint of the new content. For example, it means
// that one has to scroll a very large page by 512 pixels before we will
// re-record a new DisplayItemList for an updated recorded viewport.
const int kMinimumDistanceBeforeUpdatingRecordedViewport = 512;

#ifdef NDEBUG
const bool kDefaultClearCanvasSetting = false;
#else
const bool kDefaultClearCanvasSetting = true;
#endif

DEFINE_SCOPED_UMA_HISTOGRAM_AREA_TIMER(
    ScopedDisplayListRecordingSourceUpdateTimer,
    "Compositing.DisplayListRecordingSource.UpdateUs",
    "Compositing.DisplayListRecordingSource.UpdateInvalidatedAreaPerMs");

}  // namespace

namespace cc {

DisplayListRecordingSource::DisplayListRecordingSource(
    const gfx::Size& grid_cell_size)
    : slow_down_raster_scale_factor_for_debug_(0),
      gather_pixel_refs_(false),
      requires_clear_(false),
      is_solid_color_(false),
      clear_canvas_with_debug_color_(kDefaultClearCanvasSetting),
      solid_color_(SK_ColorTRANSPARENT),
      background_color_(SK_ColorTRANSPARENT),
      pixel_record_distance_(kPixelDistanceToRecord),
      grid_cell_size_(grid_cell_size),
      is_suitable_for_gpu_rasterization_(true) {
}

DisplayListRecordingSource::~DisplayListRecordingSource() {
}

// This method only really makes sense to call if the size of the layer didn't
// change.
bool DisplayListRecordingSource::ExposesEnoughNewArea(
    const gfx::Rect& current_recorded_viewport,
    const gfx::Rect& potential_new_recorded_viewport,
    const gfx::Size& layer_size) {
  // If both are empty, nothing to do.
  if (current_recorded_viewport.IsEmpty() &&
      potential_new_recorded_viewport.IsEmpty())
    return false;

  // Re-record when going from empty to not-empty, to cover cases where
  // the layer is recorded for the first time, or otherwise becomes visible.
  if (current_recorded_viewport.IsEmpty())
    return true;

  // Re-record if the new viewport includes area outside of a skirt around the
  // existing viewport.
  gfx::Rect expanded_viewport(current_recorded_viewport);
  expanded_viewport.Inset(-kMinimumDistanceBeforeUpdatingRecordedViewport,
                          -kMinimumDistanceBeforeUpdatingRecordedViewport);
  if (!expanded_viewport.Contains(potential_new_recorded_viewport))
    return true;

  // Even if the new viewport doesn't include enough new area to satisfy the
  // condition above, re-record anyway if touches a layer edge not touched by
  // the existing viewport. Viewports are clipped to layer boundaries, so if the
  // new viewport touches a layer edge not touched by the existing viewport,
  // the new viewport must expose new area that touches this layer edge. Since
  // this new area touches a layer edge, it's impossible to expose more area in
  // that direction, so recording cannot be deferred until the exposed new area
  // satisfies the condition above.
  if (potential_new_recorded_viewport.x() == 0 &&
      current_recorded_viewport.x() != 0)
    return true;
  if (potential_new_recorded_viewport.y() == 0 &&
      current_recorded_viewport.y() != 0)
    return true;
  if (potential_new_recorded_viewport.right() == layer_size.width() &&
      current_recorded_viewport.right() != layer_size.width())
    return true;
  if (potential_new_recorded_viewport.bottom() == layer_size.height() &&
      current_recorded_viewport.bottom() != layer_size.height())
    return true;

  return false;
}

bool DisplayListRecordingSource::UpdateAndExpandInvalidation(
    ContentLayerClient* painter,
    Region* invalidation,
    const gfx::Size& layer_size,
    const gfx::Rect& visible_layer_rect,
    int frame_number,
    RecordingMode recording_mode) {
  ScopedDisplayListRecordingSourceUpdateTimer timer;
  bool updated = false;

  if (size_ != layer_size) {
    size_ = layer_size;
    updated = true;
  }

  // The recorded viewport is the visible layer rect, expanded
  // by the pixel record distance, up to a maximum of the total
  // layer size.
  gfx::Rect potential_new_recorded_viewport = visible_layer_rect;
  potential_new_recorded_viewport.Inset(-pixel_record_distance_,
                                        -pixel_record_distance_);
  potential_new_recorded_viewport.Intersect(gfx::Rect(GetSize()));

  if (updated ||
      ExposesEnoughNewArea(recorded_viewport_, potential_new_recorded_viewport,
                           GetSize())) {
    gfx::Rect old_recorded_viewport = recorded_viewport_;
    recorded_viewport_ = potential_new_recorded_viewport;

    // Invalidate newly-exposed and no-longer-exposed areas.
    Region newly_exposed_region(recorded_viewport_);
    newly_exposed_region.Subtract(old_recorded_viewport);
    invalidation->Union(newly_exposed_region);

    Region no_longer_exposed_region(old_recorded_viewport);
    no_longer_exposed_region.Subtract(recorded_viewport_);
    invalidation->Union(no_longer_exposed_region);

    updated = true;
  }

  // Count the area that is being invalidated.
  Region recorded_invalidation(*invalidation);
  recorded_invalidation.Intersect(recorded_viewport_);
  for (Region::Iterator it(recorded_invalidation); it.has_rect(); it.next())
    timer.AddArea(it.rect().size().GetArea());

  if (!updated && !invalidation->Intersects(recorded_viewport_))
    return false;

  ContentLayerClient::PaintingControlSetting painting_control =
      ContentLayerClient::PAINTING_BEHAVIOR_NORMAL;

  switch (recording_mode) {
    case RECORD_NORMALLY:
      // Already setup for normal recording.
      break;
    case RECORD_WITH_PAINTING_DISABLED:
      painting_control = ContentLayerClient::DISPLAY_LIST_PAINTING_DISABLED;
      break;
    case RECORD_WITH_CACHING_DISABLED:
      painting_control = ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED;
      break;
    case RECORD_WITH_CONSTRUCTION_DISABLED:
      painting_control = ContentLayerClient::DISPLAY_LIST_CONSTRUCTION_DISABLED;
      break;
    default:
      // case RecordingSource::RECORD_WITH_SK_NULL_CANVAS should not be reached
      NOTREACHED();
  }

  int repeat_count = 1;
  if (slow_down_raster_scale_factor_for_debug_ > 1) {
    repeat_count = slow_down_raster_scale_factor_for_debug_;
    painting_control = ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED;
  }

  for (int i = 0; i < repeat_count; ++i) {
    display_list_ = painter->PaintContentsToDisplayList(recorded_viewport_,
                                                        painting_control);
  }

  is_suitable_for_gpu_rasterization_ =
      display_list_->IsSuitableForGpuRasterization();
  DetermineIfSolidColor();
  display_list_->EmitTraceSnapshot();
  if (gather_pixel_refs_)
    display_list_->GatherPixelRefs(grid_cell_size_);

  return true;
}

gfx::Size DisplayListRecordingSource::GetSize() const {
  return size_;
}

void DisplayListRecordingSource::SetEmptyBounds() {
  size_ = gfx::Size();
  Clear();
}

void DisplayListRecordingSource::SetSlowdownRasterScaleFactor(int factor) {
  slow_down_raster_scale_factor_for_debug_ = factor;
}

void DisplayListRecordingSource::SetGatherPixelRefs(bool gather_pixel_refs) {
  gather_pixel_refs_ = gather_pixel_refs;
}

void DisplayListRecordingSource::SetBackgroundColor(SkColor background_color) {
  background_color_ = background_color;
}

void DisplayListRecordingSource::SetRequiresClear(bool requires_clear) {
  requires_clear_ = requires_clear;
}

void DisplayListRecordingSource::SetUnsuitableForGpuRasterizationForTesting() {
  is_suitable_for_gpu_rasterization_ = false;
}

bool DisplayListRecordingSource::IsSuitableForGpuRasterization() const {
  return is_suitable_for_gpu_rasterization_;
}

scoped_refptr<RasterSource> DisplayListRecordingSource::CreateRasterSource(
    bool can_use_lcd_text) const {
  return scoped_refptr<RasterSource>(
      DisplayListRasterSource::CreateFromDisplayListRecordingSource(
          this, can_use_lcd_text));
}

gfx::Size DisplayListRecordingSource::GetTileGridSizeForTesting() const {
  return gfx::Size();
}

void DisplayListRecordingSource::DetermineIfSolidColor() {
  DCHECK(display_list_.get());
  is_solid_color_ = false;
  solid_color_ = SK_ColorTRANSPARENT;

  if (display_list_->ApproximateOpCount() > kOpCountThatIsOkToAnalyze)
    return;

  gfx::Size layer_size = GetSize();
  skia::AnalysisCanvas canvas(layer_size.width(), layer_size.height());
  display_list_->Raster(&canvas, nullptr, gfx::Rect(), 1.f);
  is_solid_color_ = canvas.GetColorIfSolid(&solid_color_);
}

void DisplayListRecordingSource::Clear() {
  recorded_viewport_ = gfx::Rect();
  display_list_ = NULL;
  is_solid_color_ = false;
}

}  // namespace cc
