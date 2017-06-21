// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_TRACING_TRACE_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// This class sends and receives trace messages on the browser process.
// See also: tracing_controller.h
// See also: child_trace_message_filter.h
class TraceMessageFilter : public BrowserMessageFilter {
 public:
  explicit TraceMessageFilter(int child_process_id);

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void SendBeginTracing(
      const base::trace_event::TraceConfig& trace_config);
  void SendEndTracing();
  void SendCancelTracing();
  void SendEnableMonitoring(
      const base::trace_event::TraceConfig& trace_config);
  void SendDisableMonitoring();
  void SendCaptureMonitoringSnapshot();
  void SendGetTraceLogStatus();
  void SendSetWatchEvent(const std::string& category_name,
                         const std::string& event_name);
  void SendCancelWatchEvent();
  void SendProcessMemoryDumpRequest(
      const base::trace_event::MemoryDumpRequestArgs& args);

 protected:
  ~TraceMessageFilter() override;

 private:
  // Message handlers.
  void OnChildSupportsTracing();
  void OnEndTracingAck(const std::vector<std::string>& known_categories);
  void OnCaptureMonitoringSnapshotAcked();
  void OnWatchEventMatched();
  void OnTraceLogStatusReply(const base::trace_event::TraceLogStatus& status);
  void OnTraceDataCollected(const std::string& data);
  void OnMonitoringTraceDataCollected(const std::string& data);
  void OnGlobalMemoryDumpRequest(
      const base::trace_event::MemoryDumpRequestArgs& args);
  void OnProcessMemoryDumpResponse(uint64 dump_guid, bool success);

  void SendGlobalMemoryDumpResponse(uint64 dump_guid, bool success);

  // ChildTraceMessageFilter exists:
  bool has_child_;

  // Hash of id of the child process.
  uint64 tracing_process_id_;

  // Awaiting ack for previously sent SendEndTracing
  bool is_awaiting_end_ack_;
  // Awaiting ack for previously sent SendCaptureMonitoringSnapshot
  bool is_awaiting_capture_monitoring_snapshot_ack_;
  // Awaiting ack for previously sent SendGetTraceLogStatus
  bool is_awaiting_buffer_percent_full_ack_;

  DISALLOW_COPY_AND_ASSIGN(TraceMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACE_MESSAGE_FILTER_H_
