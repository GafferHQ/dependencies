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

#include "parser.h"
#include <QObject>
#include <QStringList>

QT_BEGIN_NAMESPACE

QString Parser::cleanArgs(const QString &func)
{
    QString slot(func);
    int begin = slot.indexOf(QLatin1Char('(')) + 1;
    QString args = slot.mid(begin);
    args = args.left(args.indexOf(QLatin1Char(')')));
    QStringList lst = args.split(QLatin1Char(','));
    QString res = slot.left(begin);
    for (QStringList::Iterator it = lst.begin(); it != lst.end(); ++it) {
        if (it != lst.begin())
            res += QLatin1Char(',');
        QString arg = *it;
        int pos = 0;
        if ((pos = arg.indexOf(QLatin1Char('&'))) != -1) {
            arg = arg.left(pos + 1);
        } else if ((pos = arg.indexOf(QLatin1Char('*'))) != -1) {
            arg = arg.left(pos + 1);
        } else {
            arg = arg.simplified();
            if ((pos = arg.indexOf(QLatin1Char(':'))) != -1)
                arg = arg.left(pos).simplified() + QLatin1Char(':') + arg.mid(pos + 1).simplified();
            QStringList l = arg.split(QLatin1Char(' '));
            if (l.count() == 2) {
                if (l[0] != QLatin1String("const")
                        && l[0] != QLatin1String("unsigned")
                        && l[0] != QLatin1String("var"))
                    arg = l[0];
            } else if (l.count() == 3) {
                arg = l[0] + QLatin1Char(' ') + l[1];
            }
        }
        res += arg;
    }
    res += QLatin1Char(')');
    return res;
}

QT_END_NAMESPACE
