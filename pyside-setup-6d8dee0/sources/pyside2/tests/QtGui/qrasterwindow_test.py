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

'''Unit test for QBackingStore, QRasterWindow and QStaticText'''

import unittest

from helper import UsesQApplication
from PySide2.QtCore import QEvent, QPoint, QRect, QSize, QTimer, Qt
from PySide2.QtGui import QColor, QBackingStore, QPaintDevice, QPainter, QWindow, QPaintDeviceWindow, QRasterWindow, QRegion, QStaticText

# QWindow rendering via QBackingStore
class TestBackingStoreWindow(QWindow):
    def __init__(self):
        super(TestBackingStoreWindow, self).__init__()
        self.backingStore = QBackingStore(self)
        self.text = QStaticText("BackingStoreWindow")

    def event(self, event):
        if event.type() == QEvent.Resize:
            self.backingStore.resize(self.size())
            self.render()
        elif event.type() == QEvent.UpdateRequest or event.type() == QEvent.Expose:
            self.backingStore.flush(QRegion(QRect(QPoint(0, 0), self.size())))

        return QWindow.event(self, event)

    def render(self):
        clientRect = QRect(QPoint(0, 0), self.size())
        painter = QPainter(self.backingStore.paintDevice())
        painter.fillRect(clientRect, QColor(Qt.green))
        painter.drawStaticText(QPoint(10, 10), self.text)

# Window using convenience class QRasterWindow
class TestRasterWindow(QRasterWindow):
    def __init__(self):
        super(TestRasterWindow, self).__init__()
        self.text = QStaticText("QRasterWindow")

    def paintEvent(self, event):
        clientRect = QRect(QPoint(0, 0), self.size())
        painter = QPainter(self)
        painter.fillRect(clientRect, QColor(Qt.red))
        painter.drawStaticText(QPoint(10, 10), self.text)

class QRasterWindowTest(UsesQApplication):
    def test(self):
        rasterWindow = TestRasterWindow()
        rasterWindow.setFramePosition(QPoint(100, 100))
        rasterWindow.resize(QSize(400, 400))
        rasterWindow.show()
        backingStoreWindow = TestBackingStoreWindow()
        backingStoreWindow.setFramePosition(QPoint(600, 100))
        backingStoreWindow.resize(QSize(400, 400))
        backingStoreWindow.show()

        QTimer.singleShot(100, self.app.quit)
        self.app.exec_()

if __name__ == '__main__':
    unittest.main()
