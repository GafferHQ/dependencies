// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service/cast_service.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/threading/thread_checker.h"

namespace chromecast {

CastService::CastService(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    metrics::CastMetricsServiceClient* metrics_service_client)
    : browser_context_(browser_context),
      pref_service_(pref_service),
      metrics_service_client_(metrics_service_client),
      stopped_(true),
      thread_checker_(new base::ThreadChecker()) {
}

CastService::~CastService() {
  DCHECK(thread_checker_->CalledOnValidThread());
  DCHECK(stopped_);
}

void CastService::Initialize() {
  DCHECK(thread_checker_->CalledOnValidThread());
  InitializeInternal();
}

void CastService::Finalize() {
  DCHECK(thread_checker_->CalledOnValidThread());
  FinalizeInternal();
}

void CastService::Start() {
  DCHECK(thread_checker_->CalledOnValidThread());
  stopped_ = false;
  StartInternal();
}

void CastService::Stop() {
  DCHECK(thread_checker_->CalledOnValidThread());
  StopInternal();
  // Consume any pending tasks which should be done before destroying in-process
  // renderer process, for example, destroying web_contents.
  base::RunLoop().RunUntilIdle();
  stopped_ = true;
}

}  // namespace chromecast
