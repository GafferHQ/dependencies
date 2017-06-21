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
import sys
import weakref

from PySide2 import QtWidgets
from PySide2 import QtCore

from helper import UsesQApplication

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self):
        QtWidgets.QMainWindow.__init__(self)

        self.createToolbar()

    def createToolbar(self):
        pointerButton = QtWidgets.QToolButton()
        pointerToolbar = self.addToolBar("Pointer type")
        pointerToolbar.addWidget(pointerButton)

class MyButton(QtWidgets.QPushButton):
    def __init__(self, parent=None):
        QtWidgets.QPushButton.__init__(self)
        self._called = False

    def myCallback(self):
        self._called = True


class TestMainWindow(UsesQApplication):

    def testCreateToolbar(self):
        w = MainWindow()
        w.show()
        QtCore.QTimer.singleShot(1000, self.app.quit)
        self.app.exec_()

    def objDel(self, obj):
        self.app.quit()

    def testRefCountToNull(self):
        w = QtWidgets.QMainWindow()
        c = QtWidgets.QWidget()
        self.assertEqual(sys.getrefcount(c), 2)
        w.setCentralWidget(c)
        self.assertEqual(sys.getrefcount(c), 3)
        wr = weakref.ref(c, self.objDel)
        w.setCentralWidget(None)
        c = None
        self.app.exec_()

    def testRefCountToAnother(self):
        w = QtWidgets.QMainWindow()
        c = QtWidgets.QWidget()
        self.assertEqual(sys.getrefcount(c), 2)
        w.setCentralWidget(c)
        self.assertEqual(sys.getrefcount(c), 3)

        c2 = QtWidgets.QWidget()
        w.setCentralWidget(c2)
        self.assertEqual(sys.getrefcount(c2), 3)

        wr = weakref.ref(c, self.objDel)
        w.setCentralWidget(None)
        c = None

        self.app.exec_()

    def testSignalDisconect(self):
        w = QtWidgets.QMainWindow()
        b = MyButton("button")
        b.clicked.connect(b.myCallback)
        w.setCentralWidget(b)

        b = MyButton("button")
        b.clicked.connect(b.myCallback)
        w.setCentralWidget(b)

        b.click()
        self.assertEqual(b._called, True)


if __name__ == '__main__':
    unittest.main()

