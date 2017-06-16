// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_H_

#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "content/browser/geolocation/wifi_data.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT WifiDataProvider
    : public base::RefCountedThreadSafe<WifiDataProvider> {
 public:
  WifiDataProvider();

  // Tells the provider to start looking for data. Callbacks will start
  // receiving notifications after this call.
  virtual void StartDataProvider() = 0;

  // Tells the provider to stop looking for data. Callbacks will stop
  // receiving notifications after this call.
  virtual void StopDataProvider() = 0;

  // Provides whatever data the provider has, which may be nothing. Return
  // value indicates whether this is all the data the provider could ever
  // obtain.
  virtual bool GetData(WifiData* data) = 0;

  typedef base::Closure WifiDataUpdateCallback;

  void AddCallback(WifiDataUpdateCallback* callback);

  bool RemoveCallback(WifiDataUpdateCallback* callback);

  bool has_callbacks() const;

 protected:
  friend class base::RefCountedThreadSafe<WifiDataProvider>;
  virtual ~WifiDataProvider();

  typedef std::set<WifiDataUpdateCallback*> CallbackSet;

  // Runs all callbacks via a posted task, so we can unwind callstack here and
  // avoid client reentrancy.
  void RunCallbacks();

  bool CalledOnClientThread() const;

  base::MessageLoop* client_loop() const;

 private:
  void DoRunCallbacks();

  // Reference to the client's message loop. All callbacks should happen in this
  // context.
  base::MessageLoop* client_loop_;

  CallbackSet callbacks_;

  DISALLOW_COPY_AND_ASSIGN(WifiDataProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_DATA_PROVIDER_H_
