// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/skia_benchmarking_extension.h"

#include "base/base64.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/playback/picture.h"
#include "content/public/child/v8_value_converter.h"
#include "content/renderer/chrome_object_extensions_utils.h"
#include "content/renderer/render_thread_impl.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "skia/ext/benchmarking_canvas.h"
#include "third_party/WebKit/public/web/WebArrayBuffer.h"
#include "third_party/WebKit/public/web/WebArrayBufferConverter.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "v8/include/v8.h"

namespace content {

namespace {

scoped_ptr<base::Value> ParsePictureArg(v8::Isolate* isolate,
                                        v8::Local<v8::Value> arg) {
  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());
  return scoped_ptr<base::Value>(
      converter->FromV8Value(arg, isolate->GetCurrentContext()));
}

scoped_refptr<cc::Picture> ParsePictureStr(v8::Isolate* isolate,
                                           v8::Local<v8::Value> arg) {
  scoped_ptr<base::Value> picture_value = ParsePictureArg(isolate, arg);
  if (!picture_value)
    return NULL;
  return cc::Picture::CreateFromSkpValue(picture_value.get());
}

scoped_refptr<cc::Picture> ParsePictureHash(v8::Isolate* isolate,
                                            v8::Local<v8::Value> arg) {
  scoped_ptr<base::Value> picture_value = ParsePictureArg(isolate, arg);
  if (!picture_value)
    return NULL;
  return cc::Picture::CreateFromValue(picture_value.get());
}

class PicturePlaybackController : public SkPicture::AbortCallback {
 public:
  PicturePlaybackController(const skia::BenchmarkingCanvas& canvas,
                            size_t count)
      : canvas_(canvas), playback_count_(count) {}

  bool abort() override { return canvas_.CommandCount() > playback_count_; }

 private:
  const skia::BenchmarkingCanvas& canvas_;
  size_t playback_count_;
};

}  // namespace

gin::WrapperInfo SkiaBenchmarking::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
void SkiaBenchmarking::Install(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<SkiaBenchmarking> controller =
      gin::CreateHandle(isolate, new SkiaBenchmarking());
  if (controller.IsEmpty())
    return;

  v8::Local<v8::Object> chrome = GetOrCreateChromeObject(isolate,
                                                          context->Global());
  chrome->Set(gin::StringToV8(isolate, "skiaBenchmarking"), controller.ToV8());
}

// static
void SkiaBenchmarking::Initialize() {
  DCHECK(RenderThreadImpl::current());
  // FIXME: remove this after Skia updates SkGraphics::Init() to be
  //        thread-safe and idempotent.
  static bool skia_initialized = false;
  if (!skia_initialized) {
    LOG(WARNING) << "Enabling unsafe Skia benchmarking extension.";
    SkGraphics::Init();
    skia_initialized = true;
  }
}

SkiaBenchmarking::SkiaBenchmarking() {
  Initialize();
}

SkiaBenchmarking::~SkiaBenchmarking() {}

gin::ObjectTemplateBuilder SkiaBenchmarking::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<SkiaBenchmarking>::GetObjectTemplateBuilder(isolate)
      .SetMethod("rasterize", &SkiaBenchmarking::Rasterize)
      .SetMethod("getOps", &SkiaBenchmarking::GetOps)
      .SetMethod("getOpTimings", &SkiaBenchmarking::GetOpTimings)
      .SetMethod("getInfo", &SkiaBenchmarking::GetInfo);
}

void SkiaBenchmarking::Rasterize(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Local<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  scoped_refptr<cc::Picture> picture =
      ParsePictureHash(isolate, picture_handle);
  if (!picture.get())
    return;

  double scale = 1.0;
  gfx::Rect clip_rect(picture->LayerRect());
  int stop_index = -1;
  bool overdraw = false;

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (!args->PeekNext().IsEmpty()) {
    v8::Local<v8::Value> params;
    args->GetNext(&params);
    scoped_ptr<content::V8ValueConverter> converter(
        content::V8ValueConverter::create());
    scoped_ptr<base::Value> params_value(
        converter->FromV8Value(params, context));

    const base::DictionaryValue* params_dict = NULL;
    if (params_value.get() && params_value->GetAsDictionary(&params_dict)) {
      params_dict->GetDouble("scale", &scale);
      params_dict->GetInteger("stop", &stop_index);
      params_dict->GetBoolean("overdraw", &overdraw);

      const base::Value* clip_value = NULL;
      if (params_dict->Get("clip", &clip_value))
        cc::MathUtil::FromValue(clip_value, &clip_rect);
    }
  }

  gfx::RectF clip(clip_rect);
  clip.Intersect(picture->LayerRect());
  clip.Scale(scale);
  gfx::Rect snapped_clip = gfx::ToEnclosingRect(clip);

  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(snapped_clip.width(), snapped_clip.height()))
    return;
  bitmap.eraseARGB(0, 0, 0, 0);

  SkCanvas canvas(bitmap);
  canvas.translate(SkFloatToScalar(-clip.x()), SkFloatToScalar(-clip.y()));
  canvas.clipRect(gfx::RectToSkRect(snapped_clip));
  canvas.scale(scale, scale);
  canvas.translate(picture->LayerRect().x(), picture->LayerRect().y());

  skia::BenchmarkingCanvas benchmarking_canvas(
      &canvas,
      overdraw ? skia::BenchmarkingCanvas::kOverdrawVisualization_Flag : 0);
  size_t playback_count =
      (stop_index < 0) ? std::numeric_limits<size_t>::max() : stop_index;
  PicturePlaybackController controller(benchmarking_canvas, playback_count);
  picture->Replay(&benchmarking_canvas, &controller);

  blink::WebArrayBuffer buffer =
      blink::WebArrayBuffer::create(bitmap.getSize(), 1);
  uint32* packed_pixels = reinterpret_cast<uint32*>(bitmap.getPixels());
  uint8* buffer_pixels = reinterpret_cast<uint8*>(buffer.data());
  // Swizzle from native Skia format to RGBA as we copy out.
  for (size_t i = 0; i < bitmap.getSize(); i += 4) {
    uint32 c = packed_pixels[i >> 2];
    buffer_pixels[i] = SkGetPackedR32(c);
    buffer_pixels[i + 1] = SkGetPackedG32(c);
    buffer_pixels[i + 2] = SkGetPackedB32(c);
    buffer_pixels[i + 3] = SkGetPackedA32(c);
  }

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result->Set(v8::String::NewFromUtf8(isolate, "width"),
              v8::Number::New(isolate, snapped_clip.width()));
  result->Set(v8::String::NewFromUtf8(isolate, "height"),
              v8::Number::New(isolate, snapped_clip.height()));
  result->Set(v8::String::NewFromUtf8(isolate, "data"),
              blink::WebArrayBufferConverter::toV8Value(
                  &buffer, context->Global(), isolate));

  args->Return(result);
}

void SkiaBenchmarking::GetOps(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Local<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  scoped_refptr<cc::Picture> picture =
      ParsePictureHash(isolate, picture_handle);
  if (!picture.get())
    return;

  SkCanvas canvas(picture->LayerRect().width(), picture->LayerRect().height());
  skia::BenchmarkingCanvas benchmarking_canvas(&canvas);
  picture->Replay(&benchmarking_canvas);

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  scoped_ptr<content::V8ValueConverter> converter(
      content::V8ValueConverter::create());

  args->Return(converter->ToV8Value(&benchmarking_canvas.Commands(), context));
}

void SkiaBenchmarking::GetOpTimings(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Local<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  scoped_refptr<cc::Picture> picture =
      ParsePictureHash(isolate, picture_handle);
  if (!picture.get())
    return;

  gfx::Rect bounds = picture->LayerRect();

  // Measure the total time by drawing straight into a bitmap-backed canvas.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(bounds.width(), bounds.height());
  SkCanvas bitmap_canvas(bitmap);
  bitmap_canvas.clear(SK_ColorTRANSPARENT);
  base::TimeTicks t0 = base::TimeTicks::Now();
  picture->Replay(&bitmap_canvas);
  base::TimeDelta total_time = base::TimeTicks::Now() - t0;

  // Gather per-op timing info by drawing into a BenchmarkingCanvas.
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  skia::BenchmarkingCanvas benchmarking_canvas(&canvas);
  picture->Replay(&benchmarking_canvas);

  v8::Local<v8::Array> op_times =
      v8::Array::New(isolate, benchmarking_canvas.CommandCount());
  for (size_t i = 0; i < benchmarking_canvas.CommandCount(); ++i) {
    op_times->Set(i, v8::Number::New(isolate, benchmarking_canvas.GetTime(i)));
  }

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result->Set(v8::String::NewFromUtf8(isolate, "total_time"),
              v8::Number::New(isolate, total_time.InMillisecondsF()));
  result->Set(v8::String::NewFromUtf8(isolate, "cmd_times"), op_times);

  args->Return(result);
}

void SkiaBenchmarking::GetInfo(gin::Arguments* args) {
  v8::Isolate* isolate = args->isolate();
  if (args->PeekNext().IsEmpty())
    return;
  v8::Local<v8::Value> picture_handle;
  args->GetNext(&picture_handle);
  scoped_refptr<cc::Picture> picture =
      ParsePictureStr(isolate, picture_handle);
  if (!picture.get())
    return;

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result->Set(v8::String::NewFromUtf8(isolate, "width"),
              v8::Number::New(isolate, picture->LayerRect().width()));
  result->Set(v8::String::NewFromUtf8(isolate, "height"),
              v8::Number::New(isolate, picture->LayerRect().height()));

  args->Return(result);
}

} // namespace content
