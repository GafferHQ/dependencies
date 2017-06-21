/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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


#include "qremovaliterator_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

RemovalIterator::RemovalIterator(const Item::Iterator::Ptr &target,
                                 const xsInteger pos) : m_target(target),
                                                        m_removalPos(pos),
                                                        m_position(0)
{
    Q_ASSERT(target);
    Q_ASSERT(pos >= 1);
}

Item RemovalIterator::next()
{
    if(m_position == -1)
        return Item();

    m_current = m_target->next();

    if(!m_current)
    {
        m_position = -1;
        m_current.reset();
        return Item();
    }

    ++m_position;

    if(m_position == m_removalPos)
    {
        next(); /* Recurse, return the next item. */
        --m_position; /* Don't count the one we removed. */
        return m_current;
    }

    return m_current;
}

xsInteger RemovalIterator::count()
{
    const xsInteger itc = m_target->count();

    if(itc < m_removalPos)
        return itc;
    else
        return itc - 1;
}

Item RemovalIterator::current() const
{
    return m_current;
}

xsInteger RemovalIterator::position() const
{
    return m_position;
}

Item::Iterator::Ptr RemovalIterator::copy() const
{
    return Item::Iterator::Ptr(new RemovalIterator(m_target->copy(), m_removalPos));
}

QT_END_NAMESPACE
