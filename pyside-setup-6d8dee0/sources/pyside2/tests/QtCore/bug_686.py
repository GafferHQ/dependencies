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

from __future__ import with_statement

import unittest
from PySide2.QtCore import *

class MyWriteThread(QThread):
    def __init__(self, lock):
        QThread.__init__(self)
        self.lock = lock
        self.started = False
        self.canQuit = False

    def run(self):
        self.started = True
        while not self.lock.tryLockForWrite():
            pass
        self.canQuit = True

class MyReadThread(QThread):
    def __init__(self, lock):
        QThread.__init__(self)
        self.lock = lock
        self.started = False
        self.canQuit = False

    def run(self):
        self.started = True
        while not self.lock.tryLockForRead():
            pass
        self.canQuit = True

class MyMutexedThread(QThread):
    def __init__(self, mutex):
        QThread.__init__(self)
        self.mutex = mutex
        self.started = False
        self.canQuit = False

    def run(self):
        self.started = True
        while not self.mutex.tryLock():
            pass
        self.mutex.unlock()
        self.canQuit = True

class TestQMutex (unittest.TestCase):

    def testReadLocker(self):
        lock = QReadWriteLock()
        thread = MyWriteThread(lock)

        with QReadLocker(lock):
            thread.start()
            while not thread.started:
                pass
            self.assertFalse(thread.canQuit)

        thread.wait(2000)
        self.assertTrue(thread.canQuit)

    def testWriteLocker(self):
        lock = QReadWriteLock()
        thread = MyReadThread(lock)

        with QWriteLocker(lock):
            thread.start()
            while not thread.started:
                pass
            self.assertFalse(thread.canQuit)

        thread.wait(2000)
        self.assertTrue(thread.canQuit)

    def testMutexLocker(self):
        mutex = QMutex()
        thread = MyMutexedThread(mutex)

        with QMutexLocker(mutex):
            thread.start()
            while not thread.started:
                pass
            self.assertFalse(thread.canQuit)

        thread.wait(2000)
        self.assertTrue(thread.canQuit)

if __name__ == '__main__':
    unittest.main()
