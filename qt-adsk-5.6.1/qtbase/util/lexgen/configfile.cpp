/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the utils of the Qt Toolkit.
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
#include "configfile.h"

#include <QFile>

ConfigFile::SectionMap ConfigFile::parse(const QString &fileName)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly))
        return ConfigFile::SectionMap();
    return parse(&f);
}

ConfigFile::SectionMap ConfigFile::parse(QIODevice *dev)
{
    SectionMap sections;
    SectionMap::Iterator currentSection = sections.end();

    ConfigFile::SectionMap result;
    int currentLineNumber = 0;
    while (!dev->atEnd()) {
        QString line = QString::fromUtf8(dev->readLine()).trimmed();
        ++currentLineNumber;

        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        if (line.startsWith(QLatin1Char('['))) {
            if (!line.endsWith(']')) {
                qWarning("Syntax error at line %d: Missing ']' at start of new section.", currentLineNumber);
                return SectionMap();
            }
            line.remove(0, 1);
            line.chop(1);
            const QString sectionName = line;
            currentSection = sections.insert(sectionName, Section());
            continue;
        }

        if (currentSection == sections.end()) {
            qWarning("Syntax error at line %d: Entry found outside of any section.", currentLineNumber);
            return SectionMap();
        }

        Entry e;
        e.lineNumber = currentLineNumber;

        int equalPos = line.indexOf(QLatin1Char('='));
        if (equalPos == -1) {
            e.key = line;
        } else {
            e.key = line;
            e.key.truncate(equalPos);
            e.key = e.key.trimmed();
            e.value = line.mid(equalPos + 1).trimmed();
        }
        currentSection->append(e);
    }
    return sections;
}
