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

from PySide2.QtGui import QStandardItemModel, QStandardItem
from PySide2.QtWidgets import QWidget
try:
    # the normal call with installed PySide2
    from PySide2 import shiboken2 as shiboken
except ImportError:
    try:
        # When running make test on macOS, shiboken2 is not part of the PySide2 module,
        # so it needs to be imported as a standalone module.
        import shiboken2 as shiboken
    except ImportError:
        # sys.path is set a bit weird during tests, so we help a little to find shiboken2.
        sys.path.append("../../..")
        # the special call with testrunner.py
        from shiboken2.shibokenmodule import shiboken2 as shiboken
from helper import UsesQApplication


class QStandardItemModelTest(UsesQApplication):

    def setUp(self):
       super(QStandardItemModelTest, self).setUp()
       self.window = QWidget()
       self.model = QStandardItemModel(0, 3, self.window)

    def tearDown(self):
       del self.window
       del self.model
       super(QStandardItemModelTest, self).tearDown()

    def testInsertRow(self):
        # bug #227
        self.model.insertRow(0)

    def testClear(self):

        model = QStandardItemModel()
        root = model.invisibleRootItem()
        model.clear()
        self.assertFalse(shiboken.isValid(root))


class QStandardItemModelRef(UsesQApplication):
    def testRefCount(self):
        model = QStandardItemModel(5, 5)
        items = []
        for r in range(5):
            row = []
            for c in range(5):
                row.append(QStandardItem("%d,%d" % (r,c)) )
                self.assertEqual(sys.getrefcount(row[c]), 2)

            model.insertRow(r, row)

            for c in range(5):
                ref_after = sys.getrefcount(row[c])
                # check if the ref count was incremented after insertRow
                self.assertEqual(ref_after, 3)

            items.append(row)
            row = None

        for r in range(3):
            my_row = model.takeRow(0)
            my_row = None
            for c in range(5):
                # only rest 1 reference
                self.assertEqual(sys.getrefcount(items[r][c]), 2)

        my_i = model.item(0,0)
        # ref(my_i) + parent_ref + items list ref
        self.assertEqual(sys.getrefcount(my_i), 4)

        model.clear()
        # ref(my_i)
        self.assertEqual(sys.getrefcount(my_i), 3)


if __name__ == '__main__':
    unittest.main()

