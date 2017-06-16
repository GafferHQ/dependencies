// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_CHILD_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_
#define COMPONENTS_TRACING_CHILD_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_

#include "base/trace_event/memory_dump_manager.h"

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace tracing {

class ChildTraceMessageFilter;

// This class is a simple proxy class between the MemoryDumpManager and the
// ChildTraceMessageFilter. It's only purpose is to adapt the lifetime of
// CTMF to the demands of MDM, which expects the delegate to be thread-safe
// and long lived. CTMF, instead, can be torn down during browser shutdown.
// This class is registered as MDM delegate in child processes and handles
// gracefully (and thread-safely) failures in the case of a lack of the CTMF.
class ChildMemoryDumpManagerDelegateImpl
    : public base::trace_event::MemoryDumpManagerDelegate {
 public:
  static ChildMemoryDumpManagerDelegateImpl* GetInstance();

  // base::trace_event::MemoryDumpManagerDelegate implementation.
  void RequestGlobalMemoryDump(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const base::trace_event::MemoryDumpCallback& callback) override;
  bool IsCoordinatorProcess() const override;

  void SetChildTraceMessageFilter(ChildTraceMessageFilter* ctmf);

 protected:
  // Make CreateProcessDump() visible to ChildTraceMessageFilter.
  friend class ChildTraceMessageFilter;

 private:
  friend struct DefaultSingletonTraits<ChildMemoryDumpManagerDelegateImpl>;

  ChildMemoryDumpManagerDelegateImpl();
  ~ChildMemoryDumpManagerDelegateImpl() override;

  ChildTraceMessageFilter* ctmf_;  // Not owned.

  // The SingleThreadTaskRunner where the |ctmf_| lives.
  // It is NULL iff |cmtf_| is NULL.
  scoped_refptr<base::SingleThreadTaskRunner> ctmf_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ChildMemoryDumpManagerDelegateImpl);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_CHILD_MEMORY_DUMP_MANAGER_DELEGATE_IMPL_H_
