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

'''Test cases for overloads involving static and non-static versions of a method.'''

import os
import unittest

from PySide2.QtCore import QFile

class StaticNonStaticMethodsTest(unittest.TestCase):
    '''Test cases for overloads involving static and non-static versions of a method.'''

    def setUp(self):
        filename = 'somefile%d.txt' % os.getpid()
        self.existing_filename = os.path.join(os.path.curdir, filename)
        self.delete_file = False
        if not os.path.exists(self.existing_filename):
            f = open(self.existing_filename, 'w')
            for line in range(10):
                f.write('sbrubbles\n')
            f.close()
            self.delete_file = True

        self.non_existing_filename = os.path.join(os.path.curdir, 'inexistingfile.txt')
        i = 0
        while os.path.exists(self.non_existing_filename):
            i += 1
            filename = 'inexistingfile-%d.txt' % i
            self.non_existing_filename = os.path.join(os.path.curdir, filename)

    def tearDown(self):
        if self.delete_file:
            os.remove(self.existing_filename)

    def testCallingStaticMethodWithClass(self):
        '''Call static method using class.'''
        self.assertTrue(QFile.exists(self.existing_filename))
        self.assertFalse(QFile.exists(self.non_existing_filename))

    def testCallingStaticMethodWithInstance(self):
        '''Call static method using instance of class.'''
        f = QFile(self.non_existing_filename)
        self.assertTrue(f.exists(self.existing_filename))
        self.assertFalse(f.exists(self.non_existing_filename))

    def testCallingInstanceMethod(self):
        '''Call instance method.'''
        f1 = QFile(self.non_existing_filename)
        self.assertFalse(f1.exists())
        f2 = QFile(self.existing_filename)
        self.assertTrue(f2.exists())


if __name__ == '__main__':
    unittest.main()

