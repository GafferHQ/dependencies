// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "media/base/test_data_util.h"
#include "media/filters/h264_parser.h"
#include "media/video/h264_poc.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class H264POCTest : public testing::Test {
 public:
  H264POCTest() : sps_(), slice_hdr_() {
    // Default every frame to be a reference frame.
    slice_hdr_.nal_ref_idc = 1;
  }

 protected:
  bool ComputePOC() {
    return h264_poc_.ComputePicOrderCnt(&sps_, slice_hdr_, &poc_);
  }

  // Also sets as a reference frame and unsets IDR, which is required for
  // memory management control operations to be parsed.
  void SetMMCO5() {
    slice_hdr_.nal_ref_idc = 1;
    slice_hdr_.idr_pic_flag = false;
    slice_hdr_.adaptive_ref_pic_marking_mode_flag = true;
    slice_hdr_.ref_pic_marking[0].memory_mgmnt_control_operation = 6;
    slice_hdr_.ref_pic_marking[1].memory_mgmnt_control_operation = 5;
    slice_hdr_.ref_pic_marking[2].memory_mgmnt_control_operation = 0;
  }

  int32_t poc_;

  H264SPS sps_;
  H264SliceHeader slice_hdr_;
  H264POC h264_poc_;

  DISALLOW_COPY_AND_ASSIGN(H264POCTest);
};

TEST_F(H264POCTest, PicOrderCntType0) {
  sps_.pic_order_cnt_type = 0;
  sps_.log2_max_pic_order_cnt_lsb_minus4 = 0;  // 16

  // Initial IDR with POC 0.
  slice_hdr_.idr_pic_flag = true;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(0, poc_);

  // Ref frame with POC lsb 8.
  slice_hdr_.idr_pic_flag = false;
  slice_hdr_.pic_order_cnt_lsb = 8;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(8, poc_);

  // Ref frame with POC lsb 0. This should be detected as wrapping, as the
  // (negative) gap is at least half the maximum.
  slice_hdr_.pic_order_cnt_lsb = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(16, poc_);

  // Ref frame with POC lsb 9. This should be detected as negative wrapping,
  // as the (positive) gap is more than half the maximum.
  slice_hdr_.pic_order_cnt_lsb = 9;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(9, poc_);
}

TEST_F(H264POCTest, PicOrderCntType0_WithMMCO5) {
  sps_.pic_order_cnt_type = 0;
  sps_.log2_max_pic_order_cnt_lsb_minus4 = 0;  // 16

  // Initial IDR with POC 0.
  slice_hdr_.idr_pic_flag = true;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(0, poc_);

  // Skip ahead.
  slice_hdr_.idr_pic_flag = false;
  slice_hdr_.pic_order_cnt_lsb = 8;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(8, poc_);

  slice_hdr_.pic_order_cnt_lsb = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(16, poc_);

  slice_hdr_.pic_order_cnt_lsb = 8;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(24, poc_);

  SetMMCO5();
  slice_hdr_.pic_order_cnt_lsb = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(32, poc_);

  // Due to the MMCO5 above, this is relative to 0, but also detected as
  // positive wrapping.
  slice_hdr_.pic_order_cnt_lsb = 8;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(24, poc_);
}

TEST_F(H264POCTest, PicOrderCntType1) {
  sps_.pic_order_cnt_type = 1;
  sps_.log2_max_frame_num_minus4 = 0;  // 16
  sps_.num_ref_frames_in_pic_order_cnt_cycle = 2;
  sps_.expected_delta_per_pic_order_cnt_cycle = 3;
  sps_.offset_for_ref_frame[0] = 1;
  sps_.offset_for_ref_frame[1] = 2;

  // Initial IDR with POC 0.
  slice_hdr_.idr_pic_flag = true;
  slice_hdr_.frame_num = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(0, poc_);

  // Ref frame.
  slice_hdr_.idr_pic_flag = false;
  slice_hdr_.frame_num = 1;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(1, poc_);

  // Ref frame.
  slice_hdr_.frame_num = 2;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(3, poc_);

  // Ref frame.
  slice_hdr_.frame_num = 3;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(4, poc_);

  // Ref frame.
  slice_hdr_.frame_num = 4;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(6, poc_);

  // Ref frame, detected as wrapping (ie, this is frame 16).
  slice_hdr_.frame_num = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(24, poc_);
}

TEST_F(H264POCTest, PicOrderCntType1_WithMMCO5) {
  sps_.pic_order_cnt_type = 1;
  sps_.log2_max_frame_num_minus4 = 0;  // 16
  sps_.num_ref_frames_in_pic_order_cnt_cycle = 2;
  sps_.expected_delta_per_pic_order_cnt_cycle = 3;
  sps_.offset_for_ref_frame[0] = 1;
  sps_.offset_for_ref_frame[1] = 2;

  // Initial IDR with POC 0.
  slice_hdr_.idr_pic_flag = true;
  slice_hdr_.frame_num = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(0, poc_);

  // Ref frame.
  slice_hdr_.idr_pic_flag = false;
  slice_hdr_.frame_num = 1;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(1, poc_);

  // Ref frame, detected as wrapping.
  SetMMCO5();
  slice_hdr_.frame_num = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(24, poc_);

  // Ref frame, wrapping from before has been cleared.
  slice_hdr_.frame_num = 1;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(1, poc_);
}

TEST_F(H264POCTest, PicOrderCntType2) {
  sps_.pic_order_cnt_type = 2;

  // Initial IDR with POC 0.
  slice_hdr_.idr_pic_flag = true;
  slice_hdr_.frame_num = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(0, poc_);

  // Ref frame.
  slice_hdr_.idr_pic_flag = false;
  slice_hdr_.frame_num = 1;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(2, poc_);

  // Ref frame.
  slice_hdr_.frame_num = 2;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(4, poc_);

  // Ref frame.
  slice_hdr_.frame_num = 3;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(6, poc_);

  // Ref frame.
  slice_hdr_.frame_num = 4;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(8, poc_);

  // Ref frame, detected as wrapping (ie, this is frame 16).
  slice_hdr_.frame_num = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(32, poc_);
}

TEST_F(H264POCTest, PicOrderCntType2_WithMMCO5) {
  sps_.pic_order_cnt_type = 2;

  // Initial IDR with POC 0.
  slice_hdr_.idr_pic_flag = true;
  slice_hdr_.frame_num = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(0, poc_);

  // Ref frame.
  slice_hdr_.idr_pic_flag = false;
  slice_hdr_.frame_num = 1;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(2, poc_);

  // Ref frame, detected as wrapping.
  SetMMCO5();
  slice_hdr_.frame_num = 0;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(32, poc_);

  // Ref frame, wrapping from before has been cleared.
  slice_hdr_.frame_num = 1;
  ASSERT_TRUE(ComputePOC());
  ASSERT_EQ(2, poc_);
}

}  // namespace media
