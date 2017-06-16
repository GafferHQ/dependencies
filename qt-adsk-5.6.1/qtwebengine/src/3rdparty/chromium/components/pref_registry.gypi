# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/pref_registry
      'target_name': 'pref_registry',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'PREF_REGISTRY_IMPLEMENTATION',
      ],
      'sources': [
        'pref_registry/pref_registry_export.h',
        'pref_registry/pref_registry_syncable.cc',
        'pref_registry/pref_registry_syncable.h',
      ],
    },
    {
      # GN version: //components/pref_registry:test_support
      'target_name': 'pref_registry_test_support',
      'type': 'static_library',
      'dependencies': [
        'pref_registry',
        '../base/base.gyp:base_prefs_test_support',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'pref_registry/testing_pref_service_syncable.cc',
        'pref_registry/testing_pref_service_syncable.h',
      ],
    },
  ],
}
