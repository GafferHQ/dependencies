// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_PROCESS_MEMORY_DUMP_IMPL_H_
#define CONTENT_CHILD_WEB_PROCESS_MEMORY_DUMP_IMPL_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebProcessMemoryDump.h"

namespace base {
namespace trace_event {
class MemoryAllocatorDump;
class ProcessMemoryDump;
}  // namespace base
}  // namespace trace_event

namespace content {

class WebMemoryAllocatorDumpImpl;

// Implements the blink::WebProcessMemoryDump interface by means of proxying the
// calls to createMemoryAllocatorDump() to the underlying
// base::trace_event::ProcessMemoryDump instance.
class CONTENT_EXPORT WebProcessMemoryDumpImpl final
    : public NON_EXPORTED_BASE(blink::WebProcessMemoryDump) {
 public:
  // Creates a standalone WebProcessMemoryDumpImp, which owns the underlying
  // ProcessMemoryDump.
  WebProcessMemoryDumpImpl();

  // Wraps (without owning) an existing ProcessMemoryDump.
  explicit WebProcessMemoryDumpImpl(
      base::trace_event::ProcessMemoryDump* process_memory_dump);

  virtual ~WebProcessMemoryDumpImpl();

  // blink::WebProcessMemoryDump implementation.
  virtual blink::WebMemoryAllocatorDump* createMemoryAllocatorDump(
      const blink::WebString& absolute_name);
  virtual blink::WebMemoryAllocatorDump* createMemoryAllocatorDump(
      const blink::WebString& absolute_name,
      blink::WebMemoryAllocatorDumpGuid guid);
  virtual blink::WebMemoryAllocatorDump* getMemoryAllocatorDump(
      const blink::WebString& absolute_name) const;
  virtual void clear();
  virtual void takeAllDumpsFrom(blink::WebProcessMemoryDump* other);
  virtual void AddOwnershipEdge(blink::WebMemoryAllocatorDumpGuid source,
                                blink::WebMemoryAllocatorDumpGuid target,
                                int importance);
  virtual void AddOwnershipEdge(blink::WebMemoryAllocatorDumpGuid source,
                                blink::WebMemoryAllocatorDumpGuid target);

  const base::trace_event::ProcessMemoryDump* process_memory_dump() const {
    return process_memory_dump_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WebProcessMemoryDumpImplTest, IntegrationTest);

  blink::WebMemoryAllocatorDump* createWebMemoryAllocatorDump(
      base::trace_event::MemoryAllocatorDump* memory_allocator_dump);

  // Only for the case of ProcessMemoryDump being owned (i.e. the default ctor).
  scoped_ptr<base::trace_event::ProcessMemoryDump> owned_process_memory_dump_;

  // The underlying ProcessMemoryDump instance to which the
  // createMemoryAllocatorDump() calls will be proxied to.
  base::trace_event::ProcessMemoryDump* process_memory_dump_;  // Not owned.

  // Reverse index of MemoryAllocatorDump -> WebMemoryAllocatorDumpImpl wrapper.
  // By design WebMemoryDumpProvider(s) are not supposed to hold the pointer
  // to the WebProcessMemoryDump passed as argument of the onMemoryDump() call.
  // Those pointers are valid only within the scope of the call and can be
  // safely torn down once the WebProcessMemoryDumpImpl itself is destroyed.
  base::ScopedPtrHashMap<base::trace_event::MemoryAllocatorDump*,
                         scoped_ptr<WebMemoryAllocatorDumpImpl>>
      memory_allocator_dumps_;

  DISALLOW_COPY_AND_ASSIGN(WebProcessMemoryDumpImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_PROCESS_MEMORY_DUMP_IMPL_H_
