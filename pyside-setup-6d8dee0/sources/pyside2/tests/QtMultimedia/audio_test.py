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
from PySide2.QtMultimedia import *

class testAudioDevices(unittest.TestCase):

    def testListDevices(self):
        valid = False
        devices = QAudioDeviceInfo.availableDevices(QAudio.AudioOutput)
        if not len(devices):
            return

        valid = True
        for devInfo in devices:
            if devInfo.deviceName() == 'null':
                # skip the test if the only device found is a invalid device
                if len(devices) == 1:
                    return
                else:
                    continue
            fmt = QAudioFormat()
            for codec in devInfo.supportedCodecs():
                fmt.setCodec(codec)
                for frequency in devInfo.supportedSampleRates():
                    fmt.setSampleRate(frequency)
                    for channels in devInfo.supportedChannelCounts():
                        fmt.setChannelCount(channels)
                        for sampleType in devInfo.supportedSampleTypes():
                            fmt.setSampleType(sampleType)
                            for sampleSize in devInfo.supportedSampleSizes():
                                fmt.setSampleSize(sampleSize)
                                for endian in devInfo.supportedByteOrders():
                                    fmt.setByteOrder(endian)
                                    if devInfo.isFormatSupported(fmt):
                                        return
        self.assertTrue(False)


if __name__ == '__main__':
    unittest.main()
