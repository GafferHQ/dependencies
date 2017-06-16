// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_MANAGER_H_

#include "content/common/content_export.h"
#include "content/public/common/permission_status.mojom.h"

class GURL;

namespace content {
enum class PermissionType;
class RenderFrameHost;

// This class allows the content layer to manipulate permissions. It has to be
// implemented by the embedder which ultimately handles the permission
// management for the content layer.
class CONTENT_EXPORT PermissionManager {
 public:
  virtual ~PermissionManager() = default;

  // Requests a permission on behalf of a frame identified by render_frame_host.
  // The |request_id| is an identifier that can later be used if the request is
  // cancelled (see CancelPermissionRequest).
  // When the permission request is handled, whether it failed, timed out or
  // succeeded, the |callback| will be run.
  virtual void RequestPermission(
      PermissionType permission,
      RenderFrameHost* render_frame_host,
      int request_id,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(PermissionStatus)>& callback) = 0;

  // Cancels a previously requested permission. The given parameter must match
  // the ones passed to the RequestPermission call.
  virtual void CancelPermissionRequest(PermissionType permission,
                                       RenderFrameHost* render_frame_host,
                                       int request_id,
                                       const GURL& requesting_origin) = 0;

  // Returns the permission status of a given requesting_origin/embedding_origin
  // tuple. This is not taking a RenderFrameHost because the call might happen
  // outside of a frame context.
  virtual PermissionStatus GetPermissionStatus(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin) = 0;

  // Sets the permission back to its default for the requesting_origin/
  // embedding_origin tuple.
  virtual void ResetPermission(PermissionType permission,
                               const GURL& requesting_origin,
                               const GURL& embedding_origin) = 0;

  // Registers a permission usage.
  // TODO(mlamouri): see if we can remove this from the PermissionManager.
  virtual void RegisterPermissionUsage(PermissionType permission,
                                       const GURL& requesting_origin,
                                       const GURL& embedding_origin) = 0;

  // Runs the given |callback| whenever the |permission| associated with the
  // pair { requesting_origin, embedding_origin } changes.
  // Returns the subscription_id to be used to unsubscribe.
  virtual int SubscribePermissionStatusChange(
      PermissionType permission,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      const base::Callback<void(PermissionStatus)>& callback) = 0;

  // Unregisters from permission status change notifications.
  // The |subscription_id| must match the value returned by the
  // SubscribePermissionStatusChange call.
  virtual void UnsubscribePermissionStatusChange(int subscription_id) = 0;
};

}  // namespace content

#endif // CONTENT_PUBLIC_BROWSER_PERMISSION_MANAGER_H_
