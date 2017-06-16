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
scene = QGraphicsScene()
scene.addText("Hello, world!")

view = QGraphicsView(scene)
view.show()
//! [0]


//! [1]
scene = QGraphicsScene()
scene.addRect(QRectF(-10, -10, 20, 20))

view = QGraphicsView(scene)
view.setRenderHints(QPainter.Antialiasing | QPainter.SmoothPixmapTransform)
view.show()
//! [1]


//! [2]
view = QGraphicsView()
view.setBackgroundBrush(QImage(":/images/backgroundtile.png"))
view.setCacheMode(QGraphicsView.CacheBackground)
//! [2]


//! [3]
scene = QGraphicsScene()
scene.addText("GraphicsView rotated clockwise")

view = QGraphicsView(scene)
view.rotate(90) # the text is rendered with a 90 degree clockwise rotation
view.show()
//! [3]


//! [4]
scene = QGraphicsScene()
scene.addItem(...
...

view = QGraphicsView(scene)
view.show()
...

printer = QPrinter(QPrinter.HighResolution)
printer.setPageSize(QPrinter.A4)
painter = QPainter(printer)

# print, fitting the viewport contents into a full page
view.render(painter)

# print the upper half of the viewport into the lower.
# half of the page.
viewport = view.viewport()->rect()
view.render(painter,
            QRectF(0, printer.height() / 2,
                   printer.width(), printer.height() / 2),
            viewport.adjusted(0, 0, 0, -viewport.height() / 2))

//! [4]


//! [5]
def mousePressEvent(self, event):
    print "There are", items(event->pos()).size(), "items at position", mapToScene(event->pos())
//! [5]


//! [6]
def mousePressEvent(self, event):
    if (item = itemAt(event.pos()):
        print "You clicked on item", item
    else:
        print "You didn't click on an item."
//! [6]


//! [7]
scene = QGraphicsScene()
scene.addText("GraphicsView rotated clockwise")

view = QGraphicsView(scene)
view.rotate(90) # the text is rendered with a 90 degree clockwise rotation
view.show()
//! [7]
