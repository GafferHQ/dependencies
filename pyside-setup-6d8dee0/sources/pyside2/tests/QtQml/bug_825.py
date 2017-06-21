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

from helper import adjust_filename

from PySide2.QtCore import Qt, QUrl, QTimer
from PySide2.QtGui import QGuiApplication, QPen
from PySide2.QtWidgets import QGraphicsItem
from PySide2.QtQml import qmlRegisterType
from PySide2.QtQuick import QQuickView, QQuickItem, QQuickPaintedItem

paintCalled = False

class MetaA(type):
    pass

class A(object):
    __metaclass__ = MetaA

MetaB = type(QQuickPaintedItem)
B = QQuickPaintedItem

class MetaC(MetaA, MetaB):
    pass

class C(A, B):
    __metaclass__ = MetaC

class Bug825 (C):
    def __init__(self, parent = None):
        QQuickPaintedItem.__init__(self, parent)

    def paint(self, painter):
        global paintCalled
        pen = QPen(Qt.black, 2)
        painter.setPen(pen);
        painter.drawPie(self.boundingRect(), 0, 128);
        paintCalled = True

class TestBug825 (unittest.TestCase):
    def testIt(self):
        global paintCalled
        app = QGuiApplication([])
        qmlRegisterType(Bug825, 'bugs', 1, 0, 'Bug825')
        self.assertRaises(TypeError, qmlRegisterType, A, 'bugs', 1, 0, 'A')

        view = QQuickView()
        view.setSource(QUrl.fromLocalFile(adjust_filename('bug_825.qml', __file__)))
        view.show()
        QTimer.singleShot(250, view.close)
        app.exec_()
        self.assertTrue(paintCalled)

if __name__ == '__main__':
    unittest.main()
