// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/webui/web_ui_util.h"

#include <vector>

#include "base/base64.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/strings/grit/app_locale_settings.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace webui {

std::string GetBitmapDataUrl(const SkBitmap& bitmap) {
  TRACE_EVENT2("oobe", "GetImageDataUrl",
               "width", bitmap.width(), "height", bitmap.height());
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output);
  std::string str_url;
  str_url.insert(str_url.end(), output.begin(), output.end());

  base::Base64Encode(str_url, &str_url);
  str_url.insert(0, "data:image/png;base64,");
  return str_url;
}

WindowOpenDisposition GetDispositionFromClick(const base::ListValue* args,
                                              int start_index) {
  double button = 0.0;
  bool alt_key = false;
  bool ctrl_key = false;
  bool meta_key = false;
  bool shift_key = false;

  CHECK(args->GetDouble(start_index++, &button));
  CHECK(args->GetBoolean(start_index++, &alt_key));
  CHECK(args->GetBoolean(start_index++, &ctrl_key));
  CHECK(args->GetBoolean(start_index++, &meta_key));
  CHECK(args->GetBoolean(start_index++, &shift_key));
  return ui::DispositionFromClick(
      button == 1.0, alt_key, ctrl_key, meta_key, shift_key);
}

bool ParseScaleFactor(const base::StringPiece& identifier,
                      float* scale_factor) {
  *scale_factor = 1.0f;
  if (identifier.empty()) {
    LOG(WARNING) << "Invalid scale factor format: " << identifier;
    return false;
  }

  if (*identifier.rbegin() != 'x') {
    LOG(WARNING) << "Invalid scale factor format: " << identifier;
    return false;
  }

  double scale = 0;
  std::string stripped;
  identifier.substr(0, identifier.length() - 1).CopyToString(&stripped);
  if (!base::StringToDouble(stripped, &scale)) {
    LOG(WARNING) << "Invalid scale factor format: " << identifier;
    return false;
  }
  *scale_factor = static_cast<float>(scale);
  return true;
}

void ParsePathAndScale(const GURL& url,
                       std::string* path,
                       float* scale_factor) {
  *path = net::UnescapeURLComponent(url.path().substr(1),
                                    (net::UnescapeRule::URL_SPECIAL_CHARS |
                                     net::UnescapeRule::SPACES));
  if (scale_factor)
    *scale_factor = 1.0f;

  // Detect and parse resource string ending in @<scale>x.
  std::size_t pos = path->rfind('@');
  if (pos != std::string::npos) {
    base::StringPiece stripped_path(*path);
    float factor;

    if (ParseScaleFactor(stripped_path.substr(
            pos + 1, stripped_path.length() - pos - 1), &factor)) {
      // Strip scale factor specification from path.
      stripped_path.remove_suffix(stripped_path.length() - pos);
      stripped_path.CopyToString(path);
    }
    if (scale_factor)
      *scale_factor = factor;
  }
}

void SetLoadTimeDataDefaults(const std::string& app_locale,
                             base::DictionaryValue* localized_strings) {
  localized_strings->SetString("fontfamily", GetFontFamily());
  localized_strings->SetString("fontsize", GetFontSize());
  localized_strings->SetString("language", l10n_util::GetLanguage(app_locale));
  localized_strings->SetString("textdirection", GetTextDirection());
}

std::string GetWebUiCssTextDefaults() {
  std::vector<std::string> placeholders;
  placeholders.push_back(GetTextDirection());  // $1
  placeholders.push_back(GetFontFamily());  // $2
  placeholders.push_back(GetFontSize());  // $3

  const ui::ResourceBundle& resource_bundle =
      ui::ResourceBundle::GetSharedInstance();
  const std::string& css_template =
      resource_bundle.GetRawDataResource(IDR_WEBUI_CSS_TEXT_DEFAULTS)
          .as_string();

  return ReplaceStringPlaceholders(css_template, placeholders, nullptr);
}

void AppendWebUiCssTextDefaults(std::string* html) {
  html->append("<style>");
  html->append(GetWebUiCssTextDefaults());
  html->append("</style>");
}

std::string GetFontFamily() {
  std::string font_family = l10n_util::GetStringUTF8(
#if defined(OS_WIN)
      base::win::GetVersion() < base::win::VERSION_VISTA ?
          IDS_WEB_FONT_FAMILY_XP :
#endif
          IDS_WEB_FONT_FAMILY);

// TODO(dnicoara) Remove Ozone check when PlatformFont support is introduced
// into Ozone: crbug.com/320050
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(USE_OZONE) && !defined(TOOLKIT_QT)
  font_family = ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont).GetFontName() + ", " + font_family;
#endif

  return font_family;
}

std::string GetFontSize() {
  return l10n_util::GetStringUTF8(
#if defined(OS_WIN)
      base::win::GetVersion() < base::win::VERSION_VISTA ?
          IDS_WEB_FONT_SIZE_XP :
#endif
          IDS_WEB_FONT_SIZE);
}

std::string GetTextDirection() {
  return base::i18n::IsRTL() ? "rtl" : "ltr";
}

}  // namespace webui
