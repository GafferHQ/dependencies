/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QThread>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtGui/QRegion>
#include <QtGui/QBackingStore>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QApplication>

#include <qx11info_x11.h>

class HeavyPainter : public QWidget, public QXcbAbstractEventPeeker
{
    Q_OBJECT
public:
    HeavyPainter() : m_stopTasks(false) {}

    void setStopTasks(bool stop) { m_stopTasks = stop; }

protected:
    void paintEvent(QPaintEvent* e)
    {
        Q_UNUSED(e)
        m_buffer = QPixmap(width(), height());
        QPainter p(&m_buffer);

        for (int i = 0; !m_stopTasks; ++i)
        {
            // long operation
            p.fillRect(QRect(0, 0, width(), height()), i & 1 ? Qt::green : Qt::gray);
            p.drawText(QPoint(10,10), "Press any key to exit");
            QPainter p(this);
            QPoint point = mapTo(window(), QPoint(0,0));
            QRegion region(point.x(), point.y(), width(), height());

            QBackingStore *bs = backingStore();
            bs->beginPaint(region);
            p.drawPixmap(0,0, m_buffer);

            bs->endPaint();
            bs->flush(region, windowHandle());

            QThread::msleep(300);
            QX11Info::peekEventQueue(this);
        }

        exit(0);
    }

    bool peekEventQueue(xcb_generic_event_t *event)
    {
        uint responseType = event->response_type & ~0x80;
        if (responseType == XCB_KEY_PRESS) {
            setStopTasks(true);
            return true;
        }

        return false;
    }

private:
    bool m_stopTasks;
    QPixmap m_buffer;
};

int main(int argc, char** argv)
{
  QApplication app(argc, argv);

  QWidget win;
  win.setLayout(new QVBoxLayout);

  QWidget *painter = new HeavyPainter;
  win.layout()->addWidget(painter);

  win.setGeometry(100,100,300,300);
  win.show();

  return app.exec();
}

#include "main.moc"
