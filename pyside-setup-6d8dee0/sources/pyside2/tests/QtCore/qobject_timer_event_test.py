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

'''Test case for QObject.timerEvent overloading'''

import unittest
from time import sleep
from PySide2.QtCore import QObject, QCoreApplication

from helper import UsesQCoreApplication

class Dummy(QObject):

    def __init__(self, app):
        super(Dummy, self).__init__()
        self.times_called = 0
        self.app = app

    def timerEvent(self, event):
        QObject.timerEvent(self, event)
        event.accept()
        self.times_called += 1

        if self.times_called == 5:
            self.app.exit(0)

class QObjectTimerEvent(UsesQCoreApplication):

    def setUp(self):
        #Acquire resources
        super(QObjectTimerEvent, self).setUp()

    def tearDown(self):
        #Release resources
        super(QObjectTimerEvent, self).tearDown()

    def testTimerEvent(self):
        #QObject.timerEvent overloading
        obj = Dummy(self.app)
        timer_id = obj.startTimer(200)
        self.app.exec_()
        obj.killTimer(timer_id)
        self.assertEqual(obj.times_called, 5)

if __name__ == '__main__':
    unittest.main()
