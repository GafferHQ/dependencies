/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#include <QtWidgets>

#include "previewform.h"

PreviewForm::PreviewForm(QWidget *parent)
    : QDialog(parent)
{
    encodingComboBox = new QComboBox;

    encodingLabel = new QLabel(tr("&Encoding:"));
    encodingLabel->setBuddy(encodingComboBox);

    textEdit = new QTextEdit;
    textEdit->setLineWrapMode(QTextEdit::NoWrap);
    textEdit->setReadOnly(true);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
                                    | QDialogButtonBox::Cancel);

    connect(encodingComboBox, SIGNAL(activated(int)),
            this, SLOT(updateTextEdit()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QGridLayout *mainLayout = new QGridLayout;
    mainLayout->addWidget(encodingLabel, 0, 0);
    mainLayout->addWidget(encodingComboBox, 0, 1);
    mainLayout->addWidget(textEdit, 1, 0, 1, 2);
    mainLayout->addWidget(buttonBox, 2, 0, 1, 2);
    setLayout(mainLayout);

    setWindowTitle(tr("Choose Encoding"));
    resize(400, 300);
}

void PreviewForm::setCodecList(const QList<QTextCodec *> &list)
{
    encodingComboBox->clear();
    foreach (QTextCodec *codec, list)
        encodingComboBox->addItem(codec->name(), codec->mibEnum());
}

void PreviewForm::setEncodedData(const QByteArray &data)
{
    encodedData = data;
    updateTextEdit();
}

void PreviewForm::updateTextEdit()
{
    int mib = encodingComboBox->itemData(
                      encodingComboBox->currentIndex()).toInt();
    QTextCodec *codec = QTextCodec::codecForMib(mib);

    QTextStream in(&encodedData);
    in.setAutoDetectUnicode(false);
    in.setCodec(codec);
    decodedStr = in.readAll();

    textEdit->setPlainText(decodedStr);
}
