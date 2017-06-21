// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define MEDIA_RENDERERS_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/media_export.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class SingleThreadTaskRunner;
class SharedMemory;
}

namespace gfx {
class Rect;
class Size;
}

namespace media {

class VideoDecodeAccelerator;

// Helper interface for specifying factories needed to instantiate a hardware
// video accelerator.
// Threading model:
// * The GpuVideoAcceleratorFactories may be constructed on any thread.
// * The GpuVideoAcceleratorFactories has an associated message loop, which may
//   be retrieved as |GetMessageLoop()|.
// * All calls to the Factories after construction must be made on its message
//   loop.
class MEDIA_EXPORT GpuVideoAcceleratorFactories
    : public base::RefCountedThreadSafe<GpuVideoAcceleratorFactories> {
 public:
  // Return whether GPU encoding/decoding is enabled.
  virtual bool IsGpuVideoAcceleratorEnabled() = 0;
  // Caller owns returned pointer, but should call Destroy() on it (instead of
  // directly deleting) for proper destruction, as per the
  // VideoDecodeAccelerator interface.
  virtual scoped_ptr<VideoDecodeAccelerator> CreateVideoDecodeAccelerator() = 0;

  // Caller owns returned pointer, but should call Destroy() on it (instead of
  // directly deleting) for proper destruction, as per the
  // VideoEncodeAccelerator interface.
  virtual scoped_ptr<VideoEncodeAccelerator> CreateVideoEncodeAccelerator() = 0;

  // Allocate & delete native textures.
  virtual bool CreateTextures(int32 count,
                              const gfx::Size& size,
                              std::vector<uint32>* texture_ids,
                              std::vector<gpu::Mailbox>* texture_mailboxes,
                              uint32 texture_target) = 0;
  virtual void DeleteTexture(uint32 texture_id) = 0;

  virtual void WaitSyncPoint(uint32 sync_point) = 0;

  virtual scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage) = 0;

  virtual unsigned ImageTextureTarget() = 0;
  virtual bool IsTextureRGSupported() = 0;

  virtual gpu::gles2::GLES2Interface* GetGLES2Interface() = 0;

  // Allocate & return a shared memory segment.
  virtual scoped_ptr<base::SharedMemory> CreateSharedMemory(size_t size) = 0;

  // Returns the task runner the video accelerator runs on.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() = 0;

  // Returns the supported codec profiles of video decode accelerator.
  virtual VideoDecodeAccelerator::SupportedProfiles
      GetVideoDecodeAcceleratorSupportedProfiles() = 0;

  // Returns the supported codec profiles of video encode accelerator.
  virtual VideoEncodeAccelerator::SupportedProfiles
      GetVideoEncodeAcceleratorSupportedProfiles() = 0;

 protected:
  friend class base::RefCountedThreadSafe<GpuVideoAcceleratorFactories>;
  virtual ~GpuVideoAcceleratorFactories() {}
};

}  // namespace media

#endif  // MEDIA_RENDERERS_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
