// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/sync_socket.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event_impl.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START TracingMsgStart

IPC_STRUCT_TRAITS_BEGIN(base::trace_event::TraceLogStatus)
IPC_STRUCT_TRAITS_MEMBER(event_capacity)
IPC_STRUCT_TRAITS_MEMBER(event_count)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(base::trace_event::MemoryDumpRequestArgs)
IPC_STRUCT_TRAITS_MEMBER(dump_guid)
IPC_STRUCT_TRAITS_MEMBER(dump_type)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(
    base::trace_event::MemoryDumpType,
    static_cast<int>(base::trace_event::MemoryDumpType::LAST))

// Sent to all child processes to enable trace event recording.
IPC_MESSAGE_CONTROL3(TracingMsg_BeginTracing,
                     std::string /*  trace_config_str */,
                     base::TraceTicks /* browser_time */,
                     uint64 /* Tracing process id (hash of child id) */)

// Sent to all child processes to disable trace event recording.
IPC_MESSAGE_CONTROL0(TracingMsg_EndTracing)

// Sent to all child processes to cancel trace event recording.
IPC_MESSAGE_CONTROL0(TracingMsg_CancelTracing)

// Sent to all child processes to start monitoring.
IPC_MESSAGE_CONTROL2(TracingMsg_EnableMonitoring,
                     std::string /*  trace_config_str */,
                     base::TraceTicks /* browser_time */)

// Sent to all child processes to stop monitoring.
IPC_MESSAGE_CONTROL0(TracingMsg_DisableMonitoring)

// Sent to all child processes to capture the current monitorint snapshot.
IPC_MESSAGE_CONTROL0(TracingMsg_CaptureMonitoringSnapshot)

// Sent to all child processes to get trace buffer fullness.
IPC_MESSAGE_CONTROL0(TracingMsg_GetTraceLogStatus)

// Sent to all child processes to set watch event.
IPC_MESSAGE_CONTROL2(TracingMsg_SetWatchEvent,
                     std::string /* category_name */,
                     std::string /* event_name */)

// Sent to all child processes to clear watch event.
IPC_MESSAGE_CONTROL0(TracingMsg_CancelWatchEvent)

// Sent to all child processes to request a local (current process) memory dump.
IPC_MESSAGE_CONTROL1(TracingMsg_ProcessMemoryDumpRequest,
                     base::trace_event::MemoryDumpRequestArgs)

// Reply to TracingHostMsg_GlobalMemoryDumpRequest, sent by the browser process.
// This is to get the result of a global dump initiated by a child process.
IPC_MESSAGE_CONTROL2(TracingMsg_GlobalMemoryDumpResponse,
                     uint64 /* dump_guid */,
                     bool /* success */)

// Sent everytime when a watch event is matched.
IPC_MESSAGE_CONTROL0(TracingHostMsg_WatchEventMatched)

// Notify the browser that this child process supports tracing.
IPC_MESSAGE_CONTROL0(TracingHostMsg_ChildSupportsTracing)

// Reply from child processes acking TracingMsg_EndTracing.
IPC_MESSAGE_CONTROL1(TracingHostMsg_EndTracingAck,
                     std::vector<std::string> /* known_categories */)

// Reply from child processes acking TracingMsg_CaptureMonitoringSnapshot.
IPC_MESSAGE_CONTROL0(TracingHostMsg_CaptureMonitoringSnapshotAck)

// Child processes send back trace data in JSON chunks.
IPC_MESSAGE_CONTROL1(TracingHostMsg_TraceDataCollected,
                     std::string /*json trace data*/)

// Child processes send back trace data of the current monitoring
// in JSON chunks.
IPC_MESSAGE_CONTROL1(TracingHostMsg_MonitoringTraceDataCollected,
                     std::string /*json trace data*/)

// Reply to TracingMsg_GetTraceLogStatus.
IPC_MESSAGE_CONTROL1(
    TracingHostMsg_TraceLogStatusReply,
    base::trace_event::TraceLogStatus /*status of the trace log*/)

// Sent to the browser to initiate a global memory dump from a child process.
IPC_MESSAGE_CONTROL1(TracingHostMsg_GlobalMemoryDumpRequest,
                     base::trace_event::MemoryDumpRequestArgs)

// Reply to TracingMsg_ProcessMemoryDumpRequest.
IPC_MESSAGE_CONTROL2(TracingHostMsg_ProcessMemoryDumpResponse,
                     uint64 /* dump_guid */,
                     bool /* success */)
