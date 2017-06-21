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

'''Test cases for QTimer.singleShot'''

import unittest

from PySide2.QtCore import QObject, QTimer, QCoreApplication, Signal
from helper import UsesQCoreApplication

class WatchDog(QObject):
    '''Exits the QCoreApplication main loop after sometime.'''

    def __init__(self, watched):
        QObject.__init__(self)
        self.times_called = 0
        self.watched = watched

    def timerEvent(self, evt):
        self.times_called += 1
        if self.times_called == 20:
            self.watched.exit_app_cb()

class TestSingleShot(UsesQCoreApplication):
    '''Test case for QTimer.singleShot'''

    def setUp(self):
        #Acquire resources
        UsesQCoreApplication.setUp(self)
        self.watchdog = WatchDog(self)
        self.called = False

    def tearDown(self):
        #Release resources
        del self.watchdog
        del self.called
        UsesQCoreApplication.tearDown(self)

    def callback(self):
        self.called = True
        self.app.quit()

    def testSingleShot(self):
        QTimer.singleShot(100, self.callback)
        self.app.exec_()
        self.assertTrue(self.called)

class SigEmitter(QObject):

    sig1 = Signal()


class TestSingleShotSignal(UsesQCoreApplication):
    '''Test case for QTimer.singleShot connecting to signals'''

    def setUp(self):
        UsesQCoreApplication.setUp(self)
        self.watchdog = WatchDog(self)
        self.called = False

    def tearDown(self):
        del self.watchdog
        del self.called
        UsesQCoreApplication.tearDown(self)

    def callback(self):
        self.called = True
        self.app.quit()

    def testSingleShotSignal(self):
        emitter = SigEmitter()
        emitter.sig1.connect(self.callback)
        QTimer.singleShot(100, emitter.sig1)
        self.app.exec_()
        self.assertTrue(self.called)

if __name__ == '__main__':
    unittest.main()

