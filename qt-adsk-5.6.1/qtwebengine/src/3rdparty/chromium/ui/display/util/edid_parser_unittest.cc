// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/edid_parser.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

namespace {

// Returns the number of characters in the string literal but doesn't count its
// terminator NULL byte.
#define charsize(str) (arraysize(str) - 1)

// Sample EDID data extracted from real devices.
const unsigned char kNormalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x22\xf0\x6c\x28\x01\x01\x01\x01"
    "\x02\x16\x01\x04\xb5\x40\x28\x78\xe2\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\xe2\x68\x00\xa0\xa0\x40\x2e\x60\x30\x20"
    "\x36\x00\x81\x90\x21\x00\x00\x1a\xbc\x1b\x00\xa0\x50\x20\x17\x30"
    "\x30\x20\x36\x00\x81\x90\x21\x00\x00\x1a\x00\x00\x00\xfc\x00\x48"
    "\x50\x20\x5a\x52\x33\x30\x77\x0a\x20\x20\x20\x20\x00\x00\x00\xff"
    "\x00\x43\x4e\x34\x32\x30\x32\x31\x33\x37\x51\x0a\x20\x20\x00\x71";

const unsigned char kInternalDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\xa3\x42\x31\x00\x00\x00\x00"
    "\x00\x15\x01\x03\x80\x1a\x10\x78\x0a\xd3\xe5\x95\x5c\x60\x90\x27"
    "\x19\x50\x54\x00\x00\x00\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
    "\x01\x01\x01\x01\x01\x01\x9e\x1b\x00\xa0\x50\x20\x12\x30\x10\x30"
    "\x13\x00\x05\xa3\x10\x00\x00\x19\x00\x00\x00\x0f\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x23\x87\x02\x64\x00\x00\x00\x00\xfe\x00\x53"
    "\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x00\x00\x00\xfe"
    "\x00\x31\x32\x31\x41\x54\x31\x31\x2d\x38\x30\x31\x0a\x20\x00\x45";

const unsigned char kOverscanDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x4c\x2d\xfe\x08\x00\x00\x00\x00"
    "\x29\x15\x01\x03\x80\x10\x09\x78\x0a\xee\x91\xa3\x54\x4c\x99\x26"
    "\x0f\x50\x54\xbd\xef\x80\x71\x4f\x81\xc0\x81\x00\x81\x80\x95\x00"
    "\xa9\xc0\xb3\x00\x01\x01\x02\x3a\x80\x18\x71\x38\x2d\x40\x58\x2c"
    "\x45\x00\xa0\x5a\x00\x00\x00\x1e\x66\x21\x56\xaa\x51\x00\x1e\x30"
    "\x46\x8f\x33\x00\xa0\x5a\x00\x00\x00\x1e\x00\x00\x00\xfd\x00\x18"
    "\x4b\x0f\x51\x17\x00\x0a\x20\x20\x20\x20\x20\x20\x00\x00\x00\xfc"
    "\x00\x53\x41\x4d\x53\x55\x4e\x47\x0a\x20\x20\x20\x20\x20\x01\x1d"
    "\x02\x03\x1f\xf1\x47\x90\x04\x05\x03\x20\x22\x07\x23\x09\x07\x07"
    "\x83\x01\x00\x00\xe2\x00\x0f\x67\x03\x0c\x00\x20\x00\xb8\x2d\x01"
    "\x1d\x80\x18\x71\x1c\x16\x20\x58\x2c\x25\x00\xa0\x5a\x00\x00\x00"
    "\x9e\x01\x1d\x00\x72\x51\xd0\x1e\x20\x6e\x28\x55\x00\xa0\x5a\x00"
    "\x00\x00\x1e\x8c\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e\x96\x00\xa0"
    "\x5a\x00\x00\x00\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc6";

// The EDID info misdetecting overscan once. see crbug.com/226318
const unsigned char kMisdetectedDisplay[] =
    "\x00\xff\xff\xff\xff\xff\xff\x00\x10\xac\x64\x40\x4c\x30\x30\x32"
    "\x0c\x15\x01\x03\x80\x40\x28\x78\xea\x8d\x85\xad\x4f\x35\xb1\x25"
    "\x0e\x50\x54\xa5\x4b\x00\x71\x4f\x81\x00\x81\x80\xd1\x00\xa9\x40"
    "\x01\x01\x01\x01\x01\x01\x28\x3c\x80\xa0\x70\xb0\x23\x40\x30\x20"
    "\x36\x00\x81\x91\x21\x00\x00\x1a\x00\x00\x00\xff\x00\x50\x48\x35"
    "\x4e\x59\x31\x33\x4e\x32\x30\x30\x4c\x0a\x00\x00\x00\xfc\x00\x44"
    "\x45\x4c\x4c\x20\x55\x33\x30\x31\x31\x0a\x20\x20\x00\x00\x00\xfd"
    "\x00\x31\x56\x1d\x5e\x12\x00\x0a\x20\x20\x20\x20\x20\x20\x01\x38"
    "\x02\x03\x29\xf1\x50\x90\x05\x04\x03\x02\x07\x16\x01\x06\x11\x12"
    "\x15\x13\x14\x1f\x20\x23\x0d\x7f\x07\x83\x0f\x00\x00\x67\x03\x0c"
    "\x00\x10\x00\x38\x2d\xe3\x05\x03\x01\x02\x3a\x80\x18\x71\x38\x2d"
    "\x40\x58\x2c\x45\x00\x81\x91\x21\x00\x00\x1e\x01\x1d\x80\x18\x71"
    "\x1c\x16\x20\x58\x2c\x25\x00\x81\x91\x21\x00\x00\x9e\x01\x1d\x00"
    "\x72\x51\xd0\x1e\x20\x6e\x28\x55\x00\x81\x91\x21\x00\x00\x1e\x8c"
    "\x0a\xd0\x8a\x20\xe0\x2d\x10\x10\x3e\x96\x00\x81\x91\x21\x00\x00"
    "\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x94";

const unsigned char kLP2565A[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x76\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x80\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x81\x80\x81\x99\x71\x00\xA9\x00"
    "\xA9\x40\xB3\x00\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x11\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\xA4";

const unsigned char kLP2565B[] =
    "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00\x22\xF0\x75\x26\x01\x01\x01\x01"
    "\x02\x12\x01\x03\x6E\x34\x21\x78\xEE\xEF\x95\xA3\x54\x4C\x9B\x26"
    "\x0F\x50\x54\xA5\x6B\x80\x81\x40\x71\x00\xA9\x00\xA9\x40\xA9\x4F"
    "\xB3\x00\xD1\xC0\xD1\x00\x28\x3C\x80\xA0\x70\xB0\x23\x40\x30\x20"
    "\x36\x00\x07\x44\x21\x00\x00\x1A\x00\x00\x00\xFD\x00\x30\x55\x1E"
    "\x5E\x15\x00\x0A\x20\x20\x20\x20\x20\x20\x00\x00\x00\xFC\x00\x48"
    "\x50\x20\x4C\x50\x32\x34\x36\x35\x0A\x20\x20\x20\x00\x00\x00\xFF"
    "\x00\x43\x4E\x4B\x38\x30\x32\x30\x34\x48\x4D\x0A\x20\x20\x00\x45";

void Reset(gfx::Size* pixel, gfx::Size* size) {
  pixel->SetSize(0, 0);
  size->SetSize(0, 0);
}

}  // namespace

TEST(EDIDParserTest, ParseOverscanFlag) {
  bool flag = false;
  std::vector<uint8_t> edid(
      kNormalDisplay, kNormalDisplay + charsize(kNormalDisplay));
  EXPECT_FALSE(ParseOutputOverscanFlag(edid, &flag));

  flag = false;
  edid.assign(kInternalDisplay, kInternalDisplay + charsize(kInternalDisplay));
  EXPECT_FALSE(ParseOutputOverscanFlag(edid, &flag));

  flag = false;
  edid.assign(kOverscanDisplay, kOverscanDisplay + charsize(kOverscanDisplay));
  EXPECT_TRUE(ParseOutputOverscanFlag(edid, &flag));
  EXPECT_TRUE(flag);

  flag = false;
  edid.assign(
      kMisdetectedDisplay, kMisdetectedDisplay + charsize(kMisdetectedDisplay));
  EXPECT_FALSE(ParseOutputOverscanFlag(edid, &flag));

  flag = false;
  // Copy |kOverscanDisplay| and set flags to false in it. The overscan flags
  // are embedded at byte 150 in this specific example. Fix here too when the
  // contents of kOverscanDisplay is altered.
  edid.assign(kOverscanDisplay, kOverscanDisplay + charsize(kOverscanDisplay));
  edid[150] = '\0';
  EXPECT_TRUE(ParseOutputOverscanFlag(edid, &flag));
  EXPECT_FALSE(flag);
}

TEST(EDIDParserTest, ParseBrokenOverscanData) {
  // Do not fill valid data here because it anyway fails to parse the data.
  std::vector<uint8_t> data;
  bool flag = false;
  EXPECT_FALSE(ParseOutputOverscanFlag(data, &flag));
  data.assign(126, '\0');
  EXPECT_FALSE(ParseOutputOverscanFlag(data, &flag));

  // extending data because ParseOutputOverscanFlag() will access the data.
  data.assign(128, '\0');
  // The number of CEA extensions is stored at byte 126.
  data[126] = '\x01';
  EXPECT_FALSE(ParseOutputOverscanFlag(data, &flag));

  data.assign(150, '\0');
  data[126] = '\x01';
  EXPECT_FALSE(ParseOutputOverscanFlag(data, &flag));
}

TEST(EDIDParserTest, ParseEDID) {
  uint16_t manufacturer_id = 0;
  uint16_t product_code = 0;
  std::string human_readable_name;
  std::vector<uint8_t> edid(
      kNormalDisplay, kNormalDisplay + charsize(kNormalDisplay));
  gfx::Size pixel;
  gfx::Size size;
  EXPECT_TRUE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                    &human_readable_name, &pixel, &size));
  EXPECT_EQ(0x22f0u, manufacturer_id);
  EXPECT_EQ(0x286cu, product_code);
  EXPECT_EQ("HP ZR30w", human_readable_name);
  EXPECT_EQ("2560x1600", pixel.ToString());
  EXPECT_EQ("641x400", size.ToString());

  manufacturer_id = 0;
  product_code = 0;
  human_readable_name.clear();
  Reset(&pixel, &size);
  edid.assign(kInternalDisplay, kInternalDisplay + charsize(kInternalDisplay));

  EXPECT_TRUE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                    nullptr, &pixel, &size));
  EXPECT_EQ(0x4ca3u, manufacturer_id);
  EXPECT_EQ(0x3142u, product_code);
  EXPECT_EQ("", human_readable_name);
  EXPECT_EQ("1280x800", pixel.ToString());
  EXPECT_EQ("261x163", size.ToString());

  // Internal display doesn't have name.
  EXPECT_TRUE(ParseOutputDeviceData(edid, nullptr, nullptr,
                                    &human_readable_name, &pixel, &size));
  EXPECT_TRUE(human_readable_name.empty());

  manufacturer_id = 0;
  product_code = 0;
  human_readable_name.clear();
  Reset(&pixel, &size);
  edid.assign(kOverscanDisplay, kOverscanDisplay + charsize(kOverscanDisplay));
  EXPECT_TRUE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                    &human_readable_name, &pixel, &size));
  EXPECT_EQ(0x4c2du, manufacturer_id);
  EXPECT_EQ(0x08feu, product_code);
  EXPECT_EQ("SAMSUNG", human_readable_name);
  EXPECT_EQ("1920x1080", pixel.ToString());
  EXPECT_EQ("160x90", size.ToString());
}

TEST(EDIDParserTest, ParseBrokenEDID) {
  uint16_t manufacturer_id = 0;
  uint16_t product_code = 0;
  std::string human_readable_name;
  std::vector<uint8_t> edid;

  gfx::Size dummy;

  // length == 0
  EXPECT_FALSE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                     &human_readable_name, &dummy, &dummy));

  // name is broken. Copying kNormalDisplay and substitute its name data by
  // some control code.
  edid.assign(kNormalDisplay, kNormalDisplay + charsize(kNormalDisplay));

  // display's name data is embedded in byte 95-107 in this specific example.
  // Fix here too when the contents of kNormalDisplay is altered.
  edid[97] = '\x1b';
  EXPECT_FALSE(ParseOutputDeviceData(edid, &manufacturer_id, nullptr,
                                     &human_readable_name, &dummy, &dummy));

  // If |human_readable_name| isn't specified, it skips parsing the name.
  manufacturer_id = 0;
  product_code = 0;
  EXPECT_TRUE(ParseOutputDeviceData(edid, &manufacturer_id, &product_code,
                                    nullptr, &dummy, &dummy));
  EXPECT_EQ(0x22f0u, manufacturer_id);
  EXPECT_EQ(0x286cu, product_code);
}

TEST(EDIDParserTest, GetDisplayId) {
  // EDID of kLP2565A and B are slightly different but actually the same device.
  int64_t id1 = -1;
  int64_t id2 = -1;
  int64_t product_id1 = -1;
  int64_t product_id2 = -1;
  std::vector<uint8_t> edid(kLP2565A, kLP2565A + charsize(kLP2565A));
  EXPECT_TRUE(GetDisplayIdFromEDID(edid, 0, &id1, &product_id1));
  edid.assign(kLP2565B, kLP2565B + charsize(kLP2565B));
  EXPECT_TRUE(GetDisplayIdFromEDID(edid, 0, &id2, &product_id2));
  EXPECT_EQ(id1, id2);
  // The product code in the two EDIDs varies.
  EXPECT_NE(product_id1, product_id2);
  EXPECT_EQ(0x22f02676, product_id1);
  EXPECT_EQ(0x22f02675, product_id2);
  EXPECT_NE(-1, id1);
  EXPECT_NE(-1, product_id1);
}

TEST(EDIDParserTest, GetDisplayIdFromInternal) {
  int64_t id = -1;
  int64_t product_id = -1;
  std::vector<uint8_t> edid(
      kInternalDisplay, kInternalDisplay + charsize(kInternalDisplay));
  EXPECT_TRUE(GetDisplayIdFromEDID(edid, 0, &id, &product_id));
  EXPECT_NE(-1, id);
  EXPECT_NE(-1, product_id);
}

TEST(EDIDParserTest, GetDisplayIdFailure) {
  int64_t id = -1;
  int64_t product_id = -1;
  std::vector<uint8_t> edid;
  EXPECT_FALSE(GetDisplayIdFromEDID(edid, 0, &id, &product_id));
  EXPECT_EQ(-1, id);
  EXPECT_EQ(-1, product_id);
}

}   // namespace ui
