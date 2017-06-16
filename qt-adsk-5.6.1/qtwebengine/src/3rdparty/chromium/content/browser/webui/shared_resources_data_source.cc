// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/shared_resources_data_source.h"

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/mime_util.h"
#include "ui/base/layout.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/resources/grit/webui_resources_map.h"

namespace content {

namespace {

using ResourcesMap = base::hash_map<std::string, int>;

// TODO(rkc): Once we have a separate source for apps, remove '*/apps/' aliases.
const char* kPathAliases[][2] = {
    {"../../../third_party/polymer/v1_0/components-chromium/", "polymer/v1_0/"},
    {"../../../third_party/web-animations-js/sources/",
     "polymer/v1_0/web-animations-js/"},
    {"../../views/resources/default_100_percent/common/", "images/apps/"},
    {"../../views/resources/default_200_percent/common/", "images/2x/apps/"},
    {"../../webui/resources/cr_elements/", "cr_elements/"}};

void AddResource(const std::string& path,
                 int resource_id,
                 ResourcesMap* resources_map) {
  if (!resources_map->insert(std::make_pair(path, resource_id)).second)
    NOTREACHED() << "Redefinition of '" << path << "'";
}

const ResourcesMap* CreateResourcesMap() {
  ResourcesMap* result = new ResourcesMap();
  for (size_t i = 0; i < kWebuiResourcesSize; ++i) {
    const std::string resource_name = kWebuiResources[i].name;
    const int resource_id = kWebuiResources[i].value;
    AddResource(resource_name, resource_id, result);
    for (const char* (&alias)[2]: kPathAliases) {
      if (base::StartsWithASCII(resource_name, alias[0], true)) {
        AddResource(alias[1] + resource_name.substr(strlen(alias[0])),
                    resource_id, result);
      }
    }
  }

  return result;
}

const ResourcesMap& GetResourcesMap() {
  // This pointer will be intentionally leaked on shutdown.
  static const ResourcesMap* resources_map = CreateResourcesMap();
  return *resources_map;
}

}  // namespace

SharedResourcesDataSource::SharedResourcesDataSource() {
}

SharedResourcesDataSource::~SharedResourcesDataSource() {
}

std::string SharedResourcesDataSource::GetSource() const {
  return kChromeUIResourcesHost;
}

void SharedResourcesDataSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const URLDataSource::GotDataCallback& callback) {
  const ResourcesMap& resources_map = GetResourcesMap();
  auto it = resources_map.find(path);
  int idr = (it != resources_map.end()) ? it->second : -1;
  DCHECK_NE(-1, idr) << " path: " << path;
  scoped_refptr<base::RefCountedMemory> bytes;

  if (idr == IDR_WEBUI_CSS_TEXT_DEFAULTS) {
    std::string css = webui::GetWebUiCssTextDefaults();
    bytes = base::RefCountedString::TakeString(&css);
  } else {
    bytes = GetContentClient()->GetDataResourceBytes(idr);
  }

  callback.Run(bytes.get());
}

std::string SharedResourcesDataSource::GetMimeType(
    const std::string& path) const {
  // Requests should not block on the disk!  On POSIX this goes to disk.
  // http://code.google.com/p/chromium/issues/detail?id=59849

  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string mime_type;
  net::GetMimeTypeFromFile(base::FilePath().AppendASCII(path), &mime_type);
  return mime_type;
}

std::string
SharedResourcesDataSource::GetAccessControlAllowOriginForOrigin(
    const std::string& origin) const {
  // For now we give access only for "chrome://*" origins.
  // According to CORS spec, Access-Control-Allow-Origin header doesn't support
  // wildcards, so we need to set its value explicitly by passing the |origin|
  // back.
  std::string allowed_origin_prefix = kChromeUIScheme;
  allowed_origin_prefix += "://";
  if (origin.find(allowed_origin_prefix) != 0)
    return "null";
  return origin;
}

}  // namespace content
