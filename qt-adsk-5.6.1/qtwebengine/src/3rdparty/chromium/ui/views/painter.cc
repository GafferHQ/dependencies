// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/painter.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/nine_image_painter.h"
#include "ui/views/view.h"

namespace views {

namespace {

// DashedFocusPainter ----------------------------------------------------------

class DashedFocusPainter : public Painter {
 public:
  explicit DashedFocusPainter(const gfx::Insets& insets);
  ~DashedFocusPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(DashedFocusPainter);
};

DashedFocusPainter::DashedFocusPainter(const gfx::Insets& insets)
    : insets_(insets) {
}

DashedFocusPainter::~DashedFocusPainter() {
}

gfx::Size DashedFocusPainter::GetMinimumSize() const {
  return gfx::Size();
}

void DashedFocusPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  gfx::Rect rect(size);
  rect.Inset(insets_);
  canvas->DrawFocusRect(rect);
}

// SolidFocusPainter -----------------------------------------------------------

class SolidFocusPainter : public Painter {
 public:
  SolidFocusPainter(SkColor color, const gfx::Insets& insets);
  ~SolidFocusPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  const SkColor color_;
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(SolidFocusPainter);
};

SolidFocusPainter::SolidFocusPainter(SkColor color,
                                     const gfx::Insets& insets)
    : color_(color),
      insets_(insets) {
}

SolidFocusPainter::~SolidFocusPainter() {
}

gfx::Size SolidFocusPainter::GetMinimumSize() const {
  return gfx::Size();
}

void SolidFocusPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  gfx::Rect rect(size);
  rect.Inset(insets_);
  canvas->DrawSolidFocusRect(rect, color_);
}

// GradientPainter ------------------------------------------------------------

class GradientPainter : public Painter {
 public:
  GradientPainter(bool horizontal,
                  SkColor* colors,
                  SkScalar* pos,
                  size_t count);
  ~GradientPainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  // If |horizontal_| is true then the gradient is painted horizontally.
  bool horizontal_;
  // The gradient colors.
  scoped_ptr<SkColor[]> colors_;
  // The relative positions of the corresponding gradient colors.
  scoped_ptr<SkScalar[]> pos_;
  // The number of elements in |colors_| and |pos_|.
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(GradientPainter);
};

GradientPainter::GradientPainter(bool horizontal,
                                 SkColor* colors,
                                 SkScalar* pos,
                                 size_t count)
    : horizontal_(horizontal),
      colors_(new SkColor[count]),
      pos_(new SkScalar[count]),
      count_(count) {
  for (size_t i = 0; i < count_; ++i) {
    pos_[i] = pos[i];
    colors_[i] = colors[i];
  }
}

GradientPainter::~GradientPainter() {
}

gfx::Size GradientPainter::GetMinimumSize() const {
  return gfx::Size();
}

void GradientPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  SkPaint paint;
  SkPoint p[2];
  p[0].iset(0, 0);
  if (horizontal_)
    p[1].iset(size.width(), 0);
  else
    p[1].iset(0, size.height());

  skia::RefPtr<SkShader> s = skia::AdoptRef(SkGradientShader::CreateLinear(
      p, colors_.get(), pos_.get(), count_, SkShader::kClamp_TileMode));
  paint.setStyle(SkPaint::kFill_Style);
  paint.setShader(s.get());

  canvas->sk_canvas()->drawRectCoords(SkIntToScalar(0), SkIntToScalar(0),
                                      SkIntToScalar(size.width()),
                                      SkIntToScalar(size.height()), paint);
}

// ImagePainter ---------------------------------------------------------------

// ImagePainter stores and paints nine images as a scalable grid.
class ImagePainter : public Painter {
 public:
  // Constructs an ImagePainter with the specified image resource ids.
  // See CreateImageGridPainter()'s comment regarding image ID count and order.
  explicit ImagePainter(const int image_ids[]);

  // Constructs an ImagePainter with the specified image and insets.
  ImagePainter(const gfx::ImageSkia& image, const gfx::Insets& insets);

  ~ImagePainter() override;

  // Painter:
  gfx::Size GetMinimumSize() const override;
  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override;

 private:
  scoped_ptr<gfx::NineImagePainter> nine_painter_;

  DISALLOW_COPY_AND_ASSIGN(ImagePainter);
};

ImagePainter::ImagePainter(const int image_ids[])
    : nine_painter_(ui::CreateNineImagePainter(image_ids)) {
}

ImagePainter::ImagePainter(const gfx::ImageSkia& image,
                           const gfx::Insets& insets)
    : nine_painter_(new gfx::NineImagePainter(image, insets)) {
}

ImagePainter::~ImagePainter() {
}

gfx::Size ImagePainter::GetMinimumSize() const {
  return nine_painter_->GetMinimumSize();
}

void ImagePainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  nine_painter_->Paint(canvas, gfx::Rect(size));
}

}  // namespace


// Painter --------------------------------------------------------------------

Painter::Painter() {
}

Painter::~Painter() {
}

// static
void Painter::PaintPainterAt(gfx::Canvas* canvas,
                             Painter* painter,
                             const gfx::Rect& rect) {
  DCHECK(canvas && painter);
  canvas->Save();
  canvas->Translate(rect.OffsetFromOrigin());
  painter->Paint(canvas, rect.size());
  canvas->Restore();
}

// static
void Painter::PaintFocusPainter(View* view,
                                gfx::Canvas* canvas,
                                Painter* focus_painter) {
  if (focus_painter && view->HasFocus())
    PaintPainterAt(canvas, focus_painter, view->GetLocalBounds());
}

// static
Painter* Painter::CreateHorizontalGradient(SkColor c1, SkColor c2) {
  SkColor colors[2];
  colors[0] = c1;
  colors[1] = c2;
  SkScalar pos[] = {0, 1};
  return new GradientPainter(true, colors, pos, 2);
}

// static
Painter* Painter::CreateVerticalGradient(SkColor c1, SkColor c2) {
  SkColor colors[2];
  colors[0] = c1;
  colors[1] = c2;
  SkScalar pos[] = {0, 1};
  return new GradientPainter(false, colors, pos, 2);
}

// static
Painter* Painter::CreateVerticalMultiColorGradient(SkColor* colors,
                                                   SkScalar* pos,
                                                   size_t count) {
  return new GradientPainter(false, colors, pos, count);
}

// static
Painter* Painter::CreateImagePainter(const gfx::ImageSkia& image,
                                     const gfx::Insets& insets) {
  return new ImagePainter(image, insets);
}

// static
Painter* Painter::CreateImageGridPainter(const int image_ids[]) {
  return new ImagePainter(image_ids);
}

// static
scoped_ptr<Painter> Painter::CreateDashedFocusPainter() {
  return make_scoped_ptr(new DashedFocusPainter(gfx::Insets()));
}

// static
scoped_ptr<Painter> Painter::CreateDashedFocusPainterWithInsets(
    const gfx::Insets& insets) {
  return make_scoped_ptr(new DashedFocusPainter(insets));
}

// static
scoped_ptr<Painter> Painter::CreateSolidFocusPainter(
    SkColor color,
    const gfx::Insets& insets) {
  return make_scoped_ptr(new SolidFocusPainter(color, insets));
}

// HorizontalPainter ----------------------------------------------------------

HorizontalPainter::HorizontalPainter(const int image_resource_names[]) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (int i = 0; i < 3; ++i)
    images_[i] = rb.GetImageNamed(image_resource_names[i]).ToImageSkia();
  DCHECK_EQ(images_[LEFT]->height(), images_[CENTER]->height());
  DCHECK_EQ(images_[LEFT]->height(), images_[RIGHT]->height());
}

HorizontalPainter::~HorizontalPainter() {
}

gfx::Size HorizontalPainter::GetMinimumSize() const {
  return gfx::Size(
      images_[LEFT]->width() + images_[CENTER]->width() +
          images_[RIGHT]->width(), images_[LEFT]->height());
}

void HorizontalPainter::Paint(gfx::Canvas* canvas, const gfx::Size& size) {
  if (size.width() < GetMinimumSize().width())
    return;  // No room to paint.

  canvas->DrawImageInt(*images_[LEFT], 0, 0);
  canvas->DrawImageInt(*images_[RIGHT], size.width() - images_[RIGHT]->width(),
                       0);
  canvas->TileImageInt(
      *images_[CENTER], images_[LEFT]->width(), 0,
      size.width() - images_[LEFT]->width() - images_[RIGHT]->width(),
      images_[LEFT]->height());
}

}  // namespace views
