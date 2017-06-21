// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_REFLECTOR_TEXTURE_H_
#define CONTENT_BROWSER_COMPOSITOR_REFLECTOR_TEXTURE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/common/content_export.h"

namespace cc {
class ContextProvider;
}

namespace gfx {
class Rect;
class Size;
}

namespace content {
class GLHelper;

// Create and manages texture mailbox to be used by Reflector.
class CONTENT_EXPORT ReflectorTexture {
 public:
  explicit ReflectorTexture(cc::ContextProvider* provider);
  ~ReflectorTexture();

  void CopyTextureFullImage(const gfx::Size& size);
  void CopyTextureSubImage(const gfx::Rect& size);

  uint32 texture_id() const { return texture_id_; }
  scoped_refptr<OwnedMailbox> mailbox() { return mailbox_; }

 private:
  scoped_refptr<OwnedMailbox> mailbox_;
  scoped_ptr<GLHelper> gl_helper_;
  uint32 texture_id_;

  DISALLOW_COPY_AND_ASSIGN(ReflectorTexture);
};

}  // namespace content

#endif
