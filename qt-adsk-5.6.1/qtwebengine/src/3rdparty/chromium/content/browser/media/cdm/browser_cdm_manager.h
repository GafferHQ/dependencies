// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/media/cdm_messages.h"
#include "content/common/media/cdm_messages_enums.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/permission_status.mojom.h"
#include "ipc/ipc_message.h"
#include "media/base/cdm_promise.h"
#include "media/base/eme_constants.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

struct CdmHostMsg_CreateSessionAndGenerateRequest_Params;

namespace media {
class BrowserCdm;
}

namespace content {

// This class manages all CDM objects. It receives control operations from the
// the render process, and forwards them to corresponding CDM object. Callbacks
// from CDM objects are converted to IPCs and then sent to the render process.
class CONTENT_EXPORT BrowserCdmManager : public BrowserMessageFilter {
 public:
  // Returns the BrowserCdmManager associated with the |render_process_id|.
  // Returns NULL if no BrowserCdmManager is associated.
  static BrowserCdmManager* FromProcess(int render_process_id);

  // Constructs the BrowserCdmManager for |render_process_id| which runs on
  // |task_runner|.
  // If |task_runner| is not NULL, all CDM messages are posted to it. Otherwise,
  // all messages are posted to the browser UI thread.
  BrowserCdmManager(int render_process_id,
                    const scoped_refptr<base::TaskRunner>& task_runner);

  // BrowserMessageFilter implementations.
  void OnDestruct() const override;
  base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  media::BrowserCdm* GetCdm(int render_frame_id, int cdm_id) const;

  // Notifies that the render frame has been deleted so that all CDMs belongs
  // to this render frame needs to be destroyed as well. This is needed because
  // in some cases (e.g. fast termination of the renderer), the message to
  // destroy the CDM will not be received.
  void RenderFrameDeleted(int render_frame_id);

  // Promise handlers.
  void ResolvePromise(int render_frame_id, int cdm_id, uint32_t promise_id);
  void ResolvePromiseWithSession(int render_frame_id,
                                 int cdm_id,
                                 uint32_t promise_id,
                                 const std::string& session_id);
  void RejectPromise(int render_frame_id,
                     int cdm_id,
                     uint32_t promise_id,
                     media::MediaKeys::Exception exception,
                     uint32_t system_code,
                     const std::string& error_message);

 protected:
  friend class base::RefCountedThreadSafe<BrowserCdmManager>;
  friend class base::DeleteHelper<BrowserCdmManager>;
  ~BrowserCdmManager() override;

 private:
  // CDM callbacks.
  void OnSessionMessage(int render_frame_id,
                        int cdm_id,
                        const std::string& session_id,
                        media::MediaKeys::MessageType message_type,
                        const std::vector<uint8>& message,
                        const GURL& legacy_destination_url);
  void OnSessionClosed(int render_frame_id,
                       int cdm_id,
                       const std::string& session_id);
  void OnLegacySessionError(int render_frame_id,
                            int cdm_id,
                            const std::string& session_id,
                            media::MediaKeys::Exception exception_code,
                            uint32_t system_code,
                            const std::string& error_message);
  void OnSessionKeysChange(int render_frame_id,
                           int cdm_id,
                           const std::string& session_id,
                           bool has_additional_usable_key,
                           media::CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(int render_frame_id,
                                 int cdm_id,
                                 const std::string& session_id,
                                 const base::Time& new_expiry_time);

  // Message handlers.
  void OnInitializeCdm(int render_frame_id,
                       int cdm_id,
                       uint32_t promise_id,
                       const CdmHostMsg_InitializeCdm_Params& params);
  void OnSetServerCertificate(int render_frame_id,
                              int cdm_id,
                              uint32_t promise_id,
                              const std::vector<uint8_t>& certificate);
  void OnCreateSessionAndGenerateRequest(
      const CdmHostMsg_CreateSessionAndGenerateRequest_Params& params);
  void OnLoadSession(
      int render_frame_id,
      int cdm_id,
      uint32_t promise_id,
      media::MediaKeys::SessionType session_type,
      const std::string& session_id);
  void OnUpdateSession(int render_frame_id,
                       int cdm_id,
                       uint32_t promise_id,
                       const std::string& session_id,
                       const std::vector<uint8>& response);
  void OnCloseSession(int render_frame_id,
                      int cdm_id,
                      uint32_t promise_id,
                      const std::string& session_id);
  void OnRemoveSession(int render_frame_id,
                       int cdm_id,
                       uint32_t promise_id,
                       const std::string& session_id);
  void OnDestroyCdm(int render_frame_id, int cdm_id);

  // Adds a new CDM identified by |cdm_id| for the given |key_system| and
  // |security_origin|.
  void AddCdm(int render_frame_id,
              int cdm_id,
              uint32_t promise_id,
              const std::string& key_system,
              const GURL& security_origin,
              bool use_hw_secure_codecs);

  // Removes all CDMs associated with |render_frame_id|.
  void RemoveAllCdmForFrame(int render_frame_id);

  // Removes the CDM with the specified id.
  void RemoveCdm(uint64 id);

  using PermissionStatusCB = base::Callback<void(bool)>;

  // Checks protected media identifier permission for the given
  // |render_frame_id| and |cdm_id|.
  void CheckPermissionStatus(int render_frame_id,
                             int cdm_id,
                             const PermissionStatusCB& permission_status_cb);

  // Checks permission status on Browser UI thread. Runs |permission_status_cb|
  // on the |task_runner_| with the permission status.
  void CheckPermissionStatusOnUIThread(
      int render_frame_id,
      const GURL& security_origin,
      const PermissionStatusCB& permission_status_cb);

  // Calls CreateSessionAndGenerateRequest() on the CDM if
  // |permission_was_allowed| is true. Otherwise rejects the |promise|.
  void CreateSessionAndGenerateRequestIfPermitted(
      int render_frame_id,
      int cdm_id,
      media::MediaKeys::SessionType session_type,
      media::EmeInitDataType init_data_type,
      const std::vector<uint8>& init_data,
      scoped_ptr<media::NewSessionCdmPromise> promise,
      bool permission_was_allowed);

  // Calls LoadSession() on the CDM if |permission_was_allowed| is true.
  // Otherwise rejects |promise|.
  void LoadSessionIfPermitted(int render_frame_id,
                              int cdm_id,
                              media::MediaKeys::SessionType session_type,
                              const std::string& session_id,
                              scoped_ptr<media::NewSessionCdmPromise> promise,
                              bool permission_was_allowed);

  const int render_process_id_;

  // TaskRunner to dispatch all CDM messages to. If it's NULL, all messages are
  // dispatched to the browser UI thread.
  scoped_refptr<base::TaskRunner> task_runner_;

  // The key in the following maps is a combination of |render_frame_id| and
  // |cdm_id|.

  // Map of managed BrowserCdms.
  typedef base::ScopedPtrHashMap<uint64, scoped_ptr<media::BrowserCdm>> CdmMap;
  CdmMap cdm_map_;

  // Map of CDM's security origin.
  std::map<uint64, GURL> cdm_security_origin_map_;

  base::WeakPtrFactory<BrowserCdmManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCdmManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_
