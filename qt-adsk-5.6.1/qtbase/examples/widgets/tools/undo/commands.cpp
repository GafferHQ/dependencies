/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#include "commands.h"

static const int setShapeRectCommandId = 1;
static const int setShapeColorCommandId = 2;

/******************************************************************************
** AddShapeCommand
*/

AddShapeCommand::AddShapeCommand(Document *doc, const Shape &shape, QUndoCommand *parent)
    : QUndoCommand(parent)
{
    m_doc = doc;
    m_shape = shape;
}

void AddShapeCommand::undo()
{
    m_doc->deleteShape(m_shapeName);
}

void AddShapeCommand::redo()
{
    // A shape only gets a name when it is inserted into a document
    m_shapeName = m_doc->addShape(m_shape);
    setText(QObject::tr("Add %1").arg(m_shapeName));
}

/******************************************************************************
** RemoveShapeCommand
*/

RemoveShapeCommand::RemoveShapeCommand(Document *doc, const QString &shapeName,
                                        QUndoCommand *parent)
    : QUndoCommand(parent)
{
    setText(QObject::tr("Remove %1").arg(shapeName));
    m_doc = doc;
    m_shape = doc->shape(shapeName);
    m_shapeName = shapeName;
}

void RemoveShapeCommand::undo()
{
    m_shapeName = m_doc->addShape(m_shape);
}

void RemoveShapeCommand::redo()
{
    m_doc->deleteShape(m_shapeName);
}

/******************************************************************************
** SetShapeColorCommand
*/

SetShapeColorCommand::SetShapeColorCommand(Document *doc, const QString &shapeName,
                                            const QColor &color, QUndoCommand *parent)
    : QUndoCommand(parent)
{
    setText(QObject::tr("Set %1's color").arg(shapeName));

    m_doc = doc;
    m_shapeName = shapeName;
    m_oldColor = doc->shape(shapeName).color();
    m_newColor = color;
}

void SetShapeColorCommand::undo()
{
    m_doc->setShapeColor(m_shapeName, m_oldColor);
}

void SetShapeColorCommand::redo()
{
    m_doc->setShapeColor(m_shapeName, m_newColor);
}

bool SetShapeColorCommand::mergeWith(const QUndoCommand *command)
{
    if (command->id() != setShapeColorCommandId)
        return false;

    const SetShapeColorCommand *other = static_cast<const SetShapeColorCommand*>(command);
    if (m_shapeName != other->m_shapeName)
        return false;

    m_newColor = other->m_newColor;
    return true;
}

int SetShapeColorCommand::id() const
{
    return setShapeColorCommandId;
}

/******************************************************************************
** SetShapeRectCommand
*/

SetShapeRectCommand::SetShapeRectCommand(Document *doc, const QString &shapeName,
                                            const QRect &rect, QUndoCommand *parent)
    : QUndoCommand(parent)
{
    setText(QObject::tr("Change %1's geometry").arg(shapeName));

    m_doc = doc;
    m_shapeName = shapeName;
    m_oldRect = doc->shape(shapeName).rect();
    m_newRect = rect;
}

void SetShapeRectCommand::undo()
{
    m_doc->setShapeRect(m_shapeName, m_oldRect);
}

void SetShapeRectCommand::redo()
{
    m_doc->setShapeRect(m_shapeName, m_newRect);
}

bool SetShapeRectCommand::mergeWith(const QUndoCommand *command)
{
    if (command->id() != setShapeRectCommandId)
        return false;

    const SetShapeRectCommand *other = static_cast<const SetShapeRectCommand*>(command);
    if (m_shapeName != other->m_shapeName)
        return false;

    m_newRect = other->m_newRect;
    return true;
}

int SetShapeRectCommand::id() const
{
    return setShapeRectCommandId;
}
