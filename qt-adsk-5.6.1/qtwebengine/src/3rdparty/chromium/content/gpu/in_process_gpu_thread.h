// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_IN_PROCESS_GPU_THREAD_H_
#define CONTENT_GPU_IN_PROCESS_GPU_THREAD_H_

#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/common/in_process_child_thread_params.h"

namespace content {

class GpuMemoryBufferFactory;
class GpuProcess;

// This class creates a GPU thread (instead of a GPU process), when running
// with --in-process-gpu or --single-process.
class InProcessGpuThread : public base::Thread {
 public:
  InProcessGpuThread(const InProcessChildThreadParams& params);
  ~InProcessGpuThread() override;

 protected:
  void Init() override;
  void CleanUp() override;

 private:
  InProcessChildThreadParams params_;

  // Deleted in CleanUp() on the gpu thread, so don't use smart pointers.
  GpuProcess* gpu_process_;

  scoped_ptr<GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  DISALLOW_COPY_AND_ASSIGN(InProcessGpuThread);
};

CONTENT_EXPORT base::Thread* CreateInProcessGpuThread(
    const InProcessChildThreadParams& params);

}  // namespace content

#endif  // CONTENT_GPU_IN_PROCESS_GPU_THREAD_H_
