// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_permission_manager.h"

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/permission_type.h"

namespace chromecast {
namespace shell {

CastPermissionManager::CastPermissionManager()
    : content::PermissionManager() {
}

CastPermissionManager::~CastPermissionManager() {
}

void CastPermissionManager::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    int request_id,
    const GURL& origin,
    bool user_gesture,
    const base::Callback<void(content::PermissionStatus)>& callback) {
  LOG(INFO) << __FUNCTION__ << ": " << static_cast<int>(permission);
  callback.Run(content::PermissionStatus::PERMISSION_STATUS_GRANTED);
}

void CastPermissionManager::CancelPermissionRequest(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    int request_id,
    const GURL& origin) {
}

void CastPermissionManager::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

content::PermissionStatus CastPermissionManager::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  LOG(INFO) << __FUNCTION__ << ": " << static_cast<int>(permission);
  return content::PermissionStatus::PERMISSION_STATUS_GRANTED;
}

void CastPermissionManager::RegisterPermissionUsage(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

int CastPermissionManager::SubscribePermissionStatusChange(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(content::PermissionStatus)>& callback) {
  return -1;
}

void CastPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
}

}  // namespace shell
}  // namespace chromecast
