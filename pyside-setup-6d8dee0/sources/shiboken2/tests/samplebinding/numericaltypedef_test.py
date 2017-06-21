#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
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

import unittest

from sample import SizeF

class NumericalTypedefTest(unittest.TestCase):

    def testNumericalTypedefExact(self):
        width, height = (1.1, 2.2)
        size = SizeF(width, height)
        self.assertEqual(size.width(), width)
        self.assertEqual(size.height(), height)

    def testNumericalTypedefConvertible(self):
        width, height = (1, 2)
        size = SizeF(width, height)
        self.assertEqual(size.width(), float(width))
        self.assertEqual(size.height(), float(height))

    def testNumericalTypedefOfUnsignedShort(self):
        self.assertEqual(SizeF.passTypedefOfUnsignedShort(123), 123)
        self.assertEqual(SizeF.passTypedefOfUnsignedShort(321), 321)
        self.assertNotEqual(SizeF.passTypedefOfUnsignedShort(123), 0)

if __name__ == '__main__':
    unittest.main()
