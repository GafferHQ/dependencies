// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/readback_types.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class Layer;
}

namespace gfx {
class Rect;
}

namespace ui {
class ViewAndroid;
class WindowAndroid;
}

namespace content {
class WebContents;

// Native side of the ContentViewCore.java, which is the primary way of
// communicating with the native Chromium code on Android.  This is a
// public interface used by native code outside of the content module.
class CONTENT_EXPORT ContentViewCore {
 public:
  // Returns the existing ContentViewCore for |web_contents|, or nullptr.
  static ContentViewCore* FromWebContents(WebContents* web_contents);
  static ContentViewCore* GetNativeContentViewCore(JNIEnv* env, jobject obj);

  virtual WebContents* GetWebContents() const = 0;

  // May return null reference.
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
  virtual ui::ViewAndroid* GetViewAndroid() const = 0;
  virtual ui::WindowAndroid* GetWindowAndroid() const = 0;
  virtual const scoped_refptr<cc::Layer>& GetLayer() const = 0;
  virtual void ShowPastePopup(int x, int y) = 0;

  // Request a scaled content readback. The result is passed through the
  // callback. The boolean parameter indicates whether the readback was a
  // success or not. The content is passed through the SkBitmap parameter.
  virtual void GetScaledContentBitmap(
      float scale,
      SkColorType color_type,
      const gfx::Rect& src_rect,
      ReadbackRequestCallback& result_callback) = 0;
  virtual float GetDpiScale() const = 0;
  virtual void PauseOrResumeGeolocation(bool should_pause) = 0;

  // Text surrounding selection.
  virtual void RequestTextSurroundingSelection(
      int max_length,
      const base::Callback<void(const base::string16& content,
                                int start_offset,
                                int end_offset)>& callback) = 0;

 protected:
  virtual ~ContentViewCore() {};
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_CORE_H_
