// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define MEDIA_RENDERERS_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"

template <class T>
class scoped_refptr;

namespace base {
class SharedMemory;
}

namespace media {

class MockGpuVideoAcceleratorFactories : public GpuVideoAcceleratorFactories {
 public:
  MockGpuVideoAcceleratorFactories();

  bool IsGpuVideoAcceleratorEnabled() override;
  // CreateVideo{Decode,Encode}Accelerator returns scoped_ptr, which the mocking
  // framework does not want.  Trampoline them.
  MOCK_METHOD0(DoCreateVideoDecodeAccelerator, VideoDecodeAccelerator*());
  MOCK_METHOD0(DoCreateVideoEncodeAccelerator, VideoEncodeAccelerator*());

  MOCK_METHOD5(CreateTextures,
               bool(int32 count,
                    const gfx::Size& size,
                    std::vector<uint32>* texture_ids,
                    std::vector<gpu::Mailbox>* texture_mailboxes,
                    uint32 texture_target));
  MOCK_METHOD1(DeleteTexture, void(uint32 texture_id));
  MOCK_METHOD1(WaitSyncPoint, void(uint32 sync_point));
  MOCK_METHOD0(GetTaskRunner, scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(GetVideoDecodeAcceleratorSupportedProfiles,
               VideoDecodeAccelerator::SupportedProfiles());
  MOCK_METHOD0(GetVideoEncodeAcceleratorSupportedProfiles,
               VideoEncodeAccelerator::SupportedProfiles());

  scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      gfx::GpuMemoryBuffer::Usage usage) override;

  unsigned ImageTextureTarget() override;
  MOCK_METHOD0(IsTextureRGSupported, bool());
  MOCK_METHOD0(GetGLES2Interface, gpu::gles2::GLES2Interface*());

  scoped_ptr<base::SharedMemory> CreateSharedMemory(size_t size) override;

  scoped_ptr<VideoDecodeAccelerator> CreateVideoDecodeAccelerator() override;

  scoped_ptr<VideoEncodeAccelerator> CreateVideoEncodeAccelerator() override;

 private:
  ~MockGpuVideoAcceleratorFactories() override;

  DISALLOW_COPY_AND_ASSIGN(MockGpuVideoAcceleratorFactories);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
