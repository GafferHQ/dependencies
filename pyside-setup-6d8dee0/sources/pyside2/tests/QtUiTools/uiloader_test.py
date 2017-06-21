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
import os
from helper import UsesQApplication

from PySide2.QtWidgets import QWidget
from PySide2.QtUiTools import QUiLoader

def get_file_path():
    for path in file_path:
        if os.path.exists(path):
            return path
    return ""

class QUioaderTeste(UsesQApplication):
    def testLoadFile(self):
        filePath = os.path.join(os.path.dirname(__file__), 'test.ui')
        loader = QUiLoader()
        parent = QWidget()
        w = loader.load(filePath, parent)
        self.assertNotEqual(w, None)

        self.assertEqual(len(parent.children()), 1)

        child = w.findChild(QWidget, "child_object")
        self.assertNotEqual(child, None)
        self.assertEqual(w.findChild(QWidget, "grandson_object"), child.findChild(QWidget, "grandson_object"))

    def testLoadFileUnicodeFilePath(self):
        filePath = str(os.path.join(os.path.dirname(__file__), 'test.ui'))
        loader = QUiLoader()
        parent = QWidget()
        w = loader.load(filePath, parent)
        self.assertNotEqual(w, None)

        self.assertEqual(len(parent.children()), 1)

        child = w.findChild(QWidget, "child_object")
        self.assertNotEqual(child, None)
        self.assertEqual(w.findChild(QWidget, "grandson_object"), child.findChild(QWidget, "grandson_object"))

if __name__ == '__main__':
    unittest.main()

