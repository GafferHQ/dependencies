// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_PROMISE_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_PROMISE_H_

#include "base/macros.h"
#include "media/base/cdm_promise.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/services/mojo_type_trait.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/callback.h"

namespace media {

// media::CdmPromiseTemplate implementations backed by mojo::Callbacks.
template <typename... T>
class MojoCdmPromise : public CdmPromiseTemplate<T...> {
 public:
  typedef mojo::Callback<void(mojo::CdmPromiseResultPtr,
                              typename MojoTypeTrait<T>::MojoType...)>
      CallbackType;

  MojoCdmPromise(const CallbackType& callback);
  ~MojoCdmPromise() final;

  // CdmPromiseTemplate<> implementation.
  void resolve(const T&... result) final;
  void reject(MediaKeys::Exception exception,
              uint32_t system_code,
              const std::string& error_message) final;

 private:
  using media::CdmPromiseTemplate<T...>::MarkPromiseSettled;

  CallbackType callback_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_PROMISE_H_
