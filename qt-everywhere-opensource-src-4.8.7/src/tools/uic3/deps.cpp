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

#include "ui3reader.h"

#include <QDomElement>
#include <QFile>

QT_BEGIN_NAMESPACE

void Ui3Reader::computeDeps(const QDomElement &e,
        QStringList &globalIncludes,
        QStringList &localIncludes, bool impl)
{
    QDomNodeList nl;

    // additional includes (local or global) and forward declaractions
    nl = e.toElement().elementsByTagName(QLatin1String("include"));
    for (int i = 0; i < (int) nl.length(); i++) {
        QDomElement n2 = nl.item(i).toElement();
        QString s = n2.firstChild().toText().data();

        if (s.right(5) == QLatin1String(".ui.h") && !QFile::exists(s))
            continue;

        if (impl && n2.attribute(QLatin1String("impldecl"), QLatin1String("in implementation")) != QLatin1String("in implementation"))
            continue;

        if (n2.attribute(QLatin1String("location")) != QLatin1String("local"))
            globalIncludes += s;
        else
            localIncludes += s;
    }

    // do the local includes afterwards, since global includes have priority on clashes
    nl = e.toElement().elementsByTagName(QLatin1String("header"));
    for (int i = 0; i < (int) nl.length(); i++) {
        QDomElement n2 = nl.item(i).toElement();
        QString s = n2.firstChild().toText().data();
        if (n2.attribute(QLatin1String("location")) == QLatin1String("local") && !globalIncludes.contains(s)) {
            if (s.right(5) == QLatin1String(".ui.h") && !QFile::exists(s))
                continue;

            if (impl && n2.attribute(QLatin1String("impldecl"), QLatin1String("in implementation")) != QLatin1String("in implementation"))
                continue;

            localIncludes += s;
        }
    }

    // additional custom widget headers
    nl = e.toElement().elementsByTagName(QLatin1String("header"));
    for (int i = 0; i < (int) nl.length(); i++) {
        QDomElement n2 = nl.item(i).toElement();
        QString s = n2.firstChild().toText().data();

        if (n2.attribute(QLatin1String("location")) != QLatin1String("local"))
            globalIncludes += s;
        else
            localIncludes += s;
    }

    { // fix globalIncludes
        globalIncludes = unique(globalIncludes);
        QMutableStringListIterator it(globalIncludes);
        while (it.hasNext()) {
            QString v = it.next();

            if (v.isEmpty()) {
                it.remove();
                continue;
            }

            it.setValue(fixHeaderName(v));
        }
    }

    { // fix the localIncludes
        localIncludes = unique(localIncludes);
        QMutableStringListIterator it(localIncludes);
        while (it.hasNext()) {
            QString v = it.next();

            if (v.isEmpty()) {
                it.remove();
                continue;
            }

            it.setValue(fixHeaderName(v));
        }
    }
}

QT_END_NAMESPACE
