// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/input_controller.h"

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace ui {

namespace {

class StubInputController : public InputController {
 public:
  StubInputController();
  ~StubInputController() override;

  // InputController:
  bool HasMouse() override;
  bool HasTouchpad() override;
  bool IsCapsLockEnabled() override;
  void SetCapsLockEnabled(bool enabled) override;
  void SetNumLockEnabled(bool enabled) override;
  bool IsAutoRepeatEnabled() override;
  void SetAutoRepeatEnabled(bool enabled) override;
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override;
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override;
  void SetTouchEventLoggingEnabled(bool enabled) override;
  void SetTouchpadSensitivity(int value) override;
  void SetTapToClick(bool enabled) override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTapDragging(bool enabled) override;
  void SetNaturalScroll(bool enabled) override;
  void SetMouseSensitivity(int value) override;
  void SetPrimaryButtonRight(bool right) override;
  void SetTapToClickPaused(bool state) override;
  void GetTouchDeviceStatus(const GetTouchDeviceStatusReply& reply) override;
  void GetTouchEventLog(const base::FilePath& out_dir,
                        const GetTouchEventLogReply& reply) override;
  void SetInternalTouchpadEnabled(bool enabled) override;
  void SetInternalKeyboardFilter(bool enable_filter,
                                 std::vector<DomCode> allowed_keys) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StubInputController);
};

StubInputController::StubInputController() {
}

StubInputController::~StubInputController() {
}

bool StubInputController::HasMouse() {
  return false;
}

bool StubInputController::HasTouchpad() {
  return false;
}

bool StubInputController::IsCapsLockEnabled() {
  return false;
}

void StubInputController::SetCapsLockEnabled(bool enabled) {
}

void StubInputController::SetNumLockEnabled(bool enabled) {
}

bool StubInputController::IsAutoRepeatEnabled() {
  return true;
}

void StubInputController::SetAutoRepeatEnabled(bool enabled) {
}

void StubInputController::SetAutoRepeatRate(const base::TimeDelta& delay,
                                            const base::TimeDelta& interval) {
}

void StubInputController::GetAutoRepeatRate(base::TimeDelta* delay,
                                            base::TimeDelta* interval) {
}

void StubInputController::SetTouchpadSensitivity(int value) {
}

void StubInputController::SetTouchEventLoggingEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

void StubInputController::SetTapToClick(bool enabled) {
}

void StubInputController::SetThreeFingerClick(bool enabled) {
}

void StubInputController::SetTapDragging(bool enabled) {
}

void StubInputController::SetNaturalScroll(bool enabled) {
}

void StubInputController::SetMouseSensitivity(int value) {
}

void StubInputController::SetPrimaryButtonRight(bool right) {
}

void StubInputController::SetTapToClickPaused(bool state) {
}

void StubInputController::GetTouchDeviceStatus(
    const GetTouchDeviceStatusReply& reply) {
  reply.Run(scoped_ptr<std::string>(new std::string));
}

void StubInputController::GetTouchEventLog(const base::FilePath& out_dir,
                                           const GetTouchEventLogReply& reply) {
  reply.Run(make_scoped_ptr(new std::vector<base::FilePath>));
}

void StubInputController::SetInternalTouchpadEnabled(bool enabled) {
}

void StubInputController::SetInternalKeyboardFilter(
    bool enable_filter,
    std::vector<DomCode> allowed_keys) {
}

}  // namespace

scoped_ptr<InputController> CreateStubInputController() {
  return make_scoped_ptr(new StubInputController);
}

}  // namespace ui
