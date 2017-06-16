// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DRAW_PROPERTIES_H_
#define CC_LAYERS_DRAW_PROPERTIES_H_

#include "base/memory/scoped_ptr.h"
#include "cc/trees/occlusion.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace cc {

// Container for properties that layers need to compute before they can be
// drawn.
template <typename LayerType>
struct CC_EXPORT DrawProperties {
  DrawProperties()
      : opacity(0.f),
        blend_mode(SkXfermode::kSrcOver_Mode),
        opacity_is_animating(false),
        screen_space_opacity_is_animating(false),
        target_space_transform_is_animating(false),
        screen_space_transform_is_animating(false),
        can_use_lcd_text(false),
        is_clipped(false),
        render_target(nullptr),
        num_unclipped_descendants(0),
        layer_or_descendant_has_copy_request(false),
        layer_or_descendant_has_input_handler(false),
        has_child_with_a_scroll_parent(false),
        index_of_first_descendants_addition(0),
        num_descendants_added(0),
        index_of_first_render_surface_layer_list_addition(0),
        num_render_surfaces_added(0),
        last_drawn_render_surface_layer_list_id(0),
        maximum_animation_contents_scale(0.f),
        starting_animation_contents_scale(0.f) {}

  // Transforms objects from content space to target surface space, where
  // this layer would be drawn.
  gfx::Transform target_space_transform;

  // Transforms objects from content space to screen space (viewport space).
  gfx::Transform screen_space_transform;

  // Known occlusion above the layer mapped to the content space of the layer.
  Occlusion occlusion_in_content_space;

  // DrawProperties::opacity may be different than LayerType::opacity,
  // particularly in the case when a RenderSurface re-parents the layer's
  // opacity, or when opacity is compounded by the hierarchy.
  float opacity;

  // DrawProperties::blend_mode may be different than LayerType::blend_mode,
  // when a RenderSurface re-parents the layer's blend_mode.
  SkXfermode::Mode blend_mode;

  // xxx_is_animating flags are used to indicate whether the DrawProperties
  // are actually meaningful on the main thread. When the properties are
  // animating, the main thread may not have the same values that are used
  // to draw.
  bool opacity_is_animating;
  bool screen_space_opacity_is_animating;
  bool target_space_transform_is_animating;
  bool screen_space_transform_is_animating;

  // True if the layer can use LCD text.
  bool can_use_lcd_text;

  // True if the layer needs to be clipped by clip_rect.
  bool is_clipped;

  // The layer whose coordinate space this layer draws into. This can be
  // either the same layer (draw_properties_.render_target == this) or an
  // ancestor of this layer.
  LayerType* render_target;

  // This rect is a bounding box around what part of the layer is visible, in
  // the layer's coordinate space.
  gfx::Rect visible_layer_rect;

  // In target surface space, the rect that encloses the clipped, drawable
  // content of the layer.
  gfx::Rect drawable_content_rect;

  // In target surface space, the original rect that clipped this layer. This
  // value is used to avoid unnecessarily changing GL scissor state.
  gfx::Rect clip_rect;

  // Number of descendants with a clip parent that is our ancestor. NB - this
  // does not include our clip children because they are clipped by us.
  size_t num_unclipped_descendants;

  // If true, the layer or some layer in its sub-tree has a CopyOutputRequest
  // present on it.
  bool layer_or_descendant_has_copy_request;

  // If true, the layer or one of its descendants has a wheel or touch handler.
  bool layer_or_descendant_has_input_handler;

  // This is true if the layer has any direct child that has a scroll parent.
  // This layer will not be the scroll parent in this case. This information
  // lets us avoid work in CalculateDrawPropertiesInternal -- if none of our
  // children have scroll parents, we will not need to recur out of order.
  bool has_child_with_a_scroll_parent;

  // If this layer is visited out of order, its contribution to the descendant
  // and render surface layer lists will be put aside in a temporary list.
  // These values will allow for an efficient reordering of these additions.
  size_t index_of_first_descendants_addition;
  size_t num_descendants_added;
  size_t index_of_first_render_surface_layer_list_addition;
  size_t num_render_surfaces_added;

  // Each time we generate a new render surface layer list, an ID is used to
  // identify it. |last_drawn_render_surface_layer_list_id| is set to the ID
  // that marked the render surface layer list generation which last updated
  // these draw properties and determined that this layer will draw itself.
  // If these draw properties are not a part of the render surface layer list,
  // or the layer doesn't contribute anything, then this ID will be either out
  // of date or 0.
  int last_drawn_render_surface_layer_list_id;

  // The maximum scale during the layers current animation at which content
  // should be rastered at to be crisp.
  float maximum_animation_contents_scale;

  // The scale during the layer animation start at which content should be
  // rastered at to be crisp.
  float starting_animation_contents_scale;
};

}  // namespace cc

#endif  // CC_LAYERS_DRAW_PROPERTIES_H_
