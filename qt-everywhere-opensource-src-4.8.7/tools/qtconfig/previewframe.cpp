/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "previewframe.h"
#include "previewwidget.h"

#include <QBoxLayout>
#include <QPainter>
#include <QMdiSubWindow>

QT_BEGIN_NAMESPACE

PreviewFrame::PreviewFrame(QWidget *parent)
    : QFrame(parent)
{
    setMinimumSize(200, 200);
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setLineWidth(1);

    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setMargin(0);
    previewWidget = new PreviewWidget;
    workspace = new Workspace(this);
    vbox->addWidget(workspace);
    previewWidget->setAutoFillBackground(true);
}

void PreviewFrame::setPreviewPalette(QPalette pal)
{
    previewWidget->setPalette(pal);
}

QString PreviewFrame::previewText() const
{
    return m_previewWindowText;
}

void PreviewFrame::setPreviewVisible(bool visible)
{
    previewWidget->parentWidget()->setVisible(visible);
    if (visible)
        m_previewWindowText = QLatin1String("The moose in the noose\nate the goose who was loose.");
    else
        m_previewWindowText = tr("Desktop settings will only take effect after an application restart.");
    workspace->viewport()->update();
}

Workspace::Workspace(PreviewFrame *parent)
    : QMdiArea(parent)
{
    previewFrame = parent;
    PreviewWidget *previewWidget = previewFrame->widget();
    QMdiSubWindow *frame = addSubWindow(previewWidget, Qt::Window);
    frame->move(10, 10);
    frame->show();
}

void Workspace::paintEvent(QPaintEvent *)
{
    QPainter p(viewport());
    p.fillRect(rect(), palette().color(backgroundRole()).dark());
    p.setPen(QPen(Qt::white));
    p.drawText(0, height() / 2,  width(), height(), Qt::AlignHCenter, previewFrame->previewText());
}

QT_END_NAMESPACE
