// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/display_link_mac.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/trace_event/trace_event.h"

namespace base {

template<>
struct ScopedTypeRefTraits<CVDisplayLinkRef> {
  static void Retain(CVDisplayLinkRef object) {
    CVDisplayLinkRetain(object);
  }
  static void Release(CVDisplayLinkRef object) {
    CVDisplayLinkRelease(object);
  }
};

}  // namespace base

namespace ui {

// static
scoped_refptr<DisplayLinkMac> DisplayLinkMac::GetForDisplay(
    CGDirectDisplayID display_id) {
  // Return the existing display link for this display, if it exists.
  DisplayMap::iterator found = display_map_.Get().find(display_id);
  if (found != display_map_.Get().end()) {
    return found->second;
  }

  CVReturn ret = kCVReturnSuccess;

  base::ScopedTypeRef<CVDisplayLinkRef> display_link;
  ret = CVDisplayLinkCreateWithCGDisplay(
      display_id,
      display_link.InitializeInto());
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkCreateWithActiveCGDisplays failed: " << ret;
    return NULL;
  }

  scoped_refptr<DisplayLinkMac> display_link_mac;
  display_link_mac = new DisplayLinkMac(display_id, display_link);
  ret = CVDisplayLinkSetOutputCallback(
      display_link_mac->display_link_,
      &DisplayLinkCallback,
      display_link_mac.get());
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkSetOutputCallback failed: " << ret;
    return NULL;
  }

  return display_link_mac;
}

DisplayLinkMac::DisplayLinkMac(
    CGDirectDisplayID display_id,
    base::ScopedTypeRef<CVDisplayLinkRef> display_link)
      : main_thread_task_runner_(
            base::MessageLoop::current()->task_runner()),
        display_id_(display_id),
        display_link_(display_link),
        timebase_and_interval_valid_(false) {
  DCHECK(display_map_.Get().find(display_id) == display_map_.Get().end());
  if (display_map_.Get().empty()) {
    CGError register_error = CGDisplayRegisterReconfigurationCallback(
        DisplayReconfigurationCallBack, nullptr);
    DPLOG_IF(ERROR, register_error != kCGErrorSuccess)
        << "CGDisplayRegisterReconfigurationCallback: "
        << register_error;
  }
  display_map_.Get().insert(std::make_pair(display_id_, this));
}

DisplayLinkMac::~DisplayLinkMac() {
  StopDisplayLink();

  DisplayMap::iterator found = display_map_.Get().find(display_id_);
  DCHECK(found != display_map_.Get().end());
  DCHECK(found->second == this);
  display_map_.Get().erase(found);
  if (display_map_.Get().empty()) {
    CGError remove_error = CGDisplayRemoveReconfigurationCallback(
        DisplayReconfigurationCallBack, nullptr);
    DPLOG_IF(ERROR, remove_error != kCGErrorSuccess)
        << "CGDisplayRemoveReconfigurationCallback: "
        << remove_error;
  }
}

bool DisplayLinkMac::GetVSyncParameters(
    base::TimeTicks* timebase, base::TimeDelta* interval) {
  if (!timebase_and_interval_valid_) {
    StartOrContinueDisplayLink();
    return false;
  }

  *timebase = timebase_;
  *interval = interval_;
  return true;
}

void DisplayLinkMac::Tick(const CVTimeStamp& cv_time) {
  TRACE_EVENT0("ui", "DisplayLinkMac::Tick");

  // Verify that videoRefreshPeriod is 32 bits.
  DCHECK((cv_time.videoRefreshPeriod & ~0xffffFFFFull) == 0ull);

  // Verify that the numerator and denominator make some sense.
  uint32 numerator = static_cast<uint32>(cv_time.videoRefreshPeriod);
  uint32 denominator = cv_time.videoTimeScale;
  if (numerator <= 0 || denominator <= 0) {
    LOG(WARNING) << "Unexpected numerator or denominator, bailing.";
    return;
  }

  timebase_ = base::TimeTicks::FromInternalValue(
      cv_time.hostTime / 1000);
  interval_ = base::TimeDelta::FromMicroseconds(
      1000000 * static_cast<int64>(numerator) / denominator);
  timebase_and_interval_valid_ = true;

  StopDisplayLink();
}

void DisplayLinkMac::StartOrContinueDisplayLink() {
  if (CVDisplayLinkIsRunning(display_link_))
    return;

  CVReturn ret = CVDisplayLinkStart(display_link_);
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkStart failed: " << ret;
  }
}

void DisplayLinkMac::StopDisplayLink() {
  if (!CVDisplayLinkIsRunning(display_link_))
    return;

  CVReturn ret = CVDisplayLinkStop(display_link_);
  if (ret != kCVReturnSuccess) {
    LOG(ERROR) << "CVDisplayLinkStop failed: " << ret;
  }
}

// static
CVReturn DisplayLinkMac::DisplayLinkCallback(
    CVDisplayLinkRef display_link,
    const CVTimeStamp* now,
    const CVTimeStamp* output_time,
    CVOptionFlags flags_in,
    CVOptionFlags* flags_out,
    void* context) {
  TRACE_EVENT0("ui", "DisplayLinkMac::DisplayLinkCallback");
  DisplayLinkMac* display_link_mac = static_cast<DisplayLinkMac*>(context);
  display_link_mac->main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DisplayLinkMac::Tick, display_link_mac, *output_time));
  return kCVReturnSuccess;
}

// static
void DisplayLinkMac::DisplayReconfigurationCallBack(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags,
    void* user_info) {
  DisplayMap::iterator found = display_map_.Get().find(display);
  if (found == display_map_.Get().end())
    return;
  DisplayLinkMac* display_link_mac = found->second;
  display_link_mac->timebase_and_interval_valid_ = false;
}

// static
base::LazyInstance<DisplayLinkMac::DisplayMap>
    DisplayLinkMac::display_map_ = LAZY_INSTANCE_INITIALIZER;

}  // ui

