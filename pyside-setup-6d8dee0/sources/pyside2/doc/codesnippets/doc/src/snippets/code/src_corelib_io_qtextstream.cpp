/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the documentation of PySide2.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

//! [0]
data = QFile("output.txt")
if data.open(QFile.WriteOnly | QFile.Truncate):
    out = QTextStream(&data)
    out << "Result: " << qSetFieldWidth(10) << left << 3.14 << 2.7
    # writes "Result: 3.14      2.7       "

//! [0]


//! [1]
stream = QTextStream(sys.stdin.fileno())

while(True):
    line = stream.readLine()
    if line.isNull():
        break;
//! [1]


//! [2]
in_ = QTextStream("0x50 0x20")
firstNumber = 0
secondNumber = 0

in_ >> firstNumber             # firstNumber == 80
in_ >> dec >> secondNumber     # secondNumber == 0

ch = None
in_ >> ch                      # ch == 'x'
//! [2]


//! [3]
def main():
    # read numeric arguments (123, 0x20, 4.5...)
    for i in sys.argv():
          number = None
          QTextStream in_(i)
          in_ >> number
          ...
//! [3]


//! [4]
str = QString()
in_ = QTextStream(sys.stdin.fileno())
in_ >> str
//! [4]


//! [5]
s = QString()
out = QTextStream(s)
out.setFieldWidth(10)
out.setFieldAlignment(QTextStream::AlignCenter)
out.setPadChar('-')
out << "Qt" << "rocks!"
//! [5]


//! [6]
----Qt------rocks!--
//! [6]


//! [7]
in_ = QTextStream(file)
ch1 = QChar()
ch2 = QChar()
ch3 = QChar()
in_ >> ch1 >> ch2 >> ch3;
//! [7]


//! [8]
out = QTextStream(sys.stdout.fileno())
out << "Qt rocks!" << endl
//! [8]


//! [9]
stream << '\n' << flush
//! [9]


//! [10]
out = QTextStream(file)
out.setCodec("UTF-8")
//! [10]
