// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/mac/system_snapshot_mac.h"

#include <sys/time.h>
#include <time.h>

#include <string>

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "snapshot/mac/process_reader.h"
#include "test/errors.h"
#include "util/mac/mac_util.h"

namespace crashpad {
namespace test {
namespace {

// SystemSnapshotMac objects would be cumbersome to construct in each test that
// requires one, because of the repetitive and mechanical work necessary to set
// up a ProcessReader and timeval, along with the checks to verify that these
// operations succeed. This test fixture class handles the initialization work
// so that individual tests don’t have to.
class SystemSnapshotMacTest : public testing::Test {
 public:
  SystemSnapshotMacTest()
      : Test(),
        process_reader_(),
        snapshot_time_(),
        system_snapshot_() {
  }

  const internal::SystemSnapshotMac& system_snapshot() const {
    return system_snapshot_;
  }

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(process_reader_.Initialize(mach_task_self()));
    ASSERT_EQ(0, gettimeofday(&snapshot_time_, nullptr))
        << ErrnoMessage("gettimeofday");
    system_snapshot_.Initialize(&process_reader_, &snapshot_time_);
  }

 private:
  ProcessReader process_reader_;
  timeval snapshot_time_;
  internal::SystemSnapshotMac system_snapshot_;

  DISALLOW_COPY_AND_ASSIGN(SystemSnapshotMacTest);
};

TEST_F(SystemSnapshotMacTest, GetCPUArchitecture) {
  CPUArchitecture cpu_architecture = system_snapshot().GetCPUArchitecture();

#if defined(ARCH_CPU_X86)
  EXPECT_EQ(kCPUArchitectureX86, cpu_architecture);
#elif defined(ARCH_CPU_X86_64)
  EXPECT_EQ(kCPUArchitectureX86_64, cpu_architecture);
#else
#error port to your architecture
#endif
}

TEST_F(SystemSnapshotMacTest, CPUCount) {
  EXPECT_GE(system_snapshot().CPUCount(), 1);
}

TEST_F(SystemSnapshotMacTest, CPUVendor) {
  std::string cpu_vendor = system_snapshot().CPUVendor();

#if defined(ARCH_CPU_X86_FAMILY)
  // Apple has only shipped Intel x86-family CPUs, but here’s a small nod to the
  // “Hackintosh” crowd.
  if (cpu_vendor != "GenuineIntel" && cpu_vendor != "AuthenticAMD") {
    FAIL() << "cpu_vendor " << cpu_vendor;
  }
#else
#error port to your architecture
#endif
}

#if defined(ARCH_CPU_X86_FAMILY)

TEST_F(SystemSnapshotMacTest, CPUX86SupportsDAZ) {
  // All x86-family CPUs that Apple is known to have shipped should support DAZ.
  EXPECT_TRUE(system_snapshot().CPUX86SupportsDAZ());
}

#endif

TEST_F(SystemSnapshotMacTest, GetOperatingSystem) {
  EXPECT_EQ(SystemSnapshot::kOperatingSystemMacOSX,
            system_snapshot().GetOperatingSystem());
}

TEST_F(SystemSnapshotMacTest, OSVersion) {
  int major;
  int minor;
  int bugfix;
  std::string build;
  system_snapshot().OSVersion(&major, &minor, &bugfix, &build);

  EXPECT_EQ(10, major);
  EXPECT_EQ(MacOSXMinorVersion(), minor);
  EXPECT_FALSE(build.empty());
}

TEST_F(SystemSnapshotMacTest, OSVersionFull) {
  EXPECT_FALSE(system_snapshot().OSVersionFull().empty());
}

TEST_F(SystemSnapshotMacTest, MachineDescription) {
  EXPECT_FALSE(system_snapshot().MachineDescription().empty());
}

TEST_F(SystemSnapshotMacTest, TimeZone) {
  SystemSnapshot::DaylightSavingTimeStatus dst_status;
  int standard_offset_seconds;
  int daylight_offset_seconds;
  std::string standard_name;
  std::string daylight_name;

  system_snapshot().TimeZone(&dst_status,
                             &standard_offset_seconds,
                             &daylight_offset_seconds,
                             &standard_name,
                             &daylight_name);

  // |standard_offset_seconds| gives seconds east of UTC, and |timezone| gives
  // seconds west of UTC.
  EXPECT_EQ(-timezone, standard_offset_seconds);

  // In contemporary usage, most time zones have an integer hour offset from
  // UTC, although several are at a half-hour offset, and two are at 15-minute
  // offsets. Throughout history, other variations existed. See
  // http://www.timeanddate.com/time/time-zones-interesting.html.
  EXPECT_EQ(0, standard_offset_seconds % (15 * 60))
      << "standard_offset_seconds " << standard_offset_seconds;

  if (dst_status == SystemSnapshot::kDoesNotObserveDaylightSavingTime) {
    EXPECT_EQ(standard_offset_seconds, daylight_offset_seconds);
    EXPECT_EQ(standard_name, daylight_name);
  } else {
    EXPECT_EQ(0, daylight_offset_seconds % (15 * 60))
        << "daylight_offset_seconds " << daylight_offset_seconds;

    // In contemporary usage, dst_delta_seconds will almost always be one hour,
    // except for Lord Howe Island, Australia, which uses a 30-minute
    // delta. Throughout history, other variations existed. See
    // http://www.timeanddate.com/time/dst/#brief.
    int dst_delta_seconds = daylight_offset_seconds - standard_offset_seconds;
    if (dst_delta_seconds != 60 * 60 && dst_delta_seconds != 30 * 60) {
      FAIL() << "dst_delta_seconds " << dst_delta_seconds;
    }

    EXPECT_NE(standard_name, daylight_name);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
