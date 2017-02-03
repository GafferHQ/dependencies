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

#include "validator.h"
#include "driver.h"
#include "ui4.h"
#include "uic.h"

QT_BEGIN_NAMESPACE

Validator::Validator(Uic *uic)   :
    m_driver(uic->driver())
{
}

void Validator::acceptUI(DomUI *node)
{
    TreeWalker::acceptUI(node);
}

void Validator::acceptWidget(DomWidget *node)
{
    (void) m_driver->findOrInsertWidget(node);

    TreeWalker::acceptWidget(node);
}

void Validator::acceptLayoutItem(DomLayoutItem *node)
{
    (void) m_driver->findOrInsertLayoutItem(node);

    TreeWalker::acceptLayoutItem(node);
}

void Validator::acceptLayout(DomLayout *node)
{
    (void) m_driver->findOrInsertLayout(node);

    TreeWalker::acceptLayout(node);
}

void Validator::acceptActionGroup(DomActionGroup *node)
{
    (void) m_driver->findOrInsertActionGroup(node);

    TreeWalker::acceptActionGroup(node);
}

void Validator::acceptAction(DomAction *node)
{
    (void) m_driver->findOrInsertAction(node);

    TreeWalker::acceptAction(node);
}

QT_END_NAMESPACE
