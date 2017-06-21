// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/buffers.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using base::MD5DigestToBase16;

// Helper function that initializes a YV12 frame with white and black scan
// lines based on the |white_to_black| parameter.  If 0, then the entire
// frame will be black, if 1 then the entire frame will be white.
void InitializeYV12Frame(VideoFrame* frame, double white_to_black) {
  EXPECT_EQ(VideoFrame::YV12, frame->format());
  const int first_black_row =
      static_cast<int>(frame->coded_size().height() * white_to_black);
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (int row = 0; row < frame->coded_size().height(); ++row) {
    int color = (row < first_black_row) ? 0xFF : 0x00;
    memset(y_plane, color, frame->stride(VideoFrame::kYPlane));
    y_plane += frame->stride(VideoFrame::kYPlane);
  }
  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  for (int row = 0; row < frame->coded_size().height(); row += 2) {
    memset(u_plane, 0x80, frame->stride(VideoFrame::kUPlane));
    memset(v_plane, 0x80, frame->stride(VideoFrame::kVPlane));
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

// Given a |yv12_frame| this method converts the YV12 frame to RGBA and
// makes sure that all the pixels of the RBG frame equal |expect_rgb_color|.
void ExpectFrameColor(media::VideoFrame* yv12_frame, uint32 expect_rgb_color) {
  ASSERT_EQ(VideoFrame::YV12, yv12_frame->format());
  ASSERT_EQ(yv12_frame->stride(VideoFrame::kUPlane),
            yv12_frame->stride(VideoFrame::kVPlane));
  ASSERT_EQ(
      yv12_frame->coded_size().width() & (VideoFrame::kFrameSizeAlignment - 1),
      0);
  ASSERT_EQ(
      yv12_frame->coded_size().height() & (VideoFrame::kFrameSizeAlignment - 1),
      0);

  size_t bytes_per_row = yv12_frame->coded_size().width() * 4u;
  uint8* rgb_data = reinterpret_cast<uint8*>(
      base::AlignedAlloc(bytes_per_row * yv12_frame->coded_size().height() +
                             VideoFrame::kFrameSizePadding,
                         VideoFrame::kFrameAddressAlignment));

  media::ConvertYUVToRGB32(yv12_frame->data(VideoFrame::kYPlane),
                           yv12_frame->data(VideoFrame::kUPlane),
                           yv12_frame->data(VideoFrame::kVPlane),
                           rgb_data,
                           yv12_frame->coded_size().width(),
                           yv12_frame->coded_size().height(),
                           yv12_frame->stride(VideoFrame::kYPlane),
                           yv12_frame->stride(VideoFrame::kUPlane),
                           bytes_per_row,
                           media::YV12);

  for (int row = 0; row < yv12_frame->coded_size().height(); ++row) {
    uint32* rgb_row_data = reinterpret_cast<uint32*>(
        rgb_data + (bytes_per_row * row));
    for (int col = 0; col < yv12_frame->coded_size().width(); ++col) {
      SCOPED_TRACE(base::StringPrintf("Checking (%d, %d)", row, col));
      EXPECT_EQ(expect_rgb_color, rgb_row_data[col]);
    }
  }

  base::AlignedFree(rgb_data);
}

// Fill each plane to its reported extents and verify accessors report non
// zero values.  Additionally, for the first plane verify the rows and
// row_bytes values are correct.
void ExpectFrameExtents(VideoFrame::Format format, const char* expected_hash) {
  const unsigned char kFillByte = 0x80;
  const int kWidth = 61;
  const int kHeight = 31;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  gfx::Size size(kWidth, kHeight);
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      format, size, gfx::Rect(size), size, kTimestamp);
  ASSERT_TRUE(frame.get());

  int planes = VideoFrame::NumPlanes(format);
  for (int plane = 0; plane < planes; plane++) {
    SCOPED_TRACE(base::StringPrintf("Checking plane %d", plane));
    EXPECT_TRUE(frame->data(plane));
    EXPECT_TRUE(frame->stride(plane));
    EXPECT_TRUE(frame->rows(plane));
    EXPECT_TRUE(frame->row_bytes(plane));

    memset(frame->data(plane), kFillByte,
           frame->stride(plane) * frame->rows(plane));
  }

  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, frame);
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), expected_hash);
}

TEST(VideoFrame, CreateFrame) {
  const int kWidth = 64;
  const int kHeight = 48;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  // Create a YV12 Video Frame.
  gfx::Size size(kWidth, kHeight);
  scoped_refptr<media::VideoFrame> frame =
      VideoFrame::CreateFrame(media::VideoFrame::YV12, size, gfx::Rect(size),
                              size, kTimestamp);
  ASSERT_TRUE(frame.get());

  // Test VideoFrame implementation.
  EXPECT_EQ(media::VideoFrame::YV12, frame->format());
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame.get(), 0.0f);
    ExpectFrameColor(frame.get(), 0xFF000000);
  }
  base::MD5Digest digest;
  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, frame);
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), "9065c841d9fca49186ef8b4ef547e79b");
  {
    SCOPED_TRACE("");
    InitializeYV12Frame(frame.get(), 1.0f);
    ExpectFrameColor(frame.get(), 0xFFFFFFFF);
  }
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, frame);
  base::MD5Final(&digest, &context);
  EXPECT_EQ(MD5DigestToBase16(digest), "911991d51438ad2e1a40ed5f6fc7c796");

  // Test an empty frame.
  frame = VideoFrame::CreateEOSFrame();
  EXPECT_TRUE(
      frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));
}

TEST(VideoFrame, CreateBlackFrame) {
  const int kWidth = 2;
  const int kHeight = 2;
  const uint8 kExpectedYRow[] = { 0, 0 };
  const uint8 kExpectedUVRow[] = { 128 };

  scoped_refptr<media::VideoFrame> frame =
      VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
  ASSERT_TRUE(frame.get());
  EXPECT_TRUE(frame->IsMappable());

  // Test basic properties.
  EXPECT_EQ(0, frame->timestamp().InMicroseconds());
  EXPECT_FALSE(
      frame->metadata()->IsTrue(VideoFrameMetadata::END_OF_STREAM));

  // Test |frame| properties.
  EXPECT_EQ(VideoFrame::YV12, frame->format());
  EXPECT_EQ(kWidth, frame->coded_size().width());
  EXPECT_EQ(kHeight, frame->coded_size().height());

  // Test frames themselves.
  uint8* y_plane = frame->data(VideoFrame::kYPlane);
  for (int y = 0; y < frame->coded_size().height(); ++y) {
    EXPECT_EQ(0, memcmp(kExpectedYRow, y_plane, arraysize(kExpectedYRow)));
    y_plane += frame->stride(VideoFrame::kYPlane);
  }

  uint8* u_plane = frame->data(VideoFrame::kUPlane);
  uint8* v_plane = frame->data(VideoFrame::kVPlane);
  for (int y = 0; y < frame->coded_size().height() / 2; ++y) {
    EXPECT_EQ(0, memcmp(kExpectedUVRow, u_plane, arraysize(kExpectedUVRow)));
    EXPECT_EQ(0, memcmp(kExpectedUVRow, v_plane, arraysize(kExpectedUVRow)));
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

static void FrameNoLongerNeededCallback(
    const scoped_refptr<media::VideoFrame>& frame,
    bool* triggered) {
  *triggered = true;
}

TEST(VideoFrame, WrapVideoFrame) {
  const int kWidth = 4;
  const int kHeight = 4;
  scoped_refptr<media::VideoFrame> frame;
  bool done_callback_was_run = false;
  {
    scoped_refptr<media::VideoFrame> wrapped_frame =
        VideoFrame::CreateBlackFrame(gfx::Size(kWidth, kHeight));
    ASSERT_TRUE(wrapped_frame.get());

    gfx::Rect visible_rect(1, 1, 1, 1);
    gfx::Size natural_size = visible_rect.size();
    frame = media::VideoFrame::WrapVideoFrame(
        wrapped_frame, visible_rect, natural_size);
    frame->AddDestructionObserver(
        base::Bind(&FrameNoLongerNeededCallback, wrapped_frame,
                   &done_callback_was_run));
    EXPECT_EQ(wrapped_frame->coded_size(), frame->coded_size());
    EXPECT_EQ(wrapped_frame->data(media::VideoFrame::kYPlane),
              frame->data(media::VideoFrame::kYPlane));
    EXPECT_NE(wrapped_frame->visible_rect(), frame->visible_rect());
    EXPECT_EQ(visible_rect, frame->visible_rect());
    EXPECT_NE(wrapped_frame->natural_size(), frame->natural_size());
    EXPECT_EQ(natural_size, frame->natural_size());
  }

  EXPECT_FALSE(done_callback_was_run);
  frame = NULL;
  EXPECT_TRUE(done_callback_was_run);
}

// Ensure each frame is properly sized and allocated.  Will trigger OOB reads
// and writes as well as incorrect frame hashes otherwise.
TEST(VideoFrame, CheckFrameExtents) {
  // Each call consists of a VideoFrame::Format and the expected hash of all
  // planes if filled with kFillByte (defined in ExpectFrameExtents).
  ExpectFrameExtents(VideoFrame::YV12, "8e5d54cb23cd0edca111dd35ffb6ff05");
  ExpectFrameExtents(VideoFrame::YV16, "cce408a044b212db42a10dfec304b3ef");
}

static void TextureCallback(uint32* called_sync_point,
                            uint32 release_sync_point) {
  *called_sync_point = release_sync_point;
}

// Verify the gpu::MailboxHolder::ReleaseCallback is called when VideoFrame is
// destroyed with the default release sync point.
TEST(VideoFrame, TextureNoLongerNeededCallbackIsCalled) {
  uint32 called_sync_point = 1;

  {
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTexture(
        VideoFrame::ARGB,
        gpu::MailboxHolder(gpu::Mailbox::Generate(), 5, 0 /* sync_point */),
        base::Bind(&TextureCallback, &called_sync_point),
        gfx::Size(10, 10),  // coded_size
        gfx::Rect(10, 10),  // visible_rect
        gfx::Size(10, 10),  // natural_size
        base::TimeDelta()); // timestamp
    EXPECT_EQ(VideoFrame::ARGB, frame->format());
    EXPECT_EQ(VideoFrame::STORAGE_OPAQUE, frame->storage_type());
    EXPECT_TRUE(frame->HasTextures());
  }
  // Nobody set a sync point to |frame|, so |frame| set |called_sync_point| to 0
  // as default value.
  EXPECT_EQ(0u, called_sync_point);
}

namespace {

class SyncPointClientImpl : public VideoFrame::SyncPointClient {
 public:
  explicit SyncPointClientImpl(uint32 sync_point) : sync_point_(sync_point) {}
  ~SyncPointClientImpl() override {}
  uint32 InsertSyncPoint() override { return sync_point_; }
  void WaitSyncPoint(uint32 sync_point) override {}

 private:
  uint32 sync_point_;
};

}  // namespace

// Verify the gpu::MailboxHolder::ReleaseCallback is called when VideoFrame is
// destroyed with the release sync point, which was updated by clients.
// (i.e. the compositor, webgl).
TEST(VideoFrame,
     TexturesNoLongerNeededCallbackAfterTakingAndReleasingMailboxes) {
  const int kPlanesNum = 3;
  gpu::Mailbox mailbox[kPlanesNum];
  for (int i = 0; i < kPlanesNum; ++i) {
    mailbox[i].name[0] = 50 + 1;
  }

  uint32 sync_point = 7;
  uint32 target = 9;
  uint32 release_sync_point = 111;
  uint32 called_sync_point = 0;
  {
    scoped_refptr<VideoFrame> frame = VideoFrame::WrapYUV420NativeTextures(
        gpu::MailboxHolder(mailbox[VideoFrame::kYPlane], target, sync_point),
        gpu::MailboxHolder(mailbox[VideoFrame::kUPlane], target, sync_point),
        gpu::MailboxHolder(mailbox[VideoFrame::kVPlane], target, sync_point),
        base::Bind(&TextureCallback, &called_sync_point),
        gfx::Size(10, 10),  // coded_size
        gfx::Rect(10, 10),  // visible_rect
        gfx::Size(10, 10),  // natural_size
        base::TimeDelta()); // timestamp

    EXPECT_EQ(VideoFrame::STORAGE_OPAQUE, frame->storage_type());
    EXPECT_EQ(VideoFrame::I420, frame->format());
    EXPECT_EQ(3u, VideoFrame::NumPlanes(frame->format()));
    EXPECT_TRUE(frame->HasTextures());
    for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); ++i) {
      const gpu::MailboxHolder& mailbox_holder = frame->mailbox_holder(i);
      EXPECT_EQ(mailbox[i].name[0], mailbox_holder.mailbox.name[0]);
      EXPECT_EQ(target, mailbox_holder.texture_target);
      EXPECT_EQ(sync_point, mailbox_holder.sync_point);
    }

    SyncPointClientImpl client(release_sync_point);
    frame->UpdateReleaseSyncPoint(&client);
    EXPECT_EQ(sync_point,
              frame->mailbox_holder(VideoFrame::kYPlane).sync_point);
  }
  EXPECT_EQ(release_sync_point, called_sync_point);
}

TEST(VideoFrame, ZeroInitialized) {
  const int kWidth = 64;
  const int kHeight = 48;
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(1337);

  gfx::Size size(kWidth, kHeight);
  scoped_refptr<media::VideoFrame> frame = VideoFrame::CreateFrame(
      media::VideoFrame::YV12, size, gfx::Rect(size), size, kTimestamp);

  for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); ++i)
    EXPECT_EQ(0, frame->data(i)[0]);
}

TEST(VideoFrameMetadata, SetAndThenGetAllKeysForAllTypes) {
  VideoFrameMetadata metadata;

  for (int i = 0; i < VideoFrameMetadata::NUM_KEYS; ++i) {
    const VideoFrameMetadata::Key key = static_cast<VideoFrameMetadata::Key>(i);

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetBoolean(key, true);
    EXPECT_TRUE(metadata.HasKey(key));
    bool bool_value = false;
    EXPECT_TRUE(metadata.GetBoolean(key, &bool_value));
    EXPECT_EQ(true, bool_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetInteger(key, i);
    EXPECT_TRUE(metadata.HasKey(key));
    int int_value = -999;
    EXPECT_TRUE(metadata.GetInteger(key, &int_value));
    EXPECT_EQ(i, int_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetDouble(key, 3.14 * i);
    EXPECT_TRUE(metadata.HasKey(key));
    double double_value = -999.99;
    EXPECT_TRUE(metadata.GetDouble(key, &double_value));
    EXPECT_EQ(3.14 * i, double_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetString(key, base::StringPrintf("\xfe%d\xff", i));
    EXPECT_TRUE(metadata.HasKey(key));
    std::string string_value;
    EXPECT_TRUE(metadata.GetString(key, &string_value));
    EXPECT_EQ(base::StringPrintf("\xfe%d\xff", i), string_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetTimeDelta(key, base::TimeDelta::FromInternalValue(42 + i));
    EXPECT_TRUE(metadata.HasKey(key));
    base::TimeDelta delta_value;
    EXPECT_TRUE(metadata.GetTimeDelta(key, &delta_value));
    EXPECT_EQ(base::TimeDelta::FromInternalValue(42 + i), delta_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetTimeTicks(key, base::TimeTicks::FromInternalValue(~(0LL) + i));
    EXPECT_TRUE(metadata.HasKey(key));
    base::TimeTicks ticks_value;
    EXPECT_TRUE(metadata.GetTimeTicks(key, &ticks_value));
    EXPECT_EQ(base::TimeTicks::FromInternalValue(~(0LL) + i), ticks_value);
    metadata.Clear();

    EXPECT_FALSE(metadata.HasKey(key));
    metadata.SetValue(key, base::Value::CreateNullValue());
    EXPECT_TRUE(metadata.HasKey(key));
    const base::Value* const null_value = metadata.GetValue(key);
    EXPECT_TRUE(null_value);
    EXPECT_EQ(base::Value::TYPE_NULL, null_value->GetType());
    metadata.Clear();
  }
}

TEST(VideoFrameMetadata, PassMetadataViaIntermediary) {
  VideoFrameMetadata expected;
  for (int i = 0; i < VideoFrameMetadata::NUM_KEYS; ++i) {
    const VideoFrameMetadata::Key key = static_cast<VideoFrameMetadata::Key>(i);
    expected.SetInteger(key, i);
  }

  base::DictionaryValue tmp;
  expected.MergeInternalValuesInto(&tmp);
  EXPECT_EQ(static_cast<size_t>(VideoFrameMetadata::NUM_KEYS), tmp.size());

  VideoFrameMetadata result;
  result.MergeInternalValuesFrom(tmp);

  for (int i = 0; i < VideoFrameMetadata::NUM_KEYS; ++i) {
    const VideoFrameMetadata::Key key = static_cast<VideoFrameMetadata::Key>(i);
    int value = -1;
    EXPECT_TRUE(result.GetInteger(key, &value));
    EXPECT_EQ(i, value);
  }
}

}  // namespace media
