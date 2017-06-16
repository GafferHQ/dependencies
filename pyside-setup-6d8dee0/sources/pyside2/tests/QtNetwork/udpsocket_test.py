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

'''Test cases for QUdpSocket'''

import unittest

from PySide2.QtCore import QUrl, QObject, SIGNAL, QCoreApplication, QTimer
from PySide2.QtNetwork import QUdpSocket, QHostAddress

class HttpSignalsCase(unittest.TestCase):
    '''Test case for bug #124 - readDatagram signature

    QUdpSocket.readDatagram must return a tuple with the datagram, host and
    port, while receiving only the max payload size.'''

    def setUp(self):
        #Acquire resources
        self.called = False
        self.app = QCoreApplication([])

        self.socket = QUdpSocket()

        self.server = QUdpSocket()
        self.server.bind(QHostAddress(QHostAddress.LocalHost), 45454)

    def tearDown(self):
        #Release resources
        del self.socket
        del self.server
        del self.app

    def sendPackage(self):
        addr = QHostAddress(QHostAddress.LocalHost)
        self.socket.writeDatagram('datagram', addr, 45454)

    def callback(self):
        while self.server.hasPendingDatagrams():
            datagram, host, port = self.server.readDatagram(self.server.pendingDatagramSize())
            self.called = True
            self.app.quit()

    def testDefaultArgs(self):
        #QUdpSocket.readDatagram pythonic return
        # @bug 124
        QObject.connect(self.server, SIGNAL('readyRead()'), self.callback)
        self.sendPackage()
        self.app.exec_()

        self.assertTrue(self.called)

if __name__ == '__main__':
    unittest.main()
