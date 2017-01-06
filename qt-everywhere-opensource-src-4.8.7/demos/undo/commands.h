/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#ifndef COMMANDS_H
#define COMMANDS_H

#include <QUndoCommand>
#include "document.h"

class AddShapeCommand : public QUndoCommand
{
public:
    AddShapeCommand(Document *doc, const Shape &shape, QUndoCommand *parent = 0);
    void undo();
    void redo();

private:
    Document *m_doc;
    Shape m_shape;
    QString m_shapeName;
};

class RemoveShapeCommand : public QUndoCommand
{
public:
    RemoveShapeCommand(Document *doc, const QString &shapeName, QUndoCommand *parent = 0);
    void undo();
    void redo();

private:
    Document *m_doc;
    Shape m_shape;
    QString m_shapeName;
};

class SetShapeColorCommand : public QUndoCommand
{
public:
    SetShapeColorCommand(Document *doc, const QString &shapeName, const QColor &color,
                            QUndoCommand *parent = 0);

    void undo();
    void redo();

    bool mergeWith(const QUndoCommand *command);
    int id() const;

private:
    Document *m_doc;
    QString m_shapeName;
    QColor m_oldColor;
    QColor m_newColor;
};

class SetShapeRectCommand : public QUndoCommand
{
public:
    SetShapeRectCommand(Document *doc, const QString &shapeName, const QRect &rect,
                            QUndoCommand *parent = 0);

    void undo();
    void redo();

    bool mergeWith(const QUndoCommand *command);
    int id() const;

private:
    Document *m_doc;
    QString m_shapeName;
    QRect m_oldRect;
    QRect m_newRect;
};

#endif // COMMANDS_H
