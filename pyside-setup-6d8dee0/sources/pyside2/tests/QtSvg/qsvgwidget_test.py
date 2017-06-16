#!/usr/bin/python

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

import unittest, os
from PySide2.QtCore import *
from PySide2.QtWidgets import *
from PySide2.QtSvg import *

class QSvgWidgetTest(unittest.TestCase):

    def testLoad(self):
        tigerPath = os.path.join(os.path.dirname(__file__), 'tiger.svg')

        app = QApplication([])
        fromFile = QSvgWidget()
        fromFile.load(tigerPath)
        self.assertTrue(fromFile.renderer().isValid())

        tigerFile = QFile(tigerPath)
        tigerFile.open(QFile.ReadOnly)
        tigerData = tigerFile.readAll()
        fromContents = QSvgWidget()
        fromContents.load(tigerData)
        self.assertTrue(fromContents.renderer().isValid())

if __name__ == '__main__':
    unittest.main()

