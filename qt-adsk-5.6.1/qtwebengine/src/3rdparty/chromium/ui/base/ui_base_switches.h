// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/base.

#ifndef UI_BASE_UI_BASE_SWITCHES_H_
#define UI_BASE_UI_BASE_SWITCHES_H_

#include "base/compiler_specific.h"
#include "ui/base/ui_base_export.h"

namespace switches {

#if defined(OS_MACOSX) && !defined(OS_IOS)
UI_BASE_EXPORT extern const char kDisableRemoteCoreAnimation[];
UI_BASE_EXPORT extern const char kDisableNSCGLSurfaceApi[];
UI_BASE_EXPORT extern const char kForceNSCGLSurfaceApi[];
#endif

UI_BASE_EXPORT extern const char kDisableDwmComposition[];
UI_BASE_EXPORT extern const char kDisableIconNtp[];
UI_BASE_EXPORT extern const char kDisableTouchAdjustment[];
UI_BASE_EXPORT extern const char kDisableTouchDragDrop[];
UI_BASE_EXPORT extern const char kDisableTouchEditing[];
UI_BASE_EXPORT extern const char kDisableTouchFeedback[];
UI_BASE_EXPORT extern const char kEnableIconNtp[];
UI_BASE_EXPORT extern const char kEnableLinkDisambiguationPopup[];
UI_BASE_EXPORT extern const char kEnableTouchDragDrop[];
UI_BASE_EXPORT extern const char kEnableTouchEditing[];
UI_BASE_EXPORT extern const char kLang[];

#if defined(ENABLE_TOPCHROME_MD)
UI_BASE_EXPORT extern const char kTopChromeMD[];
UI_BASE_EXPORT extern const char kTopChromeMDMaterial[];
UI_BASE_EXPORT extern const char kTopChromeMDMaterialHybrid[];
UI_BASE_EXPORT extern const char kTopChromeMDNonMaterial[];
#endif  // defined(ENABLE_TOPCHROME_MD)

UI_BASE_EXPORT extern const char kViewerConnect[];

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_H_
