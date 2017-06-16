// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_request_tracker_impl.h"

namespace mojo {
namespace test {

TrackingContext::TrackingContext() : next_id(1) {
}

TrackingContext::~TrackingContext() {
}

TestRequestTrackerImpl::TestRequestTrackerImpl(
    InterfaceRequest<TestRequestTracker> request,
    TrackingContext* context)
    : context_(context), binding_(this, request.Pass()), weak_factory_(this) {
}

TestRequestTrackerImpl::~TestRequestTrackerImpl() {
}

void TestRequestTrackerImpl::RecordStats(
      uint64_t client_id,
      ServiceStatsPtr stats) {
  assert(context_->ids_to_names.find(client_id) !=
         context_->ids_to_names.end());
  context_->records[client_id].push_back(*stats);
}

void TestRequestTrackerImpl::SetNameAndReturnId(
    const String& service_name,
    const Callback<void(uint64_t id)>& callback) {
  uint64_t id = context_->next_id++;
  callback.Run(id);
  DCHECK(context_->ids_to_names.find(id) == context_->ids_to_names.end());
  context_->ids_to_names[id] = service_name;
}

TestTrackedRequestServiceImpl::TestTrackedRequestServiceImpl(
    InterfaceRequest<TestTrackedRequestService> request,
    TrackingContext* context)
    : context_(context), binding_(this, request.Pass()) {
}

TestTrackedRequestServiceImpl::~TestTrackedRequestServiceImpl() {
}

void TestTrackedRequestServiceImpl::GetReport(
    const mojo::Callback<void(mojo::Array<ServiceReportPtr>)>& callback) {
  mojo::Array<ServiceReportPtr> reports;
  for (AllRecordsMap::const_iterator it1 = context_->records.begin();
       it1 != context_->records.end(); ++it1) {
    ServiceReportPtr report(ServiceReport::New());
    report->service_name = context_->ids_to_names[it1->first];
    double mean_health_numerator = 0;
    size_t num_samples = it1->second.size();
    if (num_samples == 0)
      continue;

    for (std::vector<ServiceStats>::const_iterator it2 = it1->second.begin();
         it2 != it1->second.end(); ++it2) {
      report->total_requests += it2->num_new_requests;
      mean_health_numerator += it2->health;
    }
    report->mean_health = mean_health_numerator / num_samples;
    reports.push_back(report.Pass());
  }
  callback.Run(reports.Pass());
}

}  // namespace test
}  // namespace mojo
