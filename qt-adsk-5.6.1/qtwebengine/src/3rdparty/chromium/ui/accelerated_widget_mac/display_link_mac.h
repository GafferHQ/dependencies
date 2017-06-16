// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_DISPLAY_LINK_MAC_H_
#define UI_ACCELERATED_WIDGET_MAC_DISPLAY_LINK_MAC_H_

#include <QuartzCore/CVDisplayLink.h>
#include <map>

#include "base/lazy_instance.h"
#include "base/mac/scoped_typeref.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"

namespace ui {

class ACCELERATED_WIDGET_MAC_EXPORT DisplayLinkMac :
    public base::RefCounted<DisplayLinkMac> {
 public:
  static scoped_refptr<DisplayLinkMac> GetForDisplay(
      CGDirectDisplayID display_id);

  // Get vsync scheduling parameters.
  bool GetVSyncParameters(
      base::TimeTicks* timebase,
      base::TimeDelta* interval);

 private:
  friend class base::RefCounted<DisplayLinkMac>;

  DisplayLinkMac(
      CGDirectDisplayID display_id,
      base::ScopedTypeRef<CVDisplayLinkRef> display_link);
  virtual ~DisplayLinkMac();

  void StartOrContinueDisplayLink();
  void StopDisplayLink();
  void Tick(const CVTimeStamp& time);

  // Called by the system on the display link thread, and posts a call to Tick
  // to the UI thread.
  static CVReturn DisplayLinkCallback(
      CVDisplayLinkRef display_link,
      const CVTimeStamp* now,
      const CVTimeStamp* output_time,
      CVOptionFlags flags_in,
      CVOptionFlags* flags_out,
      void* context);

  // This is called whenever the display is reconfigured, and marks that the
  // vsync parameters must be recalculated.
  static void DisplayReconfigurationCallBack(
      CGDirectDisplayID display,
      CGDisplayChangeSummaryFlags flags,
      void* user_info);

  // The task runner to post tasks to from the display link thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // The display that this display link is attached to.
  CGDirectDisplayID display_id_;

  // CVDisplayLink for querying VSync timing info.
  base::ScopedTypeRef<CVDisplayLinkRef> display_link_;

  // VSync parameters computed during Tick.
  bool timebase_and_interval_valid_;
  base::TimeTicks timebase_;
  base::TimeDelta interval_;

  // Each display link instance consumes a non-negligible number of cycles, so
  // make all display links on the same screen share the same object.
  typedef std::map<CGDirectDisplayID, DisplayLinkMac*> DisplayMap;
  static base::LazyInstance<DisplayMap> display_map_;
};

}  // ui

#endif  // UI_ACCELERATED_WIDGET_MAC_DISPLAY_LINK_MAC_H_
