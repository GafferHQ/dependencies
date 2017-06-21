#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
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

'''Test cases for virtual destructor.'''

import sys
import unittest

from sample import VirtualDtor

class ExtendedVirtualDtor(VirtualDtor):
    def __init__(self):
        VirtualDtor.__init__(self)

class VirtualDtorTest(unittest.TestCase):
    '''Test case for virtual destructor.'''

    def setUp(self):
        VirtualDtor.resetDtorCounter()

    def testVirtualDtor(self):
        '''Original virtual destructor is being called.'''
        dtor_called = VirtualDtor.dtorCalled()
        for i in range(1, 10):
            vd = VirtualDtor()
            del vd
            self.assertEqual(VirtualDtor.dtorCalled(), dtor_called + i)

    def testVirtualDtorOnCppCreatedObject(self):
        '''Original virtual destructor is being called for a C++ created object.'''
        dtor_called = VirtualDtor.dtorCalled()
        for i in range(1, 10):
            vd = VirtualDtor.create()
            del vd
            self.assertEqual(VirtualDtor.dtorCalled(), dtor_called + i)

    def testDtorOnDerivedClass(self):
        '''Original virtual destructor is being called for a derived class.'''
        dtor_called = ExtendedVirtualDtor.dtorCalled()
        for i in range(1, 10):
            evd = ExtendedVirtualDtor()
            del evd
            self.assertEqual(ExtendedVirtualDtor.dtorCalled(), dtor_called + i)


if __name__ == '__main__':
    unittest.main()

