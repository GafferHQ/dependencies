# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
   },
  'targets': [
    {
      'target_name': 'ios_provider_chrome_browser',
      'type': 'static_library',
      'sources': [
        '../public/provider/chrome/browser/browser_state/browser_state_info_cache.cc',
        '../public/provider/chrome/browser/browser_state/browser_state_info_cache.h',
        '../public/provider/chrome/browser/browser_state/browser_state_info_cache_observer.h',
        '../public/provider/chrome/browser/browser_state/chrome_browser_state.cc',
        '../public/provider/chrome/browser/browser_state/chrome_browser_state.h',
        '../public/provider/chrome/browser/browser_state/chrome_browser_state_manager.h',
        '../public/provider/chrome/browser/chrome_browser_provider.cc',
        '../public/provider/chrome/browser/chrome_browser_provider.h',
        '../public/provider/chrome/browser/geolocation_updater_provider.h',
        '../public/provider/chrome/browser/geolocation_updater_provider.mm',
        '../public/provider/chrome/browser/keyed_service_provider.cc',
        '../public/provider/chrome/browser/keyed_service_provider.h',
        '../public/provider/chrome/browser/string_provider.h',
        '../public/provider/chrome/browser/ui/infobar_view_delegate.h',
        '../public/provider/chrome/browser/ui/infobar_view_protocol.h',
        '../public/provider/chrome/browser/updatable_resource_provider.h',
        '../public/provider/chrome/browser/updatable_resource_provider.mm',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../components/components.gyp:autofill_core_browser',
      ],
    },
  ],
}
