// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm/browser_cdm_manager.h"

#include <string>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/task_runner.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "media/base/browser_cdm.h"
#include "media/base/browser_cdm_factory.h"
#include "media/base/cdm_promise.h"
#include "media/base/limits.h"

#if defined(OS_ANDROID)
#include "content/public/common/renderer_preferences.h"
#endif

namespace content {

using media::BrowserCdm;
using media::MediaKeys;

namespace {

#if defined(OS_ANDROID)
// Android only supports 128-bit key IDs.
const size_t kAndroidKeyIdBytes = 128 / 8;
#endif

// The ID used in this class is a concatenation of |render_frame_id| and
// |cdm_id|, i.e. (render_frame_id << 32) + cdm_id.

uint64 GetId(int render_frame_id, int cdm_id) {
  return (static_cast<uint64>(render_frame_id) << 32) +
         static_cast<uint64>(cdm_id);
}

bool IdBelongsToFrame(uint64 id, int render_frame_id) {
  return (id >> 32) == static_cast<uint64>(render_frame_id);
}

// media::CdmPromiseTemplate implementation backed by a BrowserCdmManager.
template <typename... T>
class CdmPromiseInternal : public media::CdmPromiseTemplate<T...> {
 public:
  CdmPromiseInternal(BrowserCdmManager* manager,
                     int render_frame_id,
                     int cdm_id,
                     uint32_t promise_id)
      : manager_(manager),
        render_frame_id_(render_frame_id),
        cdm_id_(cdm_id),
        promise_id_(promise_id) {
    DCHECK(manager_);
  }

  ~CdmPromiseInternal() final {}

  // CdmPromiseTemplate<> implementation.
  void resolve(const T&... result) final;

  void reject(MediaKeys::Exception exception,
              uint32_t system_code,
              const std::string& error_message) final {
    MarkPromiseSettled();
    manager_->RejectPromise(render_frame_id_, cdm_id_, promise_id_, exception,
                            system_code, error_message);
  }

 private:
  using media::CdmPromiseTemplate<T...>::MarkPromiseSettled;

  BrowserCdmManager* const manager_;
  const int render_frame_id_;
  const int cdm_id_;
  const uint32_t promise_id_;
};

template <>
void CdmPromiseInternal<>::resolve() {
  MarkPromiseSettled();
  manager_->ResolvePromise(render_frame_id_, cdm_id_, promise_id_);
}

template <>
void CdmPromiseInternal<std::string>::resolve(const std::string& session_id) {
  MarkPromiseSettled();
  manager_->ResolvePromiseWithSession(render_frame_id_, cdm_id_, promise_id_,
                                      session_id);
}

typedef CdmPromiseInternal<> SimplePromise;
typedef CdmPromiseInternal<std::string> NewSessionPromise;

// Render process ID to BrowserCdmManager map.
typedef std::map<int, BrowserCdmManager*> BrowserCdmManagerMap;
base::LazyInstance<BrowserCdmManagerMap>::Leaky g_browser_cdm_manager_map =
    LAZY_INSTANCE_INITIALIZER;

// Keeps the BrowserCdmManager alive, and in the global map, for as long as the
// RenderProcessHost is connected to the child process. This class is a
// self-owned observer.
class BrowserCdmManagerProcessWatcher : public RenderProcessHostObserver {
 public:
  BrowserCdmManagerProcessWatcher(
      int render_process_id,
      const scoped_refptr<BrowserCdmManager>& manager)
      : browser_cdm_manager_(manager) {
    RenderProcessHost::FromID(render_process_id)->AddObserver(this);
    CHECK(g_browser_cdm_manager_map.Get()
              .insert(std::make_pair(render_process_id, manager.get()))
              .second);
  }

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus /* status */,
                           int /* exit_code */) override {
    RemoveHostObserverAndDestroy(host);
  }

  void RenderProcessHostDestroyed(RenderProcessHost* host) override {
    RemoveHostObserverAndDestroy(host);
  }

 private:
  void RemoveHostObserverAndDestroy(RenderProcessHost* host) {
    CHECK(g_browser_cdm_manager_map.Get().erase(host->GetID()));
    host->RemoveObserver(this);
    delete this;
  }

  const scoped_refptr<BrowserCdmManager> browser_cdm_manager_;
};

}  // namespace

// static
BrowserCdmManager* BrowserCdmManager::FromProcess(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& map = g_browser_cdm_manager_map.Get();
  auto iterator = map.find(render_process_id);
  return (iterator == map.end()) ? nullptr : iterator->second;
}

BrowserCdmManager::BrowserCdmManager(
    int render_process_id,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : BrowserMessageFilter(CdmMsgStart),
      render_process_id_(render_process_id),
      task_runner_(task_runner),
      weak_ptr_factory_(this) {
  DVLOG(1) << __FUNCTION__ << ": " << render_process_id_;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  new BrowserCdmManagerProcessWatcher(render_process_id, this);

  if (!task_runner_.get()) {
    task_runner_ =
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  }
}

BrowserCdmManager::~BrowserCdmManager() {
  DVLOG(1) << __FUNCTION__ << ": " << render_process_id_;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

// Makes sure BrowserCdmManager is always deleted on the Browser UI thread.
void BrowserCdmManager::OnDestruct() const {
  DVLOG(1) << __FUNCTION__ << ": " << render_process_id_;
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
  } else {
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }
}

base::TaskRunner* BrowserCdmManager::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  // Only handles CDM messages.
  if (IPC_MESSAGE_CLASS(message) != CdmMsgStart)
    return NULL;

  return task_runner_.get();
}

bool BrowserCdmManager::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserCdmManager, msg)
    IPC_MESSAGE_HANDLER(CdmHostMsg_InitializeCdm, OnInitializeCdm)
    IPC_MESSAGE_HANDLER(CdmHostMsg_SetServerCertificate, OnSetServerCertificate)
    IPC_MESSAGE_HANDLER(CdmHostMsg_CreateSessionAndGenerateRequest,
                        OnCreateSessionAndGenerateRequest)
    IPC_MESSAGE_HANDLER(CdmHostMsg_LoadSession, OnLoadSession)
    IPC_MESSAGE_HANDLER(CdmHostMsg_UpdateSession, OnUpdateSession)
    IPC_MESSAGE_HANDLER(CdmHostMsg_CloseSession, OnCloseSession)
    IPC_MESSAGE_HANDLER(CdmHostMsg_RemoveSession, OnRemoveSession)
    IPC_MESSAGE_HANDLER(CdmHostMsg_DestroyCdm, OnDestroyCdm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

media::BrowserCdm* BrowserCdmManager::GetCdm(int render_frame_id,
                                             int cdm_id) const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return cdm_map_.get(GetId(render_frame_id, cdm_id));
}

void BrowserCdmManager::RenderFrameDeleted(int render_frame_id) {
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserCdmManager::RemoveAllCdmForFrame,
                   this, render_frame_id));
    return;
  }
  RemoveAllCdmForFrame(render_frame_id);
}

void BrowserCdmManager::ResolvePromise(int render_frame_id,
                                       int cdm_id,
                                       uint32_t promise_id) {
  Send(new CdmMsg_ResolvePromise(render_frame_id, cdm_id, promise_id));
}

void BrowserCdmManager::ResolvePromiseWithSession(
    int render_frame_id,
    int cdm_id,
    uint32_t promise_id,
    const std::string& session_id) {
  if (session_id.length() > media::limits::kMaxSessionIdLength) {
    RejectPromise(render_frame_id, cdm_id, promise_id,
                  MediaKeys::INVALID_ACCESS_ERROR, 0,
                  "Session ID is too long.");
    return;
  }

  Send(new CdmMsg_ResolvePromiseWithSession(render_frame_id, cdm_id, promise_id,
                                            session_id));
}

void BrowserCdmManager::RejectPromise(int render_frame_id,
                                      int cdm_id,
                                      uint32_t promise_id,
                                      media::MediaKeys::Exception exception,
                                      uint32_t system_code,
                                      const std::string& error_message) {
  Send(new CdmMsg_RejectPromise(render_frame_id, cdm_id, promise_id, exception,
                                system_code, error_message));
}

void BrowserCdmManager::OnSessionMessage(
    int render_frame_id,
    int cdm_id,
    const std::string& session_id,
    media::MediaKeys::MessageType message_type,
    const std::vector<uint8>& message,
    const GURL& legacy_destination_url) {
  GURL verified_gurl = legacy_destination_url;
  if (!verified_gurl.is_valid() && !verified_gurl.is_empty()) {
    DLOG(WARNING) << "SessionMessage legacy_destination_url is invalid : "
                  << legacy_destination_url.possibly_invalid_spec();
    verified_gurl =
        GURL::EmptyGURL();  // Replace invalid legacy_destination_url.
  }

  Send(new CdmMsg_SessionMessage(render_frame_id, cdm_id, session_id,
                                 message_type, message, verified_gurl));
}

void BrowserCdmManager::OnSessionClosed(int render_frame_id,
                                        int cdm_id,
                                        const std::string& session_id) {
  Send(new CdmMsg_SessionClosed(render_frame_id, cdm_id, session_id));
}

void BrowserCdmManager::OnLegacySessionError(
    int render_frame_id,
    int cdm_id,
    const std::string& session_id,
    MediaKeys::Exception exception_code,
    uint32 system_code,
    const std::string& error_message) {
  Send(new CdmMsg_LegacySessionError(render_frame_id, cdm_id, session_id,
                                     exception_code, system_code,
                                     error_message));
}

void BrowserCdmManager::OnSessionKeysChange(int render_frame_id,
                                            int cdm_id,
                                            const std::string& session_id,
                                            bool has_additional_usable_key,
                                            media::CdmKeysInfo keys_info) {
  std::vector<media::CdmKeyInformation> key_info_vector;
  for (const auto& key_info : keys_info)
    key_info_vector.push_back(*key_info);
  Send(new CdmMsg_SessionKeysChange(render_frame_id, cdm_id, session_id,
                                    has_additional_usable_key,
                                    key_info_vector));
}

void BrowserCdmManager::OnSessionExpirationUpdate(
    int render_frame_id,
    int cdm_id,
    const std::string& session_id,
    const base::Time& new_expiry_time) {
  Send(new CdmMsg_SessionExpirationUpdate(render_frame_id, cdm_id, session_id,
                                          new_expiry_time));
}

void BrowserCdmManager::OnInitializeCdm(
    int render_frame_id,
    int cdm_id,
    uint32_t promise_id,
    const CdmHostMsg_InitializeCdm_Params& params) {
  if (params.key_system.size() > media::limits::kMaxKeySystemLength) {
    NOTREACHED() << "Invalid key system: " << params.key_system;
    RejectPromise(render_frame_id, cdm_id, promise_id,
                  MediaKeys::INVALID_ACCESS_ERROR, 0, "Invalid key system.");
    return;
  }

  AddCdm(render_frame_id, cdm_id, promise_id, params.key_system,
         params.security_origin, params.use_hw_secure_codecs);
}

void BrowserCdmManager::OnSetServerCertificate(
    int render_frame_id,
    int cdm_id,
    uint32_t promise_id,
    const std::vector<uint8_t>& certificate) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<SimplePromise> promise(
      new SimplePromise(this, render_frame_id, cdm_id, promise_id));

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  if (certificate.empty()) {
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0, "Empty certificate.");
    return;
  }

  cdm->SetServerCertificate(certificate, promise.Pass());
}

void BrowserCdmManager::OnCreateSessionAndGenerateRequest(
    const CdmHostMsg_CreateSessionAndGenerateRequest_Params& params) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  int render_frame_id = params.render_frame_id;
  int cdm_id = params.cdm_id;
  const std::vector<uint8>& init_data = params.init_data;
  scoped_ptr<NewSessionPromise> promise(
      new NewSessionPromise(this, render_frame_id, cdm_id, params.promise_id));

  if (init_data.size() > media::limits::kMaxInitDataLength) {
    LOG(WARNING) << "InitData for ID: " << cdm_id
                 << " too long: " << init_data.size();
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0, "Init data too long.");
    return;
  }
#if defined(OS_ANDROID)
  // 'webm' initData is a single key ID. On Android the length is restricted.
  if (params.init_data_type == INIT_DATA_TYPE_WEBM &&
      init_data.size() != kAndroidKeyIdBytes) {
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0,
                    "'webm' initData is not the correct length.");
    return;
  }
#endif

  media::EmeInitDataType eme_init_data_type;
  switch (params.init_data_type) {
    case INIT_DATA_TYPE_WEBM:
      eme_init_data_type = media::EmeInitDataType::WEBM;
      break;
#if defined(USE_PROPRIETARY_CODECS)
    case INIT_DATA_TYPE_CENC:
      eme_init_data_type = media::EmeInitDataType::CENC;
      break;
#endif
    default:
      NOTREACHED();
      promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0,
                      "Invalid init data type.");
      return;
  }

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM found for: " << render_frame_id << ", " << cdm_id;
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  CheckPermissionStatus(
      render_frame_id, cdm_id,
      base::Bind(&BrowserCdmManager::CreateSessionAndGenerateRequestIfPermitted,
                 this, render_frame_id, cdm_id, params.session_type,
                 eme_init_data_type, init_data, base::Passed(&promise)));
}

void BrowserCdmManager::OnLoadSession(
    int render_frame_id,
    int cdm_id,
    uint32_t promise_id,
    media::MediaKeys::SessionType session_type,
    const std::string& session_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<NewSessionPromise> promise(
      new NewSessionPromise(this, render_frame_id, cdm_id, promise_id));

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM found for: " << render_frame_id << ", " << cdm_id;
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  CheckPermissionStatus(
      render_frame_id, cdm_id,
      base::Bind(&BrowserCdmManager::LoadSessionIfPermitted,
                 this, render_frame_id, cdm_id, session_type,
                 session_id, base::Passed(&promise)));
}

void BrowserCdmManager::OnUpdateSession(int render_frame_id,
                                        int cdm_id,
                                        uint32_t promise_id,
                                        const std::string& session_id,
                                        const std::vector<uint8>& response) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<SimplePromise> promise(
      new SimplePromise(this, render_frame_id, cdm_id, promise_id));

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  if (response.size() > media::limits::kMaxSessionResponseLength) {
    LOG(WARNING) << "Response for ID " << cdm_id
                 << " is too long: " << response.size();
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0, "Response too long.");
    return;
  }

  if (response.empty()) {
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0, "Response is empty.");
    return;
  }

  cdm->UpdateSession(session_id, response, promise.Pass());
}

void BrowserCdmManager::OnCloseSession(int render_frame_id,
                                       int cdm_id,
                                       uint32_t promise_id,
                                       const std::string& session_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<SimplePromise> promise(
      new SimplePromise(this, render_frame_id, cdm_id, promise_id));

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  cdm->CloseSession(session_id, promise.Pass());
}

void BrowserCdmManager::OnRemoveSession(int render_frame_id,
                                        int cdm_id,
                                        uint32_t promise_id,
                                        const std::string& session_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  scoped_ptr<SimplePromise> promise(
      new SimplePromise(this, render_frame_id, cdm_id, promise_id));

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  cdm->RemoveSession(session_id, promise.Pass());
}

void BrowserCdmManager::OnDestroyCdm(int render_frame_id, int cdm_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  RemoveCdm(GetId(render_frame_id, cdm_id));
}

// Use a weak pointer here instead of |this| to avoid circular references.
#define BROWSER_CDM_MANAGER_CB(func)                                   \
  base::Bind(&BrowserCdmManager::func, weak_ptr_factory_.GetWeakPtr(), \
             render_frame_id, cdm_id)

void BrowserCdmManager::AddCdm(int render_frame_id,
                               int cdm_id,
                               uint32_t promise_id,
                               const std::string& key_system,
                               const GURL& security_origin,
                               bool use_hw_secure_codecs) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!GetCdm(render_frame_id, cdm_id));

  scoped_ptr<SimplePromise> promise(
      new SimplePromise(this, render_frame_id, cdm_id, promise_id));

  scoped_ptr<BrowserCdm> cdm(media::CreateBrowserCdm(
      key_system, use_hw_secure_codecs,
      BROWSER_CDM_MANAGER_CB(OnSessionMessage),
      BROWSER_CDM_MANAGER_CB(OnSessionClosed),
      BROWSER_CDM_MANAGER_CB(OnLegacySessionError),
      BROWSER_CDM_MANAGER_CB(OnSessionKeysChange),
      BROWSER_CDM_MANAGER_CB(OnSessionExpirationUpdate)));

  if (!cdm) {
    DVLOG(1) << "failed to create CDM.";
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "Failed to create CDM.");
    return;
  }

  uint64 id = GetId(render_frame_id, cdm_id);
  cdm_map_.add(id, cdm.Pass());
  cdm_security_origin_map_[id] = security_origin;
  promise->resolve();
}

void BrowserCdmManager::RemoveAllCdmForFrame(int render_frame_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  std::vector<uint64> ids_to_remove;
  for (CdmMap::iterator it = cdm_map_.begin(); it != cdm_map_.end(); ++it) {
    if (IdBelongsToFrame(it->first, render_frame_id))
      ids_to_remove.push_back(it->first);
  }

  for (size_t i = 0; i < ids_to_remove.size(); ++i)
    RemoveCdm(ids_to_remove[i]);
}

void BrowserCdmManager::RemoveCdm(uint64 id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  cdm_map_.erase(id);
  cdm_security_origin_map_.erase(id);
}

void BrowserCdmManager::CheckPermissionStatus(
    int render_frame_id,
    int cdm_id,
    const PermissionStatusCB& permission_status_cb) {
  // Always called on |task_runner_|, which may not be on the UI thread.
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  GURL security_origin;
  std::map<uint64, GURL>::const_iterator iter =
      cdm_security_origin_map_.find(GetId(render_frame_id, cdm_id));
  DCHECK(iter != cdm_security_origin_map_.end());
  if (iter != cdm_security_origin_map_.end())
    security_origin = iter->second;

  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&BrowserCdmManager::CheckPermissionStatusOnUIThread, this,
                   render_frame_id, security_origin, permission_status_cb));
  } else {
    CheckPermissionStatusOnUIThread(render_frame_id, security_origin,
                                    permission_status_cb);
  }
}

// Note: This function runs on the UI thread, which may be different from
// |task_runner_|. Be careful about thread safety!
void BrowserCdmManager::CheckPermissionStatusOnUIThread(
    int render_frame_id,
    const GURL& security_origin,
    const base::Callback<void(bool)>& permission_status_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* rfh =
      RenderFrameHost::FromID(render_process_id_, render_frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  PermissionManager* permission_manager =
      web_contents->GetBrowserContext()->GetPermissionManager();
  if (!permission_manager) {
    permission_status_cb.Run(false);
    return;
  }

  PermissionStatus permission_status = permission_manager->GetPermissionStatus(
      PermissionType::PROTECTED_MEDIA_IDENTIFIER, security_origin,
      web_contents->GetLastCommittedURL().GetOrigin());

  bool allowed = (permission_status == PERMISSION_STATUS_GRANTED);
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(permission_status_cb, allowed));
  } else {
    permission_status_cb.Run(allowed);
  }
}

void BrowserCdmManager::CreateSessionAndGenerateRequestIfPermitted(
    int render_frame_id,
    int cdm_id,
    media::MediaKeys::SessionType session_type,
    media::EmeInitDataType init_data_type,
    const std::vector<uint8>& init_data,
    scoped_ptr<media::NewSessionCdmPromise> promise,
    bool permission_was_allowed) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!permission_was_allowed) {
    promise->reject(MediaKeys::NOT_SUPPORTED_ERROR, 0, "Permission denied.");
    return;
  }

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  cdm->CreateSessionAndGenerateRequest(session_type, init_data_type,
                                       init_data, promise.Pass());
}

void BrowserCdmManager::LoadSessionIfPermitted(
    int render_frame_id,
    int cdm_id,
    media::MediaKeys::SessionType session_type,
    const std::string& session_id,
    scoped_ptr<media::NewSessionCdmPromise> promise,
    bool permission_was_allowed) {
  DCHECK_NE(media::MediaKeys::SessionType::TEMPORARY_SESSION, session_type);
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  if (!permission_was_allowed) {
    promise->reject(MediaKeys::NOT_SUPPORTED_ERROR, 0, "Permission denied.");
    return;
  }

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  cdm->LoadSession(session_type, session_id, promise.Pass());
}

}  // namespace content
