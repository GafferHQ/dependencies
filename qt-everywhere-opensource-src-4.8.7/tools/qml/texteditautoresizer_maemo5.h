/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
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

#include <QtGui/qplaintextedit.h>
#include <QtGui/qtextedit.h>
#include <QtGui/qabstractkineticscroller.h>
#include <QtGui/qscrollarea.h>
#include <QtDebug>

#ifndef TEXTEDITAUTORESIZER_H
#define TEXTEDITAUTORESIZER_H

class TextEditAutoResizer : public QObject
{
    Q_OBJECT
public:
    TextEditAutoResizer(QWidget *parent)
        : QObject(parent), plainTextEdit(qobject_cast<QPlainTextEdit *>(parent)),
          textEdit(qobject_cast<QTextEdit *>(parent)), edit(qobject_cast<QFrame *>(parent))
    {
        // parent must either inherit QPlainTextEdit or  QTextEdit!
        Q_ASSERT(plainTextEdit || textEdit);

        connect(parent, SIGNAL(textChanged()), this, SLOT(textEditChanged()));
        connect(parent, SIGNAL(cursorPositionChanged()), this, SLOT(textEditChanged()));

        textEditChanged();
    }

private Q_SLOTS:
    inline void textEditChanged();

private:
    QPlainTextEdit *plainTextEdit;
    QTextEdit *textEdit;
    QFrame *edit;
};

void TextEditAutoResizer::textEditChanged()
{
    QTextDocument *doc = textEdit ? textEdit->document() : plainTextEdit->document();
    QRect cursor = textEdit ? textEdit->cursorRect() : plainTextEdit->cursorRect();

    QSize s = doc->size().toSize();
    if (plainTextEdit)
        s.setHeight((s.height() + 2) * edit->fontMetrics().lineSpacing());

    const QRect fr = edit->frameRect();
    const QRect cr = edit->contentsRect();

    edit->setMinimumHeight(qMax(70, s.height() + (fr.height() - cr.height() - 1)));

    // make sure the cursor is visible in case we have a QAbstractScrollArea parent
    QPoint pos = edit->pos();
    QWidget *pw = edit->parentWidget();
    while (pw) {
        if (qobject_cast<QScrollArea *>(pw))
            break;
        pw = pw->parentWidget();
    }

    if (pw) {
        QScrollArea *area = static_cast<QScrollArea *>(pw);
        QPoint scrollto = area->widget()->mapFrom(edit, cursor.center());
        QPoint margin(10 + cursor.width(), 2 * cursor.height());

        if (QAbstractKineticScroller *scroller = area->property("kineticScroller").value<QAbstractKineticScroller *>()) {
            scroller->ensureVisible(scrollto, margin.x(), margin.y());
        } else {
            area->ensureVisible(scrollto.x(), scrollto.y(), margin.x(), margin.y());
        }
    }
}

#endif
