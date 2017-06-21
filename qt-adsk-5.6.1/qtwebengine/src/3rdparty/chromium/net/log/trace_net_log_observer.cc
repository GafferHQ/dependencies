// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/trace_net_log_observer.h"

#include <stdio.h>

#include <string>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/log/net_log.h"

namespace net {

namespace {

// TraceLog category for NetLog events.
const char kNetLogTracingCategory[] = "netlog";

class TracedValue : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit TracedValue(scoped_ptr<base::Value> value) : value_(value.Pass()) {}

 private:
  ~TracedValue() override {}

  void AppendAsTraceFormat(std::string* out) const override {
    if (value_) {
      std::string tmp;
      base::JSONWriter::Write(*value_, &tmp);
      *out += tmp;
    } else {
      *out += "\"\"";
    }
  }

 private:
  scoped_ptr<base::Value> value_;
};

}  // namespace

TraceNetLogObserver::TraceNetLogObserver() : net_log_to_watch_(NULL) {
}

TraceNetLogObserver::~TraceNetLogObserver() {
  DCHECK(!net_log_to_watch_);
  DCHECK(!net_log());
}

void TraceNetLogObserver::OnAddEntry(const NetLog::Entry& entry) {
  scoped_ptr<base::Value> params(entry.ParametersToValue());
  switch (entry.phase()) {
    case NetLog::PHASE_BEGIN:
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
          kNetLogTracingCategory, NetLog::EventTypeToString(entry.type()),
          entry.source().id, "source_type",
          NetLog::SourceTypeToString(entry.source().type), "params",
          scoped_refptr<base::trace_event::ConvertableToTraceFormat>(
              new TracedValue(params.Pass())));
      break;
    case NetLog::PHASE_END:
      TRACE_EVENT_NESTABLE_ASYNC_END2(
          kNetLogTracingCategory, NetLog::EventTypeToString(entry.type()),
          entry.source().id, "source_type",
          NetLog::SourceTypeToString(entry.source().type), "params",
          scoped_refptr<base::trace_event::ConvertableToTraceFormat>(
              new TracedValue(params.Pass())));
      break;
    case NetLog::PHASE_NONE:
      TRACE_EVENT_NESTABLE_ASYNC_INSTANT2(
          kNetLogTracingCategory, NetLog::EventTypeToString(entry.type()),
          entry.source().id, "source_type",
          NetLog::SourceTypeToString(entry.source().type), "params",
          scoped_refptr<base::trace_event::ConvertableToTraceFormat>(
              new TracedValue(params.Pass())));
      break;
  }
}

void TraceNetLogObserver::WatchForTraceStart(NetLog* netlog) {
  DCHECK(!net_log_to_watch_);
  DCHECK(!net_log());
  net_log_to_watch_ = netlog;
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
}

void TraceNetLogObserver::StopWatchForTraceStart() {
  // Should only stop if is currently watching.
  DCHECK(net_log_to_watch_);
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
  if (net_log())
    net_log()->DeprecatedRemoveObserver(this);
  net_log_to_watch_ = NULL;
}

void TraceNetLogObserver::OnTraceLogEnabled() {
  net_log_to_watch_->DeprecatedAddObserver(this, NetLogCaptureMode::Default());
}

void TraceNetLogObserver::OnTraceLogDisabled() {
  if (net_log())
    net_log()->DeprecatedRemoveObserver(this);
}

}  // namespace net
