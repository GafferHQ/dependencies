// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include <fontconfig/fontconfig.h>

#include <map>
#include <string>
#include <vector>

#include "base/lazy_instance.h"

namespace gfx {

namespace {

typedef std::map<std::string, std::vector<std::string> > FallbackCache;
base::LazyInstance<FallbackCache>::Leaky g_fallback_cache =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

std::vector<std::string> GetFallbackFontFamilies(
    const std::string& font_family) {
  std::vector<std::string>* fallback_fonts =
      &g_fallback_cache.Get()[font_family];
  if (!fallback_fonts->empty())
    return *fallback_fonts;

  FcPattern* pattern = FcPatternCreate();
  FcValue family;
  family.type = FcTypeString;
  family.u.s = reinterpret_cast<const FcChar8*>(font_family.c_str());
  FcPatternAdd(pattern, FC_FAMILY, family, FcFalse);
  if (FcConfigSubstitute(NULL, pattern, FcMatchPattern) == FcTrue) {
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcFontSet* fonts = FcFontSort(NULL, pattern, FcTrue, NULL, &result);
    if (fonts) {
      for (int i = 0; i < fonts->nfont; ++i) {
        char* name = NULL;
        FcPatternGetString(fonts->fonts[i], FC_FAMILY, 0,
            reinterpret_cast<FcChar8**>(&name));
        // FontConfig returns multiple fonts with the same family name and
        // different configurations. Check to prevent duplicate family names.
        if (fallback_fonts->empty() || fallback_fonts->back() != name)
          fallback_fonts->push_back(std::string(name));
      }
      FcFontSetDestroy(fonts);
    }
  }
  FcPatternDestroy(pattern);

  if (fallback_fonts->empty())
    fallback_fonts->push_back(font_family);

  return *fallback_fonts;
}

}  // namespace gfx
