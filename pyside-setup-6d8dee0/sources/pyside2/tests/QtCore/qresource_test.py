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

'''Test cases for QResource usage'''

import unittest
from helper import adjust_filename
from PySide2.QtCore import QFile, QIODevice
import resources_mc

class ResourcesUsage(unittest.TestCase):
    '''Test case for resources usage'''

    def testPhrase(self):
        #Test loading of quote.txt resource
        f = open(adjust_filename('quoteEnUS.txt', __file__), "r")
        orig = f.read()
        f.close()

        f = QFile(':/quote.txt')
        f.open(QIODevice.ReadOnly) #|QIODevice.Text)
        print("Error:", f.errorString())
        copy = f.readAll()
        f.close()
        self.assertEqual(orig, copy)

    def testImage(self):
        #Test loading of sample.png resource
        f = open(adjust_filename('sample.png', __file__), "rb")
        orig = f.read()
        f.close()

        f = QFile(':/sample.png')
        f.open(QIODevice.ReadOnly)
        copy = f.readAll()
        f.close()
        self.assertEqual(len(orig), len(copy))
        self.assertEqual(orig, copy)


if __name__ == '__main__':
    unittest.main()

