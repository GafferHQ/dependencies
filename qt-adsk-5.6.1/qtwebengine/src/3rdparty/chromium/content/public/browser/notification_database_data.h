// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_DATABASE_DATA_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_DATABASE_DATA_H_

#include <stdint.h>

#include "content/public/common/platform_notification_data.h"
#include "url/gurl.h"

namespace content {

// Stores information about a Web Notification as available in the notification
// database. Beyond the notification's own data, its id and attribution need
// to be available for users of the database as well.
struct NotificationDatabaseData {
  // Id of the notification as allocated by the NotificationDatabase.
  int64_t notification_id = 0;

  // Origin of the website this notification is associated with.
  GURL origin;

  // Id of the Service Worker registration this notification is associated with.
  int64_t service_worker_registration_id = 0;

  // Platform data of the notification that's being stored.
  PlatformNotificationData notification_data;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_DATABASE_DATA_H_
