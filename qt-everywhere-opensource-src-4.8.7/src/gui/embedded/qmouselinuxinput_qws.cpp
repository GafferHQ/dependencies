/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmouselinuxinput_qws.h"

#include <QScreen>
#include <QSocketNotifier>
#include <QStringList>

#include <qplatformdefs.h>
#include <private/qcore_unix_p.h> // overrides QT_OPEN

#include <errno.h>

#include <linux/input.h>

QT_BEGIN_NAMESPACE


class QWSLinuxInputMousePrivate : public QObject
{
    Q_OBJECT
public:
    QWSLinuxInputMousePrivate(QWSLinuxInputMouseHandler *, const QString &);
    ~QWSLinuxInputMousePrivate();

    void enable(bool on);

private Q_SLOTS:
    void readMouseData();

private:
    QWSLinuxInputMouseHandler *m_handler;
    QSocketNotifier *          m_notify;
    int                        m_fd;
    int                        m_x, m_y;
    int                        m_buttons;
};

QWSLinuxInputMouseHandler::QWSLinuxInputMouseHandler(const QString &device)
    : QWSCalibratedMouseHandler(device)
{
    d = new QWSLinuxInputMousePrivate(this, device);
}

QWSLinuxInputMouseHandler::~QWSLinuxInputMouseHandler()
{
    delete d;
}

void QWSLinuxInputMouseHandler::suspend()
{
    d->enable(false);
}

void QWSLinuxInputMouseHandler::resume()
{
    d->enable(true);
}

QWSLinuxInputMousePrivate::QWSLinuxInputMousePrivate(QWSLinuxInputMouseHandler *h, const QString &device)
    : m_handler(h), m_notify(0), m_x(0), m_y(0), m_buttons(0)
{
    setObjectName(QLatin1String("LinuxInputSubsystem Mouse Handler"));

    QString dev = QLatin1String("/dev/input/event0");
    int grab = 0;

    QStringList args = device.split(QLatin1Char(':'));
    foreach (const QString &arg, args) {
        if (arg.startsWith(QLatin1String("grab=")))
            grab = arg.mid(5).toInt();
        else if (arg.startsWith(QLatin1String("/dev/")))
            dev = arg;
    }

    m_fd = QT_OPEN(dev.toLocal8Bit().constData(), O_RDONLY | O_NDELAY, 0);
    if (m_fd >= 0) {
        ::ioctl(m_fd, EVIOCGRAB, grab);
        m_notify = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
        connect(m_notify, SIGNAL(activated(int)), this, SLOT(readMouseData()));
    } else {
        qWarning("Cannot open mouse input device '%s': %s", qPrintable(dev), strerror(errno));
        return;
    }
}

QWSLinuxInputMousePrivate::~QWSLinuxInputMousePrivate()
{
    if (m_fd >= 0)
        QT_CLOSE(m_fd);
}

void QWSLinuxInputMousePrivate::enable(bool on)
{
    if (m_notify)
        m_notify->setEnabled(on);
}

void QWSLinuxInputMousePrivate::readMouseData()
{
    if (!qt_screen)
        return;

    struct ::input_event buffer[32];
    int n = 0;

    forever {
        int bytesRead = QT_READ(m_fd, reinterpret_cast<char *>(buffer) + n, sizeof(buffer) - n);
        if (bytesRead == 0) {
            qWarning("Got EOF from the input device.");
            return;
        }
        if (bytesRead == -1) {
            if (errno != EAGAIN)
                qWarning("Could not read from input device: %s", strerror(errno));
            break;
        }

        n += bytesRead;
        if (n % sizeof(buffer[0]) == 0)
            break;
    }
    n /= sizeof(buffer[0]);

    for (int i = 0; i < n; ++i) {
        struct ::input_event *data = &buffer[i];

        bool unknown = false;
        if (data->type == EV_ABS) {
            if (data->code == ABS_X) {
                m_x = data->value;
            } else if (data->code == ABS_Y) {
                m_y = data->value;
            } else {
                unknown = true;
            }
        } else if (data->type == EV_REL) {
            if (data->code == REL_X) {
                m_x += data->value;
            } else if (data->code == REL_Y) {
                m_y += data->value;
            } else {
                unknown = true;
            }
        } else if (data->type == EV_KEY && data->code == BTN_TOUCH) {
            m_buttons = data->value ? Qt::LeftButton : 0;
        } else if (data->type == EV_KEY) {
            int button = 0;
            switch (data->code) {
            case BTN_LEFT: button = Qt::LeftButton; break;
            case BTN_MIDDLE: button = Qt::MidButton; break;
            case BTN_RIGHT: button = Qt::RightButton; break;
            }
            if (data->value)
                m_buttons |= button;
            else
                m_buttons &= ~button;
        } else if (data->type == EV_SYN && data->code == SYN_REPORT) {
            QPoint pos(m_x, m_y);
            pos = m_handler->transform(pos);
            m_handler->limitToScreen(pos);
            m_handler->mouseChanged(pos, m_buttons);
        } else if (data->type == EV_MSC && data->code == MSC_SCAN) {
            // kernel encountered an unmapped key - just ignore it
            continue;
        } else {
            unknown = true;
        }
        if (unknown) {
            qWarning("unknown mouse event type=%x, code=%x, value=%x", data->type, data->code, data->value);
        }
    }
}

QT_END_NAMESPACE

#include "qmouselinuxinput_qws.moc"
