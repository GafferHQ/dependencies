#############################################################################
##
## Copyright (C) 2017 The Qt Company Ltd.
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

from __future__ import print_function

import unittest

from PySide2 import QtWidgets
from PySide2 import QtWebEngineWidgets

class MainTest(unittest.TestCase):

    def test_WebEngineView_findText_exists(self):
        qApp = (QtWidgets.QApplication.instance() or
                QtWidgets.QApplication([]))
        view = QtWebEngineWidgets.QWebEngineView()
        with self.assertRaises(TypeError):
            view.findText(123)
        view.findText("nothing")
        # we are testing only the existence of the function,
        # after it suddenly showed up.

if __name__ == '__main__':
    unittest.main()
