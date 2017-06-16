# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Add your directory-specific .gyp file to this list for it to be continuously
# typechecked on the builder:
# http://build.chromium.org/p/chromium.fyi/builders/Closure%20Compilation%20Linux
#
# Also, see our guide to Closure compilation in chrome:
# https://code.google.com/p/chromium/wiki/ClosureCompilation
{
  'targets': [
    {
      'target_name': 'compile_all_resources',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/chrome/browser/resources/downloads/compiled_resources2.gyp:*',
        '<(DEPTH)/chrome/browser/resources/md_downloads/compiled_resources2.gyp:*',
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:*',
        '<(DEPTH)/ui/webui/resources/js/cr/ui/compiled_resources2.gyp:*',
      ],
    },
  ]
}
