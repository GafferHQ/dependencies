/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#include <QtCore/QFile>

#include "qhpwriter.h"
#include "adpreader.h"

QT_BEGIN_NAMESPACE

QhpWriter::QhpWriter(const QString &namespaceName,
                     const QString &virtualFolder)
{
    m_namespaceName = namespaceName;
    m_virtualFolder = virtualFolder;
    setAutoFormatting(true);
}

void QhpWriter::setAdpReader(AdpReader *reader)
{
    m_adpReader = reader;
}

void QhpWriter::setFilterAttributes(const QStringList &attributes)
{
    m_filterAttributes = attributes;
}

void QhpWriter::setCustomFilters(const QList<CustomFilter> filters)
{
    m_customFilters = filters;
}

void QhpWriter::setFiles(const QStringList &files)
{
    m_files = files;
}

void QhpWriter::generateIdentifiers(IdentifierPrefix prefix,
                                    const QString prefixString)
{
    m_prefix = prefix;
    m_prefixString = prefixString;
}

bool QhpWriter::writeFile(const QString &fileName)
{
    QFile out(fileName);
    if (!out.open(QIODevice::WriteOnly))
        return false;

    setDevice(&out);
    writeStartDocument();
    writeStartElement(QLatin1String("QtHelpProject"));
    writeAttribute(QLatin1String("version"), QLatin1String("1.0"));
    writeTextElement(QLatin1String("namespace"), m_namespaceName);
    writeTextElement(QLatin1String("virtualFolder"), m_virtualFolder);
    writeCustomFilters();
    writeFilterSection();
    writeEndDocument();

    out.close();
    return true;
}

void QhpWriter::writeCustomFilters()
{
    if (!m_customFilters.count())
        return;

    foreach (const CustomFilter &f, m_customFilters) {
        writeStartElement(QLatin1String("customFilter"));
        writeAttribute(QLatin1String("name"), f.name);
        foreach (const QString &a, f.filterAttributes)
            writeTextElement(QLatin1String("filterAttribute"), a);
        writeEndElement();
    }
}

void QhpWriter::writeFilterSection()
{
    writeStartElement(QLatin1String("filterSection"));
    foreach (const QString &a, m_filterAttributes)
        writeTextElement(QLatin1String("filterAttribute"), a);

    writeToc();
    writeKeywords();
    writeFiles();
    writeEndElement();
}

void QhpWriter::writeToc()
{
    QList<ContentItem> lst = m_adpReader->contents();
    if (lst.isEmpty())
        return;

    int depth = -1;
    writeStartElement(QLatin1String("toc"));
    foreach (const ContentItem &i, lst) {
        while (depth-- >= i.depth)
            writeEndElement();
        writeStartElement(QLatin1String("section"));
        writeAttribute(QLatin1String("title"), i.title);
        writeAttribute(QLatin1String("ref"), i.reference);
        depth = i.depth;
    }
    while (depth-- >= -1)
        writeEndElement();
}

void QhpWriter::writeKeywords()
{
    QList<KeywordItem> lst = m_adpReader->keywords();
    if (lst.isEmpty())
        return;

    writeStartElement(QLatin1String("keywords"));
    foreach (const KeywordItem &i, lst) {
        writeEmptyElement(QLatin1String("keyword"));
        writeAttribute(QLatin1String("name"), i.keyword);
        writeAttribute(QLatin1String("ref"), i.reference);
        if (m_prefix == FilePrefix) {
            QString str = i.reference.mid(
                i.reference.lastIndexOf(QLatin1Char('/'))+1);
            str = str.left(str.lastIndexOf(QLatin1Char('.')));
            writeAttribute(QLatin1String("id"), str + QLatin1String("::") + i.keyword);
        } else if (m_prefix == GlobalPrefix) {
            writeAttribute(QLatin1String("id"), m_prefixString + i.keyword);
        }
    }
    writeEndElement();
}

void QhpWriter::writeFiles()
{
    if (m_files.isEmpty())
        return;

    writeStartElement(QLatin1String("files"));
    foreach (const QString &f, m_files)
        writeTextElement(QLatin1String("file"), f);
    writeEndElement();
}

QT_END_NAMESPACE
