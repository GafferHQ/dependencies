#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import os
import subprocess

chromium_root_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                 os.pardir, os.pardir)

def commit(message, cwd=None):
  subprocess.call(['git', 'commit', '-a', '-m', message], cwd=cwd)

def system(command, cwd=None):
  return subprocess.check_output(command, cwd=cwd)

def find(patterns, start='.'):
  for path, dirs, files in os.walk(start):
    for basename in files + dirs:
      if any([fnmatch.fnmatch(basename, pattern) for pattern in patterns]):
        filename = os.path.join(path, basename)
        yield filename

def filter_file(path, predicate):
  with open(path, 'r+') as f:
    lines = f.readlines()
    new_lines = [line for line in lines if predicate(line)]
    f.seek(0)
    f.truncate()
    f.write(''.join(new_lines))
