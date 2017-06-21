// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_
#define MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_export.h"
#include "media/blink/key_system_config_selector.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaClient.h"

namespace blink {

class WebContentDecryptionModuleResult;
struct WebMediaKeySystemConfiguration;
class WebSecurityOrigin;

}  // namespace blink

namespace media {

struct CdmConfig;
class CdmFactory;
class KeySystems;
class MediaPermission;

class MEDIA_EXPORT WebEncryptedMediaClientImpl
    : public blink::WebEncryptedMediaClient {
 public:
  WebEncryptedMediaClientImpl(
      base::Callback<bool(void)> are_secure_codecs_supported_cb,
      CdmFactory* cdm_factory,
      MediaPermission* media_permission);
  virtual ~WebEncryptedMediaClientImpl();

  // WebEncryptedMediaClient implementation.
  virtual void requestMediaKeySystemAccess(
      blink::WebEncryptedMediaRequest request);

  // Create the CDM for |key_system| and |security_origin|. The caller owns
  // the created cdm (passed back using |result|).
  void CreateCdm(const blink::WebString& key_system,
                 const blink::WebSecurityOrigin& security_origin,
                 const CdmConfig& cdm_config,
                 scoped_ptr<blink::WebContentDecryptionModuleResult> result);

 private:
  // Report usage of key system to UMA. There are 2 different counts logged:
  // 1. The key system is requested.
  // 2. The requested key system and options are supported.
  // Each stat is only reported once per renderer frame per key system.
  class Reporter;

  // Complete a requestMediaKeySystemAccess() request with a supported
  // accumulated configuration.
  void OnRequestSucceeded(
      blink::WebEncryptedMediaRequest request,
      const blink::WebMediaKeySystemConfiguration& accumulated_configuration,
      const CdmConfig& cdm_config);

  // Complete a requestMediaKeySystemAccess() request with an error message.
  void OnRequestNotSupported(blink::WebEncryptedMediaRequest request,
                             const blink::WebString& error_message);

  // Gets the Reporter for |key_system|. If it doesn't already exist,
  // create one.
  Reporter* GetReporter(const blink::WebString& key_system);

  // Reporter singletons.
  base::ScopedPtrHashMap<std::string, scoped_ptr<Reporter>> reporters_;

  base::Callback<bool(void)> are_secure_codecs_supported_cb_;
  CdmFactory* cdm_factory_;
  KeySystemConfigSelector key_system_config_selector_;
  base::WeakPtrFactory<WebEncryptedMediaClientImpl> weak_factory_;
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_
