/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "customwidgetsinfo.h"
#include "driver.h"
#include "ui4.h"
#include "utils.h"

QT_BEGIN_NAMESPACE

CustomWidgetsInfo::CustomWidgetsInfo()
{
}

void CustomWidgetsInfo::acceptUI(DomUI *node)
{
    m_customWidgets.clear();

    if (node->elementCustomWidgets())
        acceptCustomWidgets(node->elementCustomWidgets());
}

void CustomWidgetsInfo::acceptCustomWidgets(DomCustomWidgets *node)
{
    TreeWalker::acceptCustomWidgets(node);
}

void CustomWidgetsInfo::acceptCustomWidget(DomCustomWidget *node)
{
    if (node->elementClass().isEmpty())
        return;

    m_customWidgets.insert(node->elementClass(), node);
}

bool CustomWidgetsInfo::extends(const QString &classNameIn, QLatin1String baseClassName) const
{
    if (classNameIn == baseClassName)
        return true;

    QString className = classNameIn;
    while (const DomCustomWidget *c = customWidget(className)) {
        const QString extends = c->elementExtends();
        if (className == extends) // Faulty legacy custom widget entries exist.
            return false;
        if (extends == baseClassName)
            return true;
        className = extends;
    }
    return false;
}

bool CustomWidgetsInfo::isCustomWidgetContainer(const QString &className) const
{
    if (const DomCustomWidget *dcw = m_customWidgets.value(className, 0))
        if (dcw->hasElementContainer())
            return dcw->elementContainer() != 0;
    return false;
}

QString CustomWidgetsInfo::realClassName(const QString &className) const
{
    if (className == QLatin1String("Line"))
        return QLatin1String("QFrame");

    return className;
}

DomScript *CustomWidgetsInfo::customWidgetScript(const QString &name) const
{
    if (m_customWidgets.empty())
        return 0;

    const NameCustomWidgetMap::const_iterator it = m_customWidgets.constFind(name);
    if (it == m_customWidgets.constEnd())
        return 0;

    return it.value()->elementScript();
}

QString CustomWidgetsInfo::customWidgetAddPageMethod(const QString &name) const
{
    if (DomCustomWidget *dcw = m_customWidgets.value(name, 0))
        return dcw->elementAddPageMethod();
    return QString();
}


QT_END_NAMESPACE
