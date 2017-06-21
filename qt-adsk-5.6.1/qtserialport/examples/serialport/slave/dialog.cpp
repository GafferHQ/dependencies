/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include "dialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QGridLayout>

#include <QtSerialPort/QSerialPortInfo>

QT_USE_NAMESPACE

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
    , transactionCount(0)
    , serialPortLabel(new QLabel(tr("Serial port:")))
    , serialPortComboBox(new QComboBox())
    , waitRequestLabel(new QLabel(tr("Wait request, msec:")))
    , waitRequestSpinBox(new QSpinBox())
    , responseLabel(new QLabel(tr("Response:")))
    , responseLineEdit(new QLineEdit(tr("Hello, I'm Slave.")))
    , trafficLabel(new QLabel(tr("No traffic.")))
    , statusLabel(new QLabel(tr("Status: Not running.")))
    , runButton(new QPushButton(tr("Start")))
{
    waitRequestSpinBox->setRange(0, 10000);
    waitRequestSpinBox->setValue(20);

    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
        serialPortComboBox->addItem(info.portName());

    QGridLayout *mainLayout = new QGridLayout;
    mainLayout->addWidget(serialPortLabel, 0, 0);
    mainLayout->addWidget(serialPortComboBox, 0, 1);
    mainLayout->addWidget(waitRequestLabel, 1, 0);
    mainLayout->addWidget(waitRequestSpinBox, 1, 1);
    mainLayout->addWidget(runButton, 0, 2, 2, 1);
    mainLayout->addWidget(responseLabel, 2, 0);
    mainLayout->addWidget(responseLineEdit, 2, 1, 1, 3);
    mainLayout->addWidget(trafficLabel, 3, 0, 1, 4);
    mainLayout->addWidget(statusLabel, 4, 0, 1, 5);
    setLayout(mainLayout);

    setWindowTitle(tr("Slave"));
    serialPortComboBox->setFocus();

    timer.setSingleShot(true);

    connect(runButton, &QPushButton::clicked, this, &Dialog::startSlave);
    connect(&serial, &QSerialPort::readyRead, this, &Dialog::readRequest);
    connect(&timer, &QTimer::timeout, this, &Dialog::processTimeout);

    connect(serialPortComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &Dialog::activateRunButton);
    connect(waitRequestSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Dialog::activateRunButton);
    connect(responseLineEdit, &QLineEdit::textChanged, this, &Dialog::activateRunButton);
}

void Dialog::startSlave()
{
    if (serial.portName() != serialPortComboBox->currentText()) {
        serial.close();
        serial.setPortName(serialPortComboBox->currentText());

        if (!serial.open(QIODevice::ReadWrite)) {
            processError(tr("Can't open %1, error code %2")
                         .arg(serial.portName()).arg(serial.error()));
            return;
        }
    }

    runButton->setEnabled(false);
    statusLabel->setText(tr("Status: Running, connected to port %1.")
                         .arg(serialPortComboBox->currentText()));
}

void Dialog::readRequest()
{
    if (!timer.isActive())
        timer.start(waitRequestSpinBox->value());
    request.append(serial.readAll());
}

void Dialog::processTimeout()
{
    serial.write(responseLineEdit->text().toLocal8Bit());

    trafficLabel->setText(tr("Traffic, transaction #%1:"
                             "\n\r-request: %2"
                             "\n\r-response: %3")
                          .arg(++transactionCount).arg(QString(request)).arg(responseLineEdit->text()));
    request.clear();
}

void Dialog::activateRunButton()
{
    runButton->setEnabled(true);
}

void Dialog::processError(const QString &s)
{
    activateRunButton();
    statusLabel->setText(tr("Status: Not running, %1.").arg(s));
    trafficLabel->setText(tr("No traffic."));
}
