// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/tracing_handler.h"

#include <cmath>

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event_impl.h"

namespace content {
namespace devtools {
namespace tracing {

typedef DevToolsProtocolClient::Response Response;

namespace {

const double kMinimumReportingInterval = 250.0;

class DevToolsTraceSinkProxy : public TracingController::TraceDataSink {
 public:
  explicit DevToolsTraceSinkProxy(base::WeakPtr<TracingHandler> handler)
      : tracing_handler_(handler) {}

  void AddTraceChunk(const std::string& chunk) override {
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceDataCollected(chunk);
  }
  void Close() override {
    if (TracingHandler* h = tracing_handler_.get())
      h->OnTraceComplete();
  }

 private:
  ~DevToolsTraceSinkProxy() override {}

  base::WeakPtr<TracingHandler> tracing_handler_;
};

}  // namespace

TracingHandler::TracingHandler(TracingHandler::Target target)
    : target_(target),
      did_initiate_recording_(false),
      weak_factory_(this) {
}

TracingHandler::~TracingHandler() {
}

void TracingHandler::SetClient(scoped_ptr<Client> client) {
  client_.swap(client);
}

void TracingHandler::Detached() {
  if (IsRecording())
    DisableRecording(true);
}

void TracingHandler::OnTraceDataCollected(const std::string& trace_fragment) {
  // Hand-craft protocol notification message so we can substitute JSON
  // that we already got as string as a bare object, not a quoted string.
  std::string message(
      "{ \"method\": \"Tracing.dataCollected\", \"params\": { \"value\": [");
  const size_t messageSuffixSize = 10;
  message.reserve(message.size() + trace_fragment.size() + messageSuffixSize);
  message += trace_fragment;
  message += "] } }";
  client_->SendRawMessage(message);
}

void TracingHandler::OnTraceComplete() {
  client_->TracingComplete(TracingCompleteParams::Create());
}

Response TracingHandler::Start(DevToolsCommandId command_id,
                               const std::string* categories,
                               const std::string* options,
                               const double* buffer_usage_reporting_interval) {
  if (IsRecording())
    return Response::InternalError("Tracing is already started");

  did_initiate_recording_ = true;

  base::trace_event::TraceConfig trace_config(
      categories ? *categories : std::string(),
      options ? *options : std::string());
  if (buffer_usage_reporting_interval)
    SetupTimer(*buffer_usage_reporting_interval);

  // If inspected target is a render process Tracing.start will be handled by
  // tracing agent in the renderer.
  if (target_ == Renderer) {
    TracingController::GetInstance()->EnableRecording(
        trace_config,
        TracingController::EnableRecordingDoneCallback());
    return Response::FallThrough();
  }

  TracingController::GetInstance()->EnableRecording(
      trace_config,
      base::Bind(&TracingHandler::OnRecordingEnabled,
                 weak_factory_.GetWeakPtr(),
                 command_id));
  return Response::OK();
}

Response TracingHandler::End(DevToolsCommandId command_id) {
  if (!IsRecording())
    return Response::InternalError("Tracing is not started");

  DisableRecording(false);
  // If inspected target is a render process Tracing.end will be handled by
  // tracing agent in the renderer.
  return target_ == Renderer ? Response::FallThrough() : Response::OK();
}

Response TracingHandler::GetCategories(DevToolsCommandId command_id) {
  TracingController::GetInstance()->GetCategories(
      base::Bind(&TracingHandler::OnCategoriesReceived,
                 weak_factory_.GetWeakPtr(),
                 command_id));
  return Response::OK();
}

void TracingHandler::OnRecordingEnabled(DevToolsCommandId command_id) {
  client_->SendStartResponse(command_id, StartResponse::Create());
}

void TracingHandler::OnBufferUsage(float percent_full,
                                   size_t approximate_event_count) {
  // TODO(crbug426117): remove set_value once all clients have switched to
  // the new interface of the event.
  client_->BufferUsage(BufferUsageParams::Create()
                           ->set_value(percent_full)
                           ->set_percent_full(percent_full)
                           ->set_event_count(approximate_event_count));
}

void TracingHandler::OnCategoriesReceived(
    DevToolsCommandId command_id,
    const std::set<std::string>& category_set) {
  std::vector<std::string> categories;
  for (const std::string& category : category_set)
    categories.push_back(category);
  client_->SendGetCategoriesResponse(command_id,
      GetCategoriesResponse::Create()->set_categories(categories));
}

void TracingHandler::SetupTimer(double usage_reporting_interval) {
  if (usage_reporting_interval == 0) return;

  if (usage_reporting_interval < kMinimumReportingInterval)
      usage_reporting_interval = kMinimumReportingInterval;

  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(
      std::ceil(usage_reporting_interval));
  buffer_usage_poll_timer_.reset(new base::Timer(
      FROM_HERE, interval,
      base::Bind(base::IgnoreResult(&TracingController::GetTraceBufferUsage),
                 base::Unretained(TracingController::GetInstance()),
                 base::Bind(&TracingHandler::OnBufferUsage,
                            weak_factory_.GetWeakPtr())),
      true));
  buffer_usage_poll_timer_->Reset();
}

void TracingHandler::DisableRecording(bool abort) {
  buffer_usage_poll_timer_.reset();
  TracingController::GetInstance()->DisableRecording(
      abort ? nullptr : new DevToolsTraceSinkProxy(weak_factory_.GetWeakPtr()));
  did_initiate_recording_ = false;
}

bool TracingHandler::IsRecording() const {
  return TracingController::GetInstance()->IsRecording();
}

}  // namespace tracing
}  // namespace devtools
}  // namespace content
