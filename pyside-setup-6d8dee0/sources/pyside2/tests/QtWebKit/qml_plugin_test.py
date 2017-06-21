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

import os
import sys
import unittest

from PySide2.QtCore import QUrl, QTimer
from PySide2.QtWidgets import QApplication, QLabel
from PySide2.QtWebKit import QWebPluginFactory, QWebView, QWebSettings

from helper import UsesQApplication

class PluginFactory(QWebPluginFactory):

    def plugins(self):
        plugins = []

        mime = self.MimeType()
        mime.name = 'DummyFile'
        mime.fileExtensions = ['.pys']

        plugin = self.Plugin()
        plugin.name = 'DummyPlugin'
        plugin.mimeTypes = [mime]

        plugins.append(plugin)

        return plugins

    def create(self, mimeType, url, argumentNames, argumentValues):
        if mimeType != 'application/x-dummy':
            return None

        for name, value in zip(argumentNames, argumentValues):
            if name == 'text':
                text = value
        else:
            text = "Webkit plugins!"

        widget = QLabel(text)
        return widget

class TestPlugin(UsesQApplication):

    def testPlugin(self):
        view = QWebView()
        fac = PluginFactory()
        view.page().setPluginFactory(fac)
        QWebSettings.globalSettings().setAttribute(QWebSettings.PluginsEnabled, True)

        view.load(QUrl(os.path.join(os.path.abspath(os.path.dirname(__file__)), 'qmlplugin', 'index.html')))

        view.resize(840, 600)
        view.show()

        QTimer.singleShot(500, self.app.quit)

        self.app.exec_()


if __name__ == '__main__':
    unittest.main()
