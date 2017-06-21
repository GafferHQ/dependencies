// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IPC Messages sent between compositor instances.

#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/filter_operation.h"
#include "cc/output/viewport_selection_bound.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/transferable_resource.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "content/common/content_export.h"
#include "gpu/ipc/gpu_command_buffer_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#ifndef CONTENT_COMMON_CC_MESSAGES_H_
#define CONTENT_COMMON_CC_MESSAGES_H_

namespace gfx {
class Transform;
}

namespace cc {
class FilterOperations;
}

namespace IPC {

template <>
struct ParamTraits<cc::FilterOperation> {
  typedef cc::FilterOperation param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<cc::FilterOperations> {
  typedef cc::FilterOperations param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<skia::RefPtr<SkImageFilter> > {
  typedef skia::RefPtr<SkImageFilter> param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<gfx::Transform> {
  typedef gfx::Transform param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<cc::RenderPass> {
  typedef cc::RenderPass param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct CONTENT_EXPORT ParamTraits<cc::CompositorFrame> {
  typedef cc::CompositorFrame param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct CONTENT_EXPORT ParamTraits<cc::CompositorFrameAck> {
  typedef cc::CompositorFrameAck param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct CONTENT_EXPORT ParamTraits<cc::DelegatedFrameData> {
  typedef cc::DelegatedFrameData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<cc::SoftwareFrameData> {
  typedef cc::SoftwareFrameData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<cc::DrawQuad::Resources> {
  typedef cc::DrawQuad::Resources param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<cc::StreamVideoDrawQuad::OverlayResources> {
  typedef cc::StreamVideoDrawQuad::OverlayResources param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<cc::TextureDrawQuad::OverlayResources> {
  typedef cc::TextureDrawQuad::OverlayResources param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_CC_MESSAGES_H_

// Multiply-included message file, hence no include guard.

#define IPC_MESSAGE_START CCMsgStart
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(cc::DrawQuad::Material, cc::DrawQuad::MATERIAL_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(cc::IOSurfaceDrawQuad::Orientation,
                          cc::IOSurfaceDrawQuad::ORIENTATION_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(cc::FilterOperation::FilterType,
                          cc::FilterOperation::FILTER_TYPE_LAST )
IPC_ENUM_TRAITS_MAX_VALUE(cc::ResourceFormat, cc::RESOURCE_FORMAT_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(cc::SelectionBoundType, cc::SELECTION_BOUND_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(SkXfermode::Mode, SkXfermode::kLastMode)
IPC_ENUM_TRAITS_MAX_VALUE(cc::YUVVideoDrawQuad::ColorSpace,
                          cc::YUVVideoDrawQuad::COLOR_SPACE_LAST)

IPC_STRUCT_TRAITS_BEGIN(cc::RenderPassId)
  IPC_STRUCT_TRAITS_MEMBER(layer_id)
  IPC_STRUCT_TRAITS_MEMBER(index)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SurfaceId)
  IPC_STRUCT_TRAITS_MEMBER(id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SurfaceSequence)
  IPC_STRUCT_TRAITS_MEMBER(id_namespace)
  IPC_STRUCT_TRAITS_MEMBER(sequence)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(material)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(opaque_rect)
  IPC_STRUCT_TRAITS_MEMBER(visible_rect)
  IPC_STRUCT_TRAITS_MEMBER(needs_blending)
  IPC_STRUCT_TRAITS_MEMBER(resources)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::CheckerboardDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(scale)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::DebugBorderDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(width)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::IOSurfaceDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(io_surface_size)
  IPC_STRUCT_TRAITS_MEMBER(orientation)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::RenderPassDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(render_pass_id)
  IPC_STRUCT_TRAITS_MEMBER(mask_uv_scale)
  IPC_STRUCT_TRAITS_MEMBER(mask_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(filters)
  IPC_STRUCT_TRAITS_MEMBER(filters_scale)
  IPC_STRUCT_TRAITS_MEMBER(background_filters)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SolidColorDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(force_anti_aliasing_off)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::StreamVideoDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(overlay_resources)
  IPC_STRUCT_TRAITS_MEMBER(matrix)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SurfaceDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(surface_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TextureDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(overlay_resources)
  IPC_STRUCT_TRAITS_MEMBER(premultiplied_alpha)
  IPC_STRUCT_TRAITS_MEMBER(uv_top_left)
  IPC_STRUCT_TRAITS_MEMBER(uv_bottom_right)
  IPC_STRUCT_TRAITS_MEMBER(background_color)
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[0])
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[1])
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[2])
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[3])
  IPC_STRUCT_TRAITS_MEMBER(y_flipped)
  IPC_STRUCT_TRAITS_MEMBER(nearest_neighbor)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TileDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(tex_coord_rect)
  IPC_STRUCT_TRAITS_MEMBER(texture_size)
  IPC_STRUCT_TRAITS_MEMBER(swizzle_contents)
  IPC_STRUCT_TRAITS_MEMBER(nearest_neighbor)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::YUVVideoDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(ya_tex_coord_rect)
  IPC_STRUCT_TRAITS_MEMBER(uv_tex_coord_rect)
  IPC_STRUCT_TRAITS_MEMBER(ya_tex_size)
  IPC_STRUCT_TRAITS_MEMBER(uv_tex_size)
  IPC_STRUCT_TRAITS_MEMBER(color_space)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SharedQuadState)
IPC_STRUCT_TRAITS_MEMBER(quad_to_target_transform)
IPC_STRUCT_TRAITS_MEMBER(quad_layer_bounds)
IPC_STRUCT_TRAITS_MEMBER(visible_quad_layer_rect)
  IPC_STRUCT_TRAITS_MEMBER(clip_rect)
  IPC_STRUCT_TRAITS_MEMBER(is_clipped)
  IPC_STRUCT_TRAITS_MEMBER(opacity)
  IPC_STRUCT_TRAITS_MEMBER(blend_mode)
  IPC_STRUCT_TRAITS_MEMBER(sorting_context_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TransferableResource)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(format)
  IPC_STRUCT_TRAITS_MEMBER(filter)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(mailbox_holder)
  IPC_STRUCT_TRAITS_MEMBER(read_lock_fences_enabled)
  IPC_STRUCT_TRAITS_MEMBER(is_repeated)
  IPC_STRUCT_TRAITS_MEMBER(is_software)
  IPC_STRUCT_TRAITS_MEMBER(allow_overlay)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::ReturnedResource)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(sync_point)
  IPC_STRUCT_TRAITS_MEMBER(count)
  IPC_STRUCT_TRAITS_MEMBER(lost)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::ViewportSelectionBound)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(edge_top)
  IPC_STRUCT_TRAITS_MEMBER(edge_bottom)
  IPC_STRUCT_TRAITS_MEMBER(visible)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::ViewportSelection)
  IPC_STRUCT_TRAITS_MEMBER(start)
  IPC_STRUCT_TRAITS_MEMBER(end)
  IPC_STRUCT_TRAITS_MEMBER(is_editable)
  IPC_STRUCT_TRAITS_MEMBER(is_empty_text_form_control)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE( \
    cc::BeginFrameArgs::BeginFrameArgsType, \
    cc::BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX - 1)

IPC_STRUCT_TRAITS_BEGIN(cc::BeginFrameArgs)
  IPC_STRUCT_TRAITS_MEMBER(frame_time)
  IPC_STRUCT_TRAITS_MEMBER(deadline)
  IPC_STRUCT_TRAITS_MEMBER(interval)
  IPC_STRUCT_TRAITS_MEMBER(type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::CompositorFrameMetadata)
  IPC_STRUCT_TRAITS_MEMBER(device_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(root_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(scrollable_viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(root_layer_size)
  IPC_STRUCT_TRAITS_MEMBER(min_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(max_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(root_overflow_x_hidden)
  IPC_STRUCT_TRAITS_MEMBER(root_overflow_y_hidden)
  IPC_STRUCT_TRAITS_MEMBER(location_bar_offset)
  IPC_STRUCT_TRAITS_MEMBER(location_bar_content_translation)
  IPC_STRUCT_TRAITS_MEMBER(selection)
  IPC_STRUCT_TRAITS_MEMBER(latency_info)
  IPC_STRUCT_TRAITS_MEMBER(satisfies_sequences)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::GLFrameData)
  IPC_STRUCT_TRAITS_MEMBER(mailbox)
  IPC_STRUCT_TRAITS_MEMBER(sync_point)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(sub_buffer_rect)
IPC_STRUCT_TRAITS_END()
