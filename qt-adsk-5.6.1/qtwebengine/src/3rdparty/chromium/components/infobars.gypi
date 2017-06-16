# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/infobars/core
      'target_name': 'infobars_core',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/strings/ui_strings.gyp:ui_strings',
      ],
      'export_dependent_settings': [
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        # Note: sources duplicated in GN build.
        'infobars/core/confirm_infobar_delegate.cc',
        'infobars/core/confirm_infobar_delegate.h',
        'infobars/core/infobar.cc',
        'infobars/core/infobar.h',
        'infobars/core/infobar_container.cc',
        'infobars/core/infobar_container.h',
        'infobars/core/infobar_delegate.cc',
        'infobars/core/infobar_delegate.h',
        'infobars/core/infobar_manager.cc',
        'infobars/core/infobar_manager.h',
        'infobars/core/infobars_switches.cc',
        'infobars/core/infobars_switches.h',
        'infobars/core/simple_alert_infobar_delegate.cc',
        'infobars/core/simple_alert_infobar_delegate.h',
      ],
    },
  ],
}
