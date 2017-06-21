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

''' Forced disconnection: Delete one end of the signal connection'''

import unittest
from sys import getrefcount

from PySide2.QtCore import QObject, SIGNAL, SLOT

class Dummy(QObject):
    def dispatch(self):
        self.emit(SIGNAL('foo()'))

class PythonSignalRefCount(unittest.TestCase):

    def setUp(self):
        self.emitter = Dummy()

    def tearDown(self):
        self.emitter

    def testRefCount(self):
        def cb(*args):
            pass

        self.assertEqual(getrefcount(cb), 2)

        QObject.connect(self.emitter, SIGNAL('foo()'), cb)
        self.assertEqual(getrefcount(cb), 3)

        QObject.disconnect(self.emitter, SIGNAL('foo()'), cb)
        self.assertEqual(getrefcount(cb), 2)

class CppSignalRefCount(unittest.TestCase):

    def setUp(self):
        self.emitter = QObject()

    def tearDown(self):
        self.emitter

    def testRefCount(self):
        def cb(*args):
            pass

        self.assertEqual(getrefcount(cb), 2)

        QObject.connect(self.emitter, SIGNAL('destroyed()'), cb)
        self.assertEqual(getrefcount(cb), 3)

        QObject.disconnect(self.emitter, SIGNAL('destroyed()'), cb)
        self.assertEqual(getrefcount(cb), 2)

if __name__ == '__main__':
    unittest.main()
