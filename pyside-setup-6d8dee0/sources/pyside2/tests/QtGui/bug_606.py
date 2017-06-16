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

import PySide2
from PySide2.QtGui import QVector2D, QVector3D, QVector4D
from PySide2.QtGui import QColor

class testCases(unittest.TestCase):
    def testQVector2DToTuple(self):
        vec = QVector2D(1, 2)
        self.assertEqual((1, 2), vec.toTuple())

    def testQVector3DToTuple(self):
        vec = QVector3D(1, 2, 3)
        self.assertEqual((1, 2, 3), vec.toTuple())

    def testQVector4DToTuple(self):
        vec = QVector4D(1, 2, 3, 4)
        self.assertEqual((1, 2, 3, 4), vec.toTuple())

    def testQColorToTuple(self):
        c = QColor(0, 0, 255)
        c.setRgb(1, 2, 3)
        self.assertEqual((1, 2, 3, 255), c.toTuple())

if __name__ == '__main__':
    unittest.main()
