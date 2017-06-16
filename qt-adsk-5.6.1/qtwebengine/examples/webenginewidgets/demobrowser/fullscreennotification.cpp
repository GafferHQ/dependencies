/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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

#include "fullscreennotification.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>

FullScreenNotification::FullScreenNotification(QWidget *parent)
    : QWidget(parent)
    , width(400)
    , height(80)
    , x((parent->geometry().width() - width) / 2)
    , y(80)
{
    setVisible(false);
    setWindowFlags(Qt::ToolTip | Qt::WindowDoesNotAcceptFocus);

    QGridLayout *layout = new QGridLayout(this);

    m_label = new QLabel(tr("You are now in fullscreen mode. Press ESC to quit!"), this);
    layout->addWidget(m_label, 0, 0, 0, 0, Qt::AlignHCenter | Qt::AlignVCenter);

    setGeometry(x, y, width, height);

    setStyleSheet("background-color: white;\
        color: black;");

    m_animation = new QPropertyAnimation(this, "windowOpacity");
    connect(m_animation, SIGNAL(finished()), this, SLOT(fadeOutFinished()));
}

FullScreenNotification::~FullScreenNotification()
{
}

void FullScreenNotification::show()
{
    setWindowOpacity(1.0);
    QTimer::singleShot(300, [&] {
        QWidget *parent = parentWidget();
        x = (parent->geometry().width() - width) / 2;
        QPoint topLeft = parent->mapToGlobal(QPoint(x, y));
        QWidget::move(topLeft.x(), topLeft.y());
        QWidget::show();
        QWidget::raise();
    });
    QTimer::singleShot(5000, this, SLOT(fadeOut()));
}

void FullScreenNotification::hide()
{
    QWidget::hide();
    m_animation->stop();
}

void FullScreenNotification::fadeOut()
{
    m_animation->setDuration(800);
    m_animation->setStartValue(1.0);
    m_animation->setEndValue(0.0);
    m_animation->setEasingCurve(QEasingCurve::OutQuad);
    m_animation->start();
}

void FullScreenNotification::fadeOutFinished()
{
    hide();
    setWindowOpacity(1.0);
}
