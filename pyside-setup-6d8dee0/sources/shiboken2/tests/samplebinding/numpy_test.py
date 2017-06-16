#############################################################################
##
## Copyright (C) 2016 The Qt Company Ltd.
## Contact: https://www.qt.io/licensing/
##
## This file is part of the test suite of PySide2.
##
## $QT_BEGIN_LICENSE:GPL-EXCEPT$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and The Qt Company. For licensing terms
## and conditions see https://www.qt.io/terms-conditions. For further
## information use the contact form at https://www.qt.io/contact-us.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3 as published by the Free Software
## Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
## included in the packaging of this file. Please review the following
## information to ensure the GNU General Public License requirements will
## be met: https://www.gnu.org/licenses/gpl-3.0.html.
##
## $QT_END_LICENSE$
##
#############################################################################

import sys

try:
    from distutils import sysconfig
    if bool(sysconfig.get_config_var('Py_DEBUG')):
        sys.exit(0)
    import numpy
except:
    sys.exit(0)

import unittest
from sample import PointF

class TestNumpyTypes(unittest.TestCase):

    def testNumpyConverted(self):
        x, y = (0.1, 0.2)
        p = PointF(float(numpy.float32(x)), float(numpy.float32(y)))
        self.assertAlmostEqual(p.x(), x)
        self.assertAlmostEqual(p.y(), y)

    def testNumpyAsIs(self):
        x, y = (0.1, 0.2)
        p = PointF(numpy.float32(x), numpy.float32(y))
        self.assertAlmostEqual(p.x(), x)
        self.assertAlmostEqual(p.y(), y)

if __name__ == "__main__":
    unittest.main()

