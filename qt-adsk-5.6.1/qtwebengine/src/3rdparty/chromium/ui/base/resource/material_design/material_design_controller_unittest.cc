// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/base/ui_base_switches.h"

namespace ui {
namespace {

// Test fixture for the MaterialDesignController class.
class MaterialDesignControllerTest : public testing::Test {
 public:
  MaterialDesignControllerTest();
  ~MaterialDesignControllerTest() override;

  // testing::Test:
  void TearDown() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MaterialDesignControllerTest);
};

MaterialDesignControllerTest::MaterialDesignControllerTest() {
}

MaterialDesignControllerTest::~MaterialDesignControllerTest() {
}

void MaterialDesignControllerTest::TearDown() {
  testing::Test::TearDown();

  // Ensure other tests aren't polluted by a Mode set in these tests.
  test::MaterialDesignControllerTestAPI::UninitializeMode();
}

#if !defined(ENABLE_TOPCHROME_MD)

// Verify the Mode maps to Mode::NON_MATERIAL when the compile time flag is not
// defined.
TEST_F(MaterialDesignControllerTest,
       NonMaterialModeWhenCompileTimeFlagDisabled) {
  EXPECT_EQ(MaterialDesignController::Mode::NON_MATERIAL,
            MaterialDesignController::GetMode());
}

#else

// Verify command line value "material" maps to Mode::MATERIAL when the compile
// time flag is defined.
TEST_F(MaterialDesignControllerTest,
       EnabledCommandLineValueMapsToMaterialModeWhenCompileTimeFlagEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTopChromeMD, "material");
  EXPECT_EQ(MaterialDesignController::Mode::MATERIAL_NORMAL,
            MaterialDesignController::GetMode());
}

// Verify command line value "material-hybrid" maps to Mode::MATERIAL_HYBRID
// when the compile time flag is defined.
TEST_F(
    MaterialDesignControllerTest,
    EnabledHybridCommandLineValueMapsToMaterialHybridModeWhenCompileTimeFlagEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTopChromeMD, "material-hybrid");
  EXPECT_EQ(MaterialDesignController::Mode::MATERIAL_HYBRID,
            MaterialDesignController::GetMode());
}

// Verify command line value "" maps to Mode::NON_MATERIAL when the compile time
// flag is defined.
TEST_F(
    MaterialDesignControllerTest,
    DisabledCommandLineValueMapsToNonMaterialModeWhenCompileTimeFlagEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTopChromeMD, "");
  EXPECT_EQ(MaterialDesignController::Mode::NON_MATERIAL,
            MaterialDesignController::GetMode());
}

// Verify no command line value maps to Mode::NON_MATERIAL when the compile time
// flag is defined.
TEST_F(MaterialDesignControllerTest,
       NoCommandLineValueMapsToNonMaterialModeWhenCompileTimeFlagEnabled) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTopChromeMD));
  EXPECT_EQ(MaterialDesignController::Mode::NON_MATERIAL,
            MaterialDesignController::GetMode());
}

// Verify an invalid command line value uses the default NON_MATERIAL mode.
TEST_F(MaterialDesignControllerTest, InvalidCommandLineValue) {
  const std::string kInvalidValue = "1nvalid-valu3";
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTopChromeMD, kInvalidValue);
  EXPECT_EQ(MaterialDesignController::Mode::NON_MATERIAL,
            MaterialDesignController::GetMode());
}

// Verify that MaterialDesignController::IsModeMaterial() will initialize the
// mode if it hasn't been initialized yet.
TEST_F(MaterialDesignControllerTest, IsModeMaterialInitializesMode) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kTopChromeMD, "material");
  EXPECT_TRUE(MaterialDesignController::IsModeMaterial());
}

#endif  // !defined(ENABLE_TOPCHROME_MD)

}  // namespace
}  // namespace ui
