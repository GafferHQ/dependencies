#!/usr/bin/python
# -*- coding: utf-8 -*-

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

from helper import TimedQApplication

from PySide2.QtQuick import QQuickView, QQuickItem

class TestBug915(TimedQApplication):
    def testReturnPolicy(self):
        view = QQuickView()

        item1 = QQuickItem()
        item1.setObjectName("Item1")
        item1.setParentItem(view.contentItem())
        self.assertEqual(item1.objectName(), "Item1") # check if the item still valid

        item2 = QQuickItem()
        item2.setObjectName("Item2")
        item2.setParentItem(view.contentItem())
        item1 = None
        self.assertEqual(item2.objectName(), "Item2") # check if the item still valid

        view = None

if __name__ == '__main__':
    unittest.main()


