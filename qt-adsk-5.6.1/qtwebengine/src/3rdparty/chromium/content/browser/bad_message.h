// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BAD_MESSAGE_H_
#define CONTENT_BROWSER_BAD_MESSAGE_H_

namespace content {
class BrowserMessageFilter;
class RenderProcessHost;

namespace bad_message {

// The browser process often chooses to terminate a renderer if it receives
// a bad IPC message. The reasons are tracked for metrics.
//
// Content embedders should implement their own bad message statistics but
// should use similar histogram names to make analysis easier.
//
// NOTE: Do not remove or reorder elements in this list. Add new entries at the
// end. Items may be renamed but do not change the values. We rely on the enum
// values in histograms. Also update histograms.xml with any new values by
// running:
//    python tools/metrics/histograms/update_bad_message_reasons.py
enum BadMessageReason {
  NC_IN_PAGE_NAVIGATION = 0,
  RFH_CAN_COMMIT_URL_BLOCKED = 1,
  RFH_CAN_ACCESS_FILES_OF_PAGE_STATE = 2,
  RFH_SANDBOX_FLAGS = 3,
  RFH_NO_PROXY_TO_PARENT = 4,
  RPH_DESERIALIZATION_FAILED = 5,
  RVH_CAN_ACCESS_FILES_OF_PAGE_STATE = 6,
  RVH_FILE_CHOOSER_PATH = 7,
  RWH_SYNTHETIC_GESTURE = 8,
  RWH_FOCUS = 9,
  RWH_BLUR = 10,
  RWH_SHARED_BITMAP = 11,
  RWH_BAD_ACK_MESSAGE = 12,
  RWHVA_SHARED_MEMORY = 13,
  SERVICE_WORKER_BAD_URL = 14,
  WC_INVALID_FRAME_SOURCE = 15,
  RWHVM_UNEXPECTED_FRAME_TYPE = 16,
  RFPH_DETACH = 17,
  DFH_BAD_EMBEDDER_MESSAGE = 18,
  NC_AUTO_SUBFRAME = 19,
  CSDH_NOT_RECOGNIZED = 20,
  DSMF_OPEN_STORAGE = 21,
  DSMF_LOAD_STORAGE = 22,
  DBMF_INVALID_ORIGIN_ON_OPEN = 23,
  DBMF_DB_NOT_OPEN_ON_MODIFY = 24,
  DBMF_DB_NOT_OPEN_ON_CLOSE = 25,
  DBMF_INVALID_ORIGIN_ON_SQLITE_ERROR = 26,
  RDH_INVALID_PRIORITY = 27,
  RDH_REQUEST_NOT_TRANSFERRING = 28,
  RDH_BAD_DOWNLOAD = 29,
  NMF_NO_PERMISSION_SHOW = 30,
  NMF_NO_PERMISSION_CLOSE = 31,
  NMF_NO_PERMISSION_VERIFY = 32,
  MH_INVALID_MIDI_PORT = 33,
  MH_SYS_EX_PERMISSION = 34,
  ACDH_REGISTER = 35,
  ACDH_UNREGISTER = 36,
  ACDH_SET_SPAWNING = 37,
  ACDH_SELECT_CACHE = 38,
  ACDH_SELECT_CACHE_FOR_WORKER = 39,
  ACDH_SELECT_CACHE_FOR_SHARED_WORKER = 40,
  ACDH_MARK_AS_FOREIGN_ENTRY = 41,
  ACDH_PENDING_REPLY_IN_GET_STATUS = 42,
  ACDH_GET_STATUS = 43,
  ACDH_PENDING_REPLY_IN_START_UPDATE = 44,
  ACDH_START_UPDATE = 45,
  ACDH_PENDING_REPLY_IN_SWAP_CACHE = 46,
  ACDH_SWAP_CACHE = 47,
  SWDH_NOT_HANDLED = 48,
  SWDH_REGISTER_BAD_URL = 49,
  SWDH_REGISTER_NO_HOST = 50,
  SWDH_REGISTER_CANNOT = 51,
  SWDH_UNREGISTER_BAD_URL = 52,
  SWDH_UNREGISTER_NO_HOST = 53,
  SWDH_UNREGISTER_CANNOT = 54,
  SWDH_GET_REGISTRATION_BAD_URL = 55,
  SWDH_GET_REGISTRATION_NO_HOST = 56,
  SWDH_GET_REGISTRATION_CANNOT = 57,
  SWDH_GET_REGISTRATION_FOR_READY_NO_HOST = 58,
  SWDH_GET_REGISTRATION_FOR_READY_ALREADY_IN_PROGRESS = 59,
  SWDH_POST_MESSAGE = 60,
  SWDH_PROVIDER_CREATED_NO_HOST = 61,
  SWDH_PROVIDER_DESTROYED_NO_HOST = 62,
  SWDH_SET_HOSTED_VERSION_NO_HOST = 63,
  SWDH_SET_HOSTED_VERSION = 64,
  SWDH_WORKER_SCRIPT_LOAD_NO_HOST = 65,
  SWDH_INCREMENT_WORKER_BAD_HANDLE = 66,
  SWDH_DECREMENT_WORKER_BAD_HANDLE = 67,
  SWDH_INCREMENT_REGISTRATION_BAD_HANDLE = 68,
  SWDH_DECREMENT_REGISTRATION_BAD_HANDLE = 69,
  SWDH_TERMINATE_BAD_HANDLE = 70,
  FAMF_APPEND_ITEM_TO_BLOB = 71,
  FAMF_APPEND_SHARED_MEMORY_TO_BLOB = 72,
  FAMF_MALFORMED_STREAM_URL = 73,
  FAMF_APPEND_ITEM_TO_STREAM = 74,
  FAMF_APPEND_SHARED_MEMORY_TO_STREAM = 75,
  IDBDH_CAN_READ_FILE = 76,
  IDBDH_GET_OR_TERMINATE = 77,
  RMF_SET_COOKIE_BAD_ORIGIN = 78,
  RMF_GET_COOKIES_BAD_ORIGIN = 79,
  SWDH_GET_REGISTRATIONS_NO_HOST = 80,
  SWDH_GET_REGISTRATIONS_INVALID_ORIGIN = 81,
  ARH_UNAUTHORIZED_URL = 82,
  BDH_INVALID_SERVICE_ID = 83,
  RFH_COMMIT_DESERIALIZATION_FAILED = 84,
  BDH_INVALID_CHARACTERISTIC_ID = 85,
  SWDH_UPDATE_NO_HOST = 86,
  SWDH_UPDATE_BAD_REGISTRATION_ID = 87,
  SWDH_UPDATE_CANNOT = 88,
  SWDH_UNREGISTER_BAD_REGISTRATION_ID = 89,
  BDH_INVALID_WRITE_VALUE_LENGTH = 90,

  // Please add new elements here. The naming convention is abbreviated class
  // name (e.g. RenderFrameHost becomes RFH) plus a unique description of the
  // reason.
  BAD_MESSAGE_MAX
};

// Called when the browser receives a bad IPC message from a renderer process on
// the UI thread. Logs the event, records a histogram metric for the |reason|,
// and terminates the process for |host|.
void ReceivedBadMessage(RenderProcessHost* host, BadMessageReason reason);

// Called when a browser message filter receives a bad IPC message from a
// renderer or other child process. Logs the event, records a histogram metric
// for the |reason|, and terminates the process for |filter|.
void ReceivedBadMessage(BrowserMessageFilter* filter, BadMessageReason reason);

}  // namespace bad_message
}  // namespace content

#endif  // CONTENT_BROWSER_BAD_MESSAGE_H_
