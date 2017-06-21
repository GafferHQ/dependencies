// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_H_
#define CC_RESOURCES_RESOURCE_H_

#include "base/numerics/safe_math.h"
#include "cc/base/cc_export.h"
#include "cc/resources/resource_provider.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class CC_EXPORT Resource {
 public:
  Resource() : id_(0), format_(RGBA_8888) {}
  Resource(unsigned id, const gfx::Size& size, ResourceFormat format)
      : id_(id),
        size_(size),
        format_(format) {}

  ResourceId id() const { return id_; }
  gfx::Size size() const { return size_; }
  ResourceFormat format() const { return format_; }

  // Return true if the call to UncheckedMemorySizeBytes would return a value
  // that fits in a size_t.
  static bool VerifySizeInBytes(const gfx::Size& size, ResourceFormat format) {
    base::CheckedNumeric<size_t> checked_value = BitsPerPixel(format);
    checked_value *= size.width();
    checked_value *= size.height();
    if (!checked_value.IsValid())
      return false;
    size_t value = checked_value.ValueOrDie();
    if ((value % 8) != 0)
      return false;
    return true;
  }

  static size_t CheckedMemorySizeBytes(const gfx::Size& size,
                                       ResourceFormat format) {
    DCHECK(VerifySizeInBytes(size, format));
    base::CheckedNumeric<size_t> checked_value = BitsPerPixel(format);
    checked_value *= size.width();
    checked_value *= size.height();
    checked_value /= 8;
    return checked_value.ValueOrDie();
  }

  inline static size_t UncheckedMemorySizeBytes(const gfx::Size& size,
                                                ResourceFormat format) {
    DCHECK(VerifySizeInBytes(size, format));
    return static_cast<size_t>(BitsPerPixel(format)) * size.width() *
           size.height() / 8;
  }

 protected:
  void set_id(ResourceId id) { id_ = id; }
  void set_dimensions(const gfx::Size& size, ResourceFormat format) {
    size_ = size;
    format_ = format;
  }

 private:
  ResourceId id_;
  gfx::Size size_;
  ResourceFormat format_;

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_H_
