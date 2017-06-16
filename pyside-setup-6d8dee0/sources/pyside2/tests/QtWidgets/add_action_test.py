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

'''Tests for QMenuBar.addAction(identifier, callback) calls'''

import unittest

from PySide2.QtCore import SLOT
from PySide2.QtWidgets import QMenuBar, QAction, QPushButton

from helper import UsesQApplication


class AddActionTest(UsesQApplication):
    '''QMenuBar addAction'''

    def tearDown(self):
        try:
            del self.called
        except AttributeError:
            pass
        super(AddActionTest, self).tearDown()

    def _callback(self):
        self.called = True

    def testBasic(self):
        '''QMenuBar.addAction(id, callback)'''
        menubar = QMenuBar()
        action = menubar.addAction("Accounts", self._callback)
        action.activate(QAction.Trigger)
        self.assertTrue(self.called)

    def testWithCppSlot(self):
        '''QMenuBar.addAction(id, object, slot)'''
        menubar = QMenuBar()
        widget = QPushButton()
        widget.setCheckable(True)
        widget.setChecked(False)
        action = menubar.addAction("Accounts", widget, SLOT("toggle()"))
        action.activate(QAction.Trigger)
        self.assertTrue(widget.isChecked())

if __name__ == '__main__':
    unittest.main()

