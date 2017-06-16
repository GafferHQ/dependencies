#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for include.IncludeNode'''

import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import os
import StringIO
import unittest

from grit.node import misc
from grit.node import include
from grit.node import empty
from grit import grd_reader
from grit import util


class IncludeNodeUnittest(unittest.TestCase):
  def testGetPath(self):
    root = misc.GritNode()
    root.StartParsing(u'grit', None)
    root.HandleAttribute(u'latest_public_release', u'0')
    root.HandleAttribute(u'current_release', u'1')
    root.HandleAttribute(u'base_dir', ur'..\resource')
    release = misc.ReleaseNode()
    release.StartParsing(u'release', root)
    release.HandleAttribute(u'seq', u'1')
    root.AddChild(release)
    includes = empty.IncludesNode()
    includes.StartParsing(u'includes', release)
    release.AddChild(includes)
    include_node = include.IncludeNode()
    include_node.StartParsing(u'include', includes)
    include_node.HandleAttribute(u'file', ur'flugel\kugel.pdf')
    includes.AddChild(include_node)
    root.EndParsing()

    self.assertEqual(root.ToRealPath(include_node.GetInputPath()),
                     util.normpath(
                       os.path.join(ur'../resource', ur'flugel/kugel.pdf')))

  def testGetPathNoBasedir(self):
    root = misc.GritNode()
    root.StartParsing(u'grit', None)
    root.HandleAttribute(u'latest_public_release', u'0')
    root.HandleAttribute(u'current_release', u'1')
    root.HandleAttribute(u'base_dir', ur'..\resource')
    release = misc.ReleaseNode()
    release.StartParsing(u'release', root)
    release.HandleAttribute(u'seq', u'1')
    root.AddChild(release)
    includes = empty.IncludesNode()
    includes.StartParsing(u'includes', release)
    release.AddChild(includes)
    include_node = include.IncludeNode()
    include_node.StartParsing(u'include', includes)
    include_node.HandleAttribute(u'file', ur'flugel\kugel.pdf')
    include_node.HandleAttribute(u'use_base_dir', u'false')
    includes.AddChild(include_node)
    root.EndParsing()

    self.assertEqual(root.ToRealPath(include_node.GetInputPath()),
                     util.normpath(
                       os.path.join(ur'../', ur'flugel/kugel.pdf')))


if __name__ == '__main__':
  unittest.main()
