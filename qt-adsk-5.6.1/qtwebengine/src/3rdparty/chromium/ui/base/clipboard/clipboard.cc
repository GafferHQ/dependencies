// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard.h"

#include <iterator>
#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

base::LazyInstance<Clipboard::AllowedThreadsVector>
    Clipboard::allowed_threads_ = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<Clipboard::ClipboardMap> Clipboard::clipboard_map_ =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::Lock>::Leaky Clipboard::clipboard_map_lock_ =
    LAZY_INSTANCE_INITIALIZER;

// static
void Clipboard::SetAllowedThreads(
    const std::vector<base::PlatformThreadId>& allowed_threads) {
  base::AutoLock lock(clipboard_map_lock_.Get());

  allowed_threads_.Get().clear();
  std::copy(allowed_threads.begin(), allowed_threads.end(),
            std::back_inserter(allowed_threads_.Get()));
}

// static
Clipboard* Clipboard::GetForCurrentThread() {
  base::AutoLock lock(clipboard_map_lock_.Get());

  base::PlatformThreadId id = base::PlatformThread::CurrentId();

  AllowedThreadsVector* allowed_threads = allowed_threads_.Pointer();
  if (!allowed_threads->empty()) {
    bool found = false;
    for (AllowedThreadsVector::const_iterator it = allowed_threads->begin();
         it != allowed_threads->end(); ++it) {
      if (*it == id) {
        found = true;
        break;
      }
    }

    DCHECK(found);
  }

  ClipboardMap* clipboard_map = clipboard_map_.Pointer();
  ClipboardMap::const_iterator it = clipboard_map->find(id);
  if (it != clipboard_map->end())
    return it->second;

  Clipboard* clipboard = Clipboard::Create();
  clipboard_map->insert(std::make_pair(id, clipboard));
  return clipboard;
}

void Clipboard::DestroyClipboardForCurrentThread() {
  base::AutoLock lock(clipboard_map_lock_.Get());

  ClipboardMap* clipboard_map = clipboard_map_.Pointer();
  base::PlatformThreadId id = base::PlatformThread::CurrentId();
  ClipboardMap::iterator it = clipboard_map->find(id);
  if (it != clipboard_map->end()) {
    delete it->second;
    clipboard_map->erase(it);
  }
}

void Clipboard::DispatchObject(ObjectType type, const ObjectMapParams& params) {
  // Ignore writes with empty parameters.
  for (const auto& param : params) {
    if (param.empty())
      return;
  }

  switch (type) {
    case CBF_TEXT:
      WriteText(&(params[0].front()), params[0].size());
      break;

    case CBF_HTML:
      if (params.size() == 2) {
        if (params[1].empty())
          return;
        WriteHTML(&(params[0].front()), params[0].size(),
                  &(params[1].front()), params[1].size());
      } else if (params.size() == 1) {
        WriteHTML(&(params[0].front()), params[0].size(), NULL, 0);
      }
      break;

    case CBF_RTF:
      WriteRTF(&(params[0].front()), params[0].size());
      break;

    case CBF_BOOKMARK:
      WriteBookmark(&(params[0].front()), params[0].size(),
                    &(params[1].front()), params[1].size());
      break;

    case CBF_WEBKIT:
      WriteWebSmartPaste();
      break;

    case CBF_SMBITMAP: {
      // Usually, the params are just UTF-8 strings. However, for images,
      // ScopedClipboardWriter actually sizes the buffer to sizeof(SkBitmap*),
      // aliases the contents of the vector to a SkBitmap**, and writes the
      // pointer to the actual SkBitmap in the clipboard object param.
      const char* packed_pointer_buffer = &params[0].front();
      WriteBitmap(**reinterpret_cast<SkBitmap* const*>(packed_pointer_buffer));
      break;
    }

    case CBF_DATA:
      WriteData(
          FormatType::Deserialize(
              std::string(&(params[0].front()), params[0].size())),
          &(params[1].front()),
          params[1].size());
      break;

    default:
      NOTREACHED();
  }
}

}  // namespace ui
