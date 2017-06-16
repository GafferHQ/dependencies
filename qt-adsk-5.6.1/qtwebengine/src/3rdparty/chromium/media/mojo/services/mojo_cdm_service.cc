// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service.h"

#include "base/bind.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_key_information.h"
#include "media/base/key_systems.h"
#include "media/mojo/services/media_type_converters.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "url/gurl.h"

namespace media {

using NewSessionMojoCdmPromise = MojoCdmPromise<std::string>;
using SimpleMojoCdmPromise = MojoCdmPromise<>;

MojoCdmService::MojoCdmService(
    MojoCdmServiceContext* context,
    mojo::ServiceProvider* service_provider,
    CdmFactory* cdm_factory,
    mojo::InterfaceRequest<mojo::ContentDecryptionModule> request)
    : binding_(this, request.Pass()),
      context_(context),
      service_provider_(service_provider),
      cdm_factory_(cdm_factory),
      cdm_id_(CdmContext::kInvalidCdmId),
      weak_factory_(this) {
  DCHECK(context_);
  DCHECK(cdm_factory_);
}

MojoCdmService::~MojoCdmService() {
  if (cdm_id_ != CdmContext::kInvalidCdmId)
    context_->UnregisterCdm(cdm_id_);
}

void MojoCdmService::SetClient(mojo::ContentDecryptionModuleClientPtr client) {
  client_ = client.Pass();
}

void MojoCdmService::Initialize(
    const mojo::String& key_system,
    const mojo::String& security_origin,
    mojo::CdmConfigPtr cdm_config,
    int32_t cdm_id,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) {
  DVLOG(1) << __FUNCTION__ << ": " << key_system;
  DCHECK(!cdm_);
  DCHECK_NE(CdmContext::kInvalidCdmId, cdm_id);

  auto weak_this = weak_factory_.GetWeakPtr();
  cdm_factory_->Create(
      key_system, GURL(security_origin), cdm_config.To<CdmConfig>(),
      base::Bind(&MojoCdmService::OnSessionMessage, weak_this),
      base::Bind(&MojoCdmService::OnSessionClosed, weak_this),
      base::Bind(&MojoCdmService::OnLegacySessionError, weak_this),
      base::Bind(&MojoCdmService::OnSessionKeysChange, weak_this),
      base::Bind(&MojoCdmService::OnSessionExpirationUpdate, weak_this),
      base::Bind(
          &MojoCdmService::OnCdmCreated, weak_this, cdm_id,
          base::Passed(make_scoped_ptr(new SimpleMojoCdmPromise(callback)))));
}

void MojoCdmService::SetServerCertificate(
    mojo::Array<uint8_t> certificate_data,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->SetServerCertificate(
      certificate_data.storage(),
      make_scoped_ptr(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::CreateSessionAndGenerateRequest(
    mojo::ContentDecryptionModule::SessionType session_type,
    mojo::ContentDecryptionModule::InitDataType init_data_type,
    mojo::Array<uint8_t> init_data,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr, mojo::String)>&
        callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->CreateSessionAndGenerateRequest(
      static_cast<MediaKeys::SessionType>(session_type),
      static_cast<EmeInitDataType>(init_data_type), init_data.storage(),
      make_scoped_ptr(new NewSessionMojoCdmPromise(callback)));
}

void MojoCdmService::LoadSession(
    mojo::ContentDecryptionModule::SessionType session_type,
    const mojo::String& session_id,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr, mojo::String)>&
        callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->LoadSession(static_cast<MediaKeys::SessionType>(session_type),
                    session_id.To<std::string>(),
                    make_scoped_ptr(new NewSessionMojoCdmPromise(callback)));
}

void MojoCdmService::UpdateSession(
    const mojo::String& session_id,
    mojo::Array<uint8_t> response,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->UpdateSession(
      session_id.To<std::string>(), response.storage(),
      scoped_ptr<SimpleCdmPromise>(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::CloseSession(
    const mojo::String& session_id,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->CloseSession(session_id.To<std::string>(),
                     make_scoped_ptr(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::RemoveSession(
    const mojo::String& session_id,
    const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) {
  DVLOG(2) << __FUNCTION__;
  cdm_->RemoveSession(session_id.To<std::string>(),
                      make_scoped_ptr(new SimpleMojoCdmPromise(callback)));
}

void MojoCdmService::GetDecryptor(
    mojo::InterfaceRequest<mojo::Decryptor> decryptor) {
  NOTIMPLEMENTED();
}

CdmContext* MojoCdmService::GetCdmContext() {
  return cdm_->GetCdmContext();
}

void MojoCdmService::OnCdmCreated(int cdm_id,
                                  scoped_ptr<SimpleMojoCdmPromise> promise,
                                  scoped_ptr<MediaKeys> cdm,
                                  const std::string& error_message) {
  // TODO(xhwang): This should not happen when KeySystemInfo is properly
  // populated. See http://crbug.com/469366
  if (!cdm) {
    promise->reject(MediaKeys::NOT_SUPPORTED_ERROR, 0, error_message);
    return;
  }

  cdm_ = cdm.Pass();
  cdm_id_ = cdm_id;
  context_->RegisterCdm(cdm_id_, this);
  promise->resolve();
}

void MojoCdmService::OnSessionMessage(const std::string& session_id,
                                      MediaKeys::MessageType message_type,
                                      const std::vector<uint8_t>& message,
                                      const GURL& legacy_destination_url) {
  DVLOG(2) << __FUNCTION__;
  client_->OnSessionMessage(session_id,
                            static_cast<mojo::CdmMessageType>(message_type),
                            mojo::Array<uint8_t>::From(message),
                            mojo::String::From(legacy_destination_url));
}

void MojoCdmService::OnSessionKeysChange(const std::string& session_id,
                                         bool has_additional_usable_key,
                                         CdmKeysInfo keys_info) {
  DVLOG(2) << __FUNCTION__;
  mojo::Array<mojo::CdmKeyInformationPtr> keys_data;
  for (const auto& key : keys_info)
    keys_data.push_back(mojo::CdmKeyInformation::From(*key));
  client_->OnSessionKeysChange(session_id, has_additional_usable_key,
                               keys_data.Pass());
}

void MojoCdmService::OnSessionExpirationUpdate(
    const std::string& session_id,
    const base::Time& new_expiry_time_sec) {
  DVLOG(2) << __FUNCTION__;
  client_->OnSessionExpirationUpdate(session_id,
                                     new_expiry_time_sec.ToDoubleT());
}

void MojoCdmService::OnSessionClosed(const std::string& session_id) {
  DVLOG(2) << __FUNCTION__;
  client_->OnSessionClosed(session_id);
}

void MojoCdmService::OnLegacySessionError(const std::string& session_id,
                                          MediaKeys::Exception exception,
                                          uint32_t system_code,
                                          const std::string& error_message) {
  DVLOG(2) << __FUNCTION__;
  client_->OnLegacySessionError(session_id,
                                static_cast<mojo::CdmException>(exception),
                                system_code, error_message);
}

}  // namespace media
