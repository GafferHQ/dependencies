// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "cc/test/test_context_provider.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "content/browser/compositor/buffer_queue.h"
#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/common/gpu/client/gl_helper.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::Ne;
using ::testing::Return;

namespace content {

class StubGpuMemoryBufferImpl : public gfx::GpuMemoryBuffer {
 public:
  StubGpuMemoryBufferImpl() {}

  // Overridden from gfx::GpuMemoryBuffer:
  bool Map(void** data) override { return false; }
  void Unmap() override {}
  bool IsMapped() const override { return false; }
  Format GetFormat() const override { return gfx::GpuMemoryBuffer::RGBX_8888; }
  void GetStride(int* stride) const override {}
  gfx::GpuMemoryBufferHandle GetHandle() const override {
    return gfx::GpuMemoryBufferHandle();
  }
  ClientBuffer AsClientBuffer() override {
    return reinterpret_cast<ClientBuffer>(this);
  }
};

class StubBrowserGpuMemoryBufferManager : public BrowserGpuMemoryBufferManager {
 public:
  StubBrowserGpuMemoryBufferManager() : BrowserGpuMemoryBufferManager(1) {}

  scoped_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBufferForScanout(
      const gfx::Size& size,
      gfx::GpuMemoryBuffer::Format format,
      int32 surface_id) override {
    return make_scoped_ptr<gfx::GpuMemoryBuffer>(new StubGpuMemoryBufferImpl);
  }
};

class MockBufferQueue : public BufferQueue {
 public:
  MockBufferQueue(scoped_refptr<cc::ContextProvider> context_provider,
                  BrowserGpuMemoryBufferManager* gpu_memory_buffer_manager,
                  unsigned int internalformat)
      : BufferQueue(context_provider,
                    internalformat,
                    nullptr,
                    gpu_memory_buffer_manager,
                    1) {}
  MOCK_METHOD4(CopyBufferDamage,
               void(int, int, const gfx::Rect&, const gfx::Rect&));
};

class BufferQueueTest : public ::testing::Test {
 public:
  BufferQueueTest() : doublebuffering_(true), first_frame_(true) {}

  void SetUp() override {
    InitWithContext(cc::TestWebGraphicsContext3D::Create());
  }

  void InitWithContext(scoped_ptr<cc::TestWebGraphicsContext3D> context) {
    scoped_refptr<cc::TestContextProvider> context_provider =
        cc::TestContextProvider::Create(context.Pass());
    context_provider->BindToCurrentThread();
    gpu_memory_buffer_manager_.reset(new StubBrowserGpuMemoryBufferManager);
    mock_output_surface_ = new MockBufferQueue(
        context_provider, gpu_memory_buffer_manager_.get(), GL_RGBA);
    output_surface_.reset(mock_output_surface_);
    output_surface_->Initialize();
  }

  unsigned current_surface() { return output_surface_->current_surface_.image; }
  const std::vector<BufferQueue::AllocatedSurface>& available_surfaces() {
    return output_surface_->available_surfaces_;
  }
  const std::deque<BufferQueue::AllocatedSurface>& in_flight_surfaces() {
    return output_surface_->in_flight_surfaces_;
  }

  const BufferQueue::AllocatedSurface& displayed_frame() {
    return output_surface_->displayed_surface_;
  }
  const BufferQueue::AllocatedSurface& current_frame() {
    return output_surface_->current_surface_;
  }
  const BufferQueue::AllocatedSurface& last_frame() {
    return output_surface_->in_flight_surfaces_.back();
  }
  const BufferQueue::AllocatedSurface& next_frame() {
    return output_surface_->available_surfaces_.back();
  }
  const gfx::Size size() { return output_surface_->size_; }

  int CountBuffers() {
    int n = available_surfaces().size() + in_flight_surfaces().size() +
            (displayed_frame().texture ? 1 : 0);
    if (current_surface())
      n++;
    return n;
  }

  // Check that each buffer is unique if present.
  void CheckUnique() {
    std::set<unsigned> buffers;
    EXPECT_TRUE(InsertUnique(&buffers, current_surface()));
    EXPECT_TRUE(InsertUnique(&buffers, displayed_frame().image));
    for (size_t i = 0; i < available_surfaces().size(); i++)
      EXPECT_TRUE(InsertUnique(&buffers, available_surfaces()[i].image));
    for (std::deque<BufferQueue::AllocatedSurface>::const_iterator it =
             in_flight_surfaces().begin();
         it != in_flight_surfaces().end();
         ++it)
      EXPECT_TRUE(InsertUnique(&buffers, it->image));
  }

  void SwapBuffers() {
    output_surface_->SwapBuffers(gfx::Rect(output_surface_->size_));
  }

  void SendDamagedFrame(const gfx::Rect& damage) {
    // We don't care about the GL-level implementation here, just how it uses
    // damage rects.
    output_surface_->BindFramebuffer();
    output_surface_->SwapBuffers(damage);
    if (doublebuffering_ || !first_frame_)
      output_surface_->PageFlipComplete();
    first_frame_ = false;
  }

  void SendFullFrame() { SendDamagedFrame(gfx::Rect(output_surface_->size_)); }

 protected:
  bool InsertUnique(std::set<unsigned>* set, unsigned value) {
    if (!value)
      return true;
    if (set->find(value) != set->end())
      return false;
    set->insert(value);
    return true;
  }

  scoped_ptr<BrowserGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  scoped_ptr<BufferQueue> output_surface_;
  MockBufferQueue* mock_output_surface_;
  bool doublebuffering_;
  bool first_frame_;
};

namespace {
const gfx::Size screen_size = gfx::Size(30, 30);
const gfx::Rect screen_rect = gfx::Rect(screen_size);
const gfx::Rect small_damage = gfx::Rect(gfx::Size(10, 10));
const gfx::Rect large_damage = gfx::Rect(gfx::Size(20, 20));
const gfx::Rect overlapping_damage = gfx::Rect(gfx::Size(5, 20));

GLuint CreateImageDefault() {
  static GLuint id = 0;
  return ++id;
}

class MockedContext : public cc::TestWebGraphicsContext3D {
 public:
  MockedContext() {
    ON_CALL(*this, createImageCHROMIUM(_, _, _, _))
        .WillByDefault(testing::InvokeWithoutArgs(&CreateImageDefault));
  }
  MOCK_METHOD2(bindFramebuffer, void(GLenum, GLuint));
  MOCK_METHOD2(bindTexture, void(GLenum, GLuint));
  MOCK_METHOD2(bindTexImage2DCHROMIUM, void(GLenum, GLint));
  MOCK_METHOD4(createImageCHROMIUM,
               GLuint(ClientBuffer, GLsizei, GLsizei, GLenum));
  MOCK_METHOD1(destroyImageCHROMIUM, void(GLuint));
  MOCK_METHOD5(framebufferTexture2D,
               void(GLenum, GLenum, GLenum, GLuint, GLint));
};

class BufferQueueMockedContextTest : public BufferQueueTest {
 public:
  void SetUp() override {
    context_ = new MockedContext();
    InitWithContext(scoped_ptr<cc::TestWebGraphicsContext3D>(context_));
  }

 protected:
  MockedContext* context_;
};

scoped_ptr<BufferQueue> CreateOutputSurfaceWithMock(
    MockedContext** context,
    BrowserGpuMemoryBufferManager* gpu_memory_buffer_manager) {
  *context = new MockedContext();
  scoped_refptr<cc::TestContextProvider> context_provider =
      cc::TestContextProvider::Create(
          scoped_ptr<cc::TestWebGraphicsContext3D>(*context));
  context_provider->BindToCurrentThread();
  scoped_ptr<BufferQueue> buffer_queue(new BufferQueue(
      context_provider, GL_RGBA, nullptr, gpu_memory_buffer_manager, 1));
  buffer_queue->Initialize();
  return buffer_queue.Pass();
}

TEST(BufferQueueStandaloneTest, FboInitialization) {
  MockedContext* context;
  scoped_ptr<BrowserGpuMemoryBufferManager> gpu_memory_buffer_manager(
      new StubBrowserGpuMemoryBufferManager);
  scoped_ptr<BufferQueue> output_surface =
      CreateOutputSurfaceWithMock(&context, gpu_memory_buffer_manager.get());

  EXPECT_CALL(*context, bindFramebuffer(GL_FRAMEBUFFER, Ne(0U)));
  ON_CALL(*context, framebufferTexture2D(_, _, _, _, _))
      .WillByDefault(Return());

  output_surface->Reshape(gfx::Size(10, 20), 1.0f);
}

TEST(BufferQueueStandaloneTest, FboBinding) {
  MockedContext* context;
  scoped_ptr<BrowserGpuMemoryBufferManager> gpu_memory_buffer_manager(
      new StubBrowserGpuMemoryBufferManager);
  scoped_ptr<BufferQueue> output_surface =
      CreateOutputSurfaceWithMock(&context, gpu_memory_buffer_manager.get());
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, Ne(0U)));
  EXPECT_CALL(*context, destroyImageCHROMIUM(1));
  Expectation image =
      EXPECT_CALL(*context, createImageCHROMIUM(_, 0, 0, GL_RGBA))
          .WillOnce(Return(1));
  Expectation fb =
      EXPECT_CALL(*context, bindFramebuffer(GL_FRAMEBUFFER, Ne(0U)));
  Expectation tex = EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, Ne(0U)));
  Expectation bind_tex =
      EXPECT_CALL(*context, bindTexImage2DCHROMIUM(GL_TEXTURE_2D, 1))
          .After(tex, image);
  EXPECT_CALL(
      *context,
      framebufferTexture2D(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Ne(0U), _))
      .After(fb, bind_tex);

  output_surface->BindFramebuffer();
}

TEST(BufferQueueStandaloneTest, CheckBoundFramebuffer) {
  scoped_ptr<BrowserGpuMemoryBufferManager> gpu_memory_buffer_manager;
  scoped_ptr<BufferQueue> output_surface;
  scoped_refptr<cc::TestContextProvider> context_provider =
      cc::TestContextProvider::Create(cc::TestWebGraphicsContext3D::Create());
  context_provider->BindToCurrentThread();
  gpu_memory_buffer_manager.reset(new StubBrowserGpuMemoryBufferManager);

  scoped_ptr<GLHelper> gl_helper;
  gl_helper.reset(new GLHelper(context_provider->ContextGL(),
                               context_provider->ContextSupport()));

  output_surface.reset(new BufferQueue(context_provider, GL_RGBA,
                                       gl_helper.get(),
                                       gpu_memory_buffer_manager.get(), 1));
  output_surface->Initialize();
  output_surface->Reshape(screen_size, 1.0f);
  // Trigger a sub-buffer copy to exercise all paths.
  output_surface->BindFramebuffer();
  output_surface->SwapBuffers(screen_rect);
  output_surface->PageFlipComplete();
  output_surface->BindFramebuffer();
  output_surface->SwapBuffers(small_damage);

  int current_fbo = 0;
  context_provider->ContextGL()->GetIntegerv(GL_FRAMEBUFFER_BINDING,
                                             &current_fbo);
  EXPECT_EQ(static_cast<int>(output_surface->fbo()), current_fbo);
}

TEST_F(BufferQueueTest, PartialSwapReuse) {
  output_surface_->Reshape(screen_size, 1.0f);
  ASSERT_TRUE(doublebuffering_);
  EXPECT_CALL(*mock_output_surface_,
              CopyBufferDamage(_, _, small_damage, screen_rect)).Times(1);
  EXPECT_CALL(*mock_output_surface_,
              CopyBufferDamage(_, _, small_damage, small_damage)).Times(1);
  EXPECT_CALL(*mock_output_surface_,
              CopyBufferDamage(_, _, large_damage, small_damage)).Times(1);
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(small_damage);
  SendDamagedFrame(large_damage);
  // Verify that the damage has propagated.
  EXPECT_EQ(next_frame().damage, large_damage);
}

TEST_F(BufferQueueTest, PartialSwapFullFrame) {
  output_surface_->Reshape(screen_size, 1.0f);
  ASSERT_TRUE(doublebuffering_);
  EXPECT_CALL(*mock_output_surface_,
              CopyBufferDamage(_, _, small_damage, screen_rect)).Times(1);
  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendFullFrame();
  SendFullFrame();
  EXPECT_EQ(next_frame().damage, screen_rect);
}

TEST_F(BufferQueueTest, PartialSwapOverlapping) {
  output_surface_->Reshape(screen_size, 1.0f);
  ASSERT_TRUE(doublebuffering_);
  EXPECT_CALL(*mock_output_surface_,
              CopyBufferDamage(_, _, small_damage, screen_rect)).Times(1);
  EXPECT_CALL(*mock_output_surface_, CopyBufferDamage(_, _, overlapping_damage,
                                                      small_damage)).Times(1);

  SendFullFrame();
  SendDamagedFrame(small_damage);
  SendDamagedFrame(overlapping_damage);
  EXPECT_EQ(next_frame().damage, overlapping_damage);
}

TEST_F(BufferQueueTest, MultipleBindCalls) {
  // Check that multiple bind calls do not create or change surfaces.
  output_surface_->BindFramebuffer();
  EXPECT_EQ(1, CountBuffers());
  unsigned int fb = current_surface();
  output_surface_->BindFramebuffer();
  EXPECT_EQ(1, CountBuffers());
  EXPECT_EQ(fb, current_surface());
}

TEST_F(BufferQueueTest, CheckDoubleBuffering) {
  // Check buffer flow through double buffering path.
  EXPECT_EQ(0, CountBuffers());
  output_surface_->BindFramebuffer();
  EXPECT_EQ(1, CountBuffers());
  EXPECT_NE(0U, current_surface());
  EXPECT_FALSE(displayed_frame().texture);
  SwapBuffers();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  output_surface_->PageFlipComplete();
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame().texture);
  output_surface_->BindFramebuffer();
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame().texture);
  SwapBuffers();
  CheckUnique();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame().texture);
  output_surface_->PageFlipComplete();
  CheckUnique();
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_EQ(1U, available_surfaces().size());
  EXPECT_TRUE(displayed_frame().texture);
  output_surface_->BindFramebuffer();
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_TRUE(available_surfaces().empty());
}

TEST_F(BufferQueueTest, CheckTripleBuffering) {
  // Check buffer flow through triple buffering path.

  // This bit is the same sequence tested in the doublebuffering case.
  output_surface_->BindFramebuffer();
  EXPECT_FALSE(displayed_frame().texture);
  SwapBuffers();
  output_surface_->PageFlipComplete();
  output_surface_->BindFramebuffer();
  SwapBuffers();

  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame().texture);
  output_surface_->BindFramebuffer();
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame().texture);
  output_surface_->PageFlipComplete();
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(0U, current_surface());
  EXPECT_EQ(0U, in_flight_surfaces().size());
  EXPECT_TRUE(displayed_frame().texture);
  EXPECT_EQ(1U, available_surfaces().size());
}

TEST_F(BufferQueueTest, CheckCorrectBufferOrdering) {
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->BindFramebuffer();
    SwapBuffers();
  }

  EXPECT_EQ(kSwapCount, in_flight_surfaces().size());
  for (size_t i = 0; i < kSwapCount; ++i) {
    unsigned int next_texture_id = in_flight_surfaces().front().texture;
    output_surface_->PageFlipComplete();
    EXPECT_EQ(displayed_frame().texture, next_texture_id);
  }
}

TEST_F(BufferQueueTest, ReshapeWithInFlightSurfaces) {
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->BindFramebuffer();
    SwapBuffers();
  }

  output_surface_->Reshape(gfx::Size(10, 20), 1.0f);
  EXPECT_EQ(3u, in_flight_surfaces().size());

  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->PageFlipComplete();
    EXPECT_EQ(0u, displayed_frame().texture);
  }

  // The dummy surfacess left should be discarded.
  EXPECT_EQ(0u, available_surfaces().size());
}

TEST_F(BufferQueueTest, SwapAfterReshape) {
  const size_t kSwapCount = 3;
  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->BindFramebuffer();
    SwapBuffers();
  }

  output_surface_->Reshape(gfx::Size(10, 20), 1.0f);

  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->BindFramebuffer();
    SwapBuffers();
  }

  EXPECT_EQ(2 * kSwapCount, in_flight_surfaces().size());

  for (size_t i = 0; i < kSwapCount; ++i) {
    output_surface_->PageFlipComplete();
    EXPECT_EQ(0u, displayed_frame().texture);
  }

  CheckUnique();

  for (size_t i = 0; i < kSwapCount; ++i) {
    unsigned int next_texture_id = in_flight_surfaces().front().texture;
    output_surface_->PageFlipComplete();
    EXPECT_EQ(displayed_frame().texture, next_texture_id);
    EXPECT_NE(0u, displayed_frame().texture);
  }
}

TEST_F(BufferQueueMockedContextTest, RecreateBuffers) {
  // This setup is to easily get one frame in each of:
  // - currently bound for drawing.
  // - in flight to GPU.
  // - currently displayed.
  // - free frame.
  // This tests buffers in all states.
  // Bind/swap pushes frames into the in flight list, then the PageFlipComplete
  // calls pull one frame into displayed and another into the free list.
  output_surface_->BindFramebuffer();
  SwapBuffers();
  output_surface_->BindFramebuffer();
  SwapBuffers();
  output_surface_->BindFramebuffer();
  SwapBuffers();
  output_surface_->BindFramebuffer();
  output_surface_->PageFlipComplete();
  output_surface_->PageFlipComplete();
  // We should have one buffer in each possible state right now, including one
  // being drawn to.
  ASSERT_EQ(1U, in_flight_surfaces().size());
  ASSERT_EQ(1U, available_surfaces().size());
  EXPECT_TRUE(displayed_frame().texture);
  EXPECT_TRUE(current_frame().texture);

  auto current = current_frame();
  auto displayed = displayed_frame();
  auto in_flight = in_flight_surfaces().front();
  auto available = available_surfaces().front();

  // Expect all 4 images to be destroyed, 3 of the existing textures to be
  // copied from and 3 new images to be created.
  EXPECT_CALL(*context_, createImageCHROMIUM(_, 0, 0, GL_RGBA)).Times(3);
  Expectation copy1 =
      EXPECT_CALL(*mock_output_surface_,
                  CopyBufferDamage(_, displayed.texture, _, _)).Times(1);
  Expectation copy2 =
      EXPECT_CALL(*mock_output_surface_,
                  CopyBufferDamage(_, current.texture, _, _)).Times(1);
  Expectation copy3 =
      EXPECT_CALL(*mock_output_surface_,
                  CopyBufferDamage(_, in_flight.texture, _, _)).Times(1);

  EXPECT_CALL(*context_, destroyImageCHROMIUM(displayed.image))
      .Times(1)
      .After(copy1);
  EXPECT_CALL(*context_, destroyImageCHROMIUM(current.image))
      .Times(1)
      .After(copy2);
  EXPECT_CALL(*context_, destroyImageCHROMIUM(in_flight.image))
      .Times(1)
      .After(copy3);
  EXPECT_CALL(*context_, destroyImageCHROMIUM(available.image)).Times(1);
  // After copying, we expect the framebuffer binding to be updated.
  EXPECT_CALL(*context_, bindFramebuffer(_, _))
      .After(copy1)
      .After(copy2)
      .After(copy3);
  EXPECT_CALL(*context_, framebufferTexture2D(_, _, _, _, _))
      .After(copy1)
      .After(copy2)
      .After(copy3);

  output_surface_->RecreateBuffers();
  testing::Mock::VerifyAndClearExpectations(context_);
  testing::Mock::VerifyAndClearExpectations(mock_output_surface_);

  // All free buffers should be destroyed, the remaining buffers should all
  // be replaced but still valid.
  EXPECT_EQ(1U, in_flight_surfaces().size());
  EXPECT_EQ(0U, available_surfaces().size());
  EXPECT_TRUE(displayed_frame().texture);
  EXPECT_TRUE(current_frame().texture);
}

}  // namespace
}  // namespace content
