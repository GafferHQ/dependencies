// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_content_client.h"

#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "chromecast/base/version.h"
#include "content/public/common/user_agent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace shell {

namespace {

#if defined(OS_ANDROID)
std::string BuildAndroidOsInfo() {
  int32 os_major_version = 0;
  int32 os_minor_version = 0;
  int32 os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);

  std::string android_version_str;
  base::StringAppendF(
      &android_version_str, "%d.%d", os_major_version, os_minor_version);
  if (os_bugfix_version != 0)
    base::StringAppendF(&android_version_str, ".%d", os_bugfix_version);

  std::string android_info_str;
  // Append the build ID.
  std::string android_build_id = base::SysInfo::GetAndroidBuildID();
  if (android_build_id.size() > 0)
    android_info_str += "; Build/" + android_build_id;

  std::string os_info;
  base::StringAppendF(
      &os_info,
      "Android %s%s",
      android_version_str.c_str(),
      android_info_str.c_str());
  return os_info;
}
#endif

}  // namespace

std::string GetUserAgent() {
  std::string product = "Chrome/" PRODUCT_VERSION;
  std::string os_info;
  base::StringAppendF(
      &os_info,
      "%s%s",
#if defined(OS_ANDROID)
      "Linux; ",
      BuildAndroidOsInfo().c_str()
#else
      "X11; ",
      content::BuildOSCpuInfo().c_str()
#endif
      );
  return content::BuildUserAgentFromOSAndProduct(os_info, product) +
      " CrKey/" CAST_BUILD_REVISION;
}

CastContentClient::~CastContentClient() {
}

std::string CastContentClient::GetUserAgent() const {
  return chromecast::shell::GetUserAgent();
}

base::string16 CastContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece CastContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedStaticMemory* CastContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& CastContentClient::GetNativeImageNamed(int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace shell
}  // namespace chromecast
