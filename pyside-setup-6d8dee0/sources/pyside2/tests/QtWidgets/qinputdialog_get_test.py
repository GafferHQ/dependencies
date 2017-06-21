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

from PySide2 import QtCore, QtWidgets
from helper import UsesQApplication, TimedQApplication

class TestInputDialog(TimedQApplication):

    def testGetDouble(self):
        self.assertEqual(QtWidgets.QInputDialog.getDouble(None, "title", "label"), (0.0, False))

    def testGetInt(self):
        self.assertEqual(QtWidgets.QInputDialog.getInt(None, "title", "label"), (0, False))

    def testGetItem(self):
        (item, bool) = QtWidgets.QInputDialog.getItem(None, "title", "label", ["1", "2", "3"])
        self.assertEqual(str(item), "1")

    def testGetText(self):
        (text, bool) = QtWidgets.QInputDialog.getText(None, "title", "label")
        self.assertEqual(str(text),"")

if __name__ == '__main__':
    unittest.main()

