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

from PySide2.QtCore import Qt, QEvent
from PySide2.QtGui import QKeyEvent, QKeySequence
from PySide2.QtWidgets import QApplication
import unittest


class TestBug569(unittest.TestCase):

    def testIt(self):
        # We need a qapp otherwise Qt will crash when trying to detect the
        # current platform
        app = QApplication([])
        ev1 = QKeyEvent(QEvent.KeyRelease, Qt.Key_Delete, Qt.NoModifier)
        ev2 = QKeyEvent(QEvent.KeyRelease, Qt.Key_Copy, Qt.NoModifier)
        ks = QKeySequence.Delete

        self.assertEqual(ev1, ks)
        self.assertEqual(ks, ev1)
        self.assertNotEqual(ev2, ks)
        self.assertNotEqual(ks, ev2)

if __name__ == '__main__':
    unittest.main()
