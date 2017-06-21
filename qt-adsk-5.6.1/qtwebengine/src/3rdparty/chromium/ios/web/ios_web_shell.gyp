# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios_web_shell',
      'type': 'executable',
      'mac_bundle': 1,
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        'ios_web.gyp:ios_web',
        'ios_web.gyp:ios_web_app',
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../ui/base/ui_base.gyp:ui_base',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'shell/Info.plist',
        'OTHER_LDFLAGS': [
          '-Xlinker -objc_abi_version -Xlinker 2'
        ]
      },
      'sources': [
        'shell/app_delegate.h',
        'shell/app_delegate.mm',
        'shell/shell_browser_state.h',
        'shell/shell_browser_state.mm',
        'shell/shell_main_delegate.h',
        'shell/shell_main_delegate.mm',
        'shell/shell_network_delegate.cc',
        'shell/shell_network_delegate.h',
        'shell/shell_url_request_context_getter.cc',
        'shell/shell_url_request_context_getter.h',
        'shell/shell_web_client.h',
        'shell/shell_web_client.mm',
        'shell/shell_web_main_parts.h',
        'shell/shell_web_main_parts.mm',
        'shell/view_controller.h',
        'shell/view_controller.mm',
        'shell/web_exe_main.mm',
      ],
      'mac_bundle_resources': [
        'shell/Default.png',
        'shell/MainView.xib',
        'shell/textfield_background@2x.png',
        'shell/toolbar_back@2x.png',
        'shell/toolbar_forward@2x.png',
      ],
      'link_settings': {
        'libraries': [
          '$(SDKROOT)/System/Library/Frameworks/CoreGraphics.framework',
          '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
        ],
      },
    },
  ],
}
