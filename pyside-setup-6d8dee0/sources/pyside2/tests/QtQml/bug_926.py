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

import sys
import unittest

from helper import adjust_filename

from PySide2.QtCore import QUrl, QTimer, QObject, Signal, Property
from PySide2.QtGui import QGuiApplication
from PySide2.QtQml import qmlRegisterType
from PySide2.QtQuick import QQuickView

class MyClass (QObject):

    def __init__(self):
        super(MyClass,self).__init__()
        self.__url = QUrl()

    def getUrl(self):
        return self.__url

    def setUrl(self,value):
        newUrl = QUrl(value)
        if (newUrl != self.__url):
            self.__url = newUrl
            self.urlChanged.emit()

    urlChanged = Signal()
    urla = Property(QUrl, getUrl, setUrl, notify = urlChanged)

class TestBug926 (unittest.TestCase):
    def testIt(self):
        app = QGuiApplication([])
        qmlRegisterType(MyClass,'Example',1,0,'MyClass')
        view = QQuickView()
        view.setSource(QUrl.fromLocalFile(adjust_filename('bug_926.qml', __file__)))
        self.assertEqual(len(view.errors()), 0)
        view.show()
        QTimer.singleShot(0, app.quit)
        app.exec_()

if __name__ == '__main__':
    unittest.main()
