// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_browser_cdm_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/media/base/media_message_loop.h"
#include "chromecast/media/cdm/browser_cdm_cast.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_key_information.h"

namespace chromecast {
namespace media {

scoped_ptr<::media::BrowserCdm> CastBrowserCdmFactory::CreateBrowserCdm(
    const std::string& key_system_name,
    bool use_hw_secure_codecs,
    const ::media::SessionMessageCB& session_message_cb,
    const ::media::SessionClosedCB& session_closed_cb,
    const ::media::LegacySessionErrorCB& legacy_session_error_cb,
    const ::media::SessionKeysChangeCB& session_keys_change_cb,
    const ::media::SessionExpirationUpdateCB& session_expiration_update_cb) {
  DCHECK(!use_hw_secure_codecs)
      << "Chromecast does not use |use_hw_secure_codecs|";

  CastKeySystem key_system(GetKeySystemByName(key_system_name));

  scoped_ptr<chromecast::media::BrowserCdmCast> browser_cdm;
  if (key_system == chromecast::media::KEY_SYSTEM_CLEAR_KEY) {
    // TODO(gunsch): handle ClearKey decryption. See crbug.com/441957
  } else {
    browser_cdm = CreatePlatformBrowserCdm(key_system);
  }

  if (browser_cdm) {
    MediaMessageLoop::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&BrowserCdmCast::Initialize,
                   base::Unretained(browser_cdm.get()),
                   ::media::BindToCurrentLoop(session_message_cb),
                   ::media::BindToCurrentLoop(session_closed_cb),
                   ::media::BindToCurrentLoop(legacy_session_error_cb),
                   ::media::BindToCurrentLoop(session_keys_change_cb),
                   ::media::BindToCurrentLoop(session_expiration_update_cb)));
    return make_scoped_ptr(new BrowserCdmCastUi(
        browser_cdm.Pass(), MediaMessageLoop::GetTaskRunner()));
  }

  LOG(INFO) << "No matching key system found.";
  return scoped_ptr< ::media::BrowserCdm>();
}

}  // namespace media
}  // namespace chromecast
