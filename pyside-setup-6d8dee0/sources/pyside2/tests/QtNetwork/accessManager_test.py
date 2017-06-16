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

'''Test cases for QHttp'''

import unittest

from PySide2.QtCore import *
from PySide2.QtNetwork import *

from helper import UsesQCoreApplication
from httpd import TestServer

class AccessManagerCase(UsesQCoreApplication):

    def setUp(self):
        super(AccessManagerCase, self).setUp()
        self.httpd = TestServer()
        self.httpd.start()
        self.called = False

    def tearDown(self):
        super(AccessManagerCase, self).tearDown()
        if self.httpd:
            self.httpd.shutdown()
            self.httpd = None

    def goAway(self):
        self.httpd.shutdown()
        self.app.quit()
        self.httpd = None

    def slot_replyFinished(self, reply):
        self.assertEqual(type(reply), QNetworkReply)
        self.called = True
        self.goAway()

    def testNetworkRequest(self):
        manager = QNetworkAccessManager()
        manager.finished.connect(self.slot_replyFinished)
        manager.get(QNetworkRequest(QUrl("http://127.0.0.1:%s" % self.httpd.port())))
        self.app.exec_()
        self.assertTrue(self.called)

if __name__ == '__main__':
    unittest.main()
