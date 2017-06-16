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
from helper import UsesQApplication
from PySide2.QtGui import QValidator, QIntValidator
from PySide2.QtWidgets import QLineEdit

'''Bug #871 - http://bugs.pyside.org/show_bug.cgi?id=871'''


class BlankIntValidator(QIntValidator):
    def validate(self,input,pos):
        if input == '':
            return QValidator.Acceptable, input, pos
        else:
            return QIntValidator.validate(self,input,pos)


class Bug871Test(UsesQApplication):
    def testWithoutValidator(self):
        edit = QLineEdit()
        self.assertEqual(edit.text(), '')
        edit.insert('1')
        self.assertEqual(edit.text(), '1')
        edit.insert('a')
        self.assertEqual(edit.text(), '1a')
        edit.insert('2')
        self.assertEqual(edit.text(), '1a2')

    def testWithIntValidator(self):
        edit = QLineEdit()
        edit.setValidator(BlankIntValidator(edit))
        self.assertEqual(edit.text(), '')
        edit.insert('1')
        self.assertEqual(edit.text(), '1')
        edit.insert('a')
        self.assertEqual(edit.text(), '1')
        edit.insert('2')
        self.assertEqual(edit.text(), '12')


if __name__ == "__main__":
   unittest.main()

