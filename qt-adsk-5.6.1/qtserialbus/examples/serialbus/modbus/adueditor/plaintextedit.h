/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtSerialBus module.
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

#ifndef PLAINTEXTEDIT_H
#define PLAINTEXTEDIT_H

#include <QMenu>
#include <QPlainTextEdit>

class PlainTextEdit : public QPlainTextEdit
{
    Q_OBJECT
    Q_DISABLE_COPY(PlainTextEdit)

public:
    explicit PlainTextEdit(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
    {}

    void keyPressEvent(QKeyEvent *e)
    {
        switch (e->key()) {
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            setTextInteractionFlags(textInteractionFlags() | Qt::TextEditable);
            QPlainTextEdit::keyPressEvent(e);
            setTextInteractionFlags(textInteractionFlags() &~ Qt::TextEditable);
            break;
        default:
            QPlainTextEdit::keyPressEvent(e);
        }
    }

    void contextMenuEvent(QContextMenuEvent *event)
    {
        QMenu menu(this);
        menu.addAction(QStringLiteral("Clear"), this, &QPlainTextEdit::clear);
        menu.addAction(QStringLiteral("Copy"), this, &QPlainTextEdit::copy, QKeySequence::Copy);
        menu.addSeparator();
        menu.addAction(QStringLiteral("Select All"), this, &QPlainTextEdit::selectAll,
            QKeySequence::SelectAll);
        menu.exec(event->globalPos());
    }
};

#endif // PLAINTEXTEDIT_H
