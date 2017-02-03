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

#include <QRegExp>

#include "adpreader.h"

static bool versionIsAtLeast320(const QString &version)
{
    return QRegExp("\\d.\\d\\.\\d").exactMatch(version)
            && (version[0] > '3' || (version[0] == '3' && version[2] >= '2'));
}

QT_BEGIN_NAMESPACE

void AdpReader::readData(const QByteArray &contents)
{
    clear();
    m_contents.clear();
    m_keywords.clear();
    m_properties.clear();
    m_files.clear();
    addData(contents);
    while (!atEnd()) {
        readNext();
        if (isStartElement()) {
            if (name().toString().toLower() == QLatin1String("assistantconfig")
                && versionIsAtLeast320(attributes().value(QLatin1String("version")).toString())) {
                readProject();
            } else if (name().toString().toLower() == QLatin1String("dcf")) {
                QString ref = attributes().value(QLatin1String("ref")).toString();
                addFile(ref);
                m_contents.append(ContentItem(attributes().value(QLatin1String("title")).toString(),
                    ref, 0));
                readDCF();
            } else {
                raiseError();
            }
        }
    }
}

QList<ContentItem> AdpReader::contents() const
{
    return m_contents;
}

QList<KeywordItem> AdpReader::keywords() const
{
    return m_keywords;
}

QSet<QString> AdpReader::files() const
{
    return m_files;
}

QMap<QString, QString> AdpReader::properties() const
{
    return m_properties;
}

void AdpReader::readProject()
{
    while (!atEnd()) {
        readNext();
        if (isStartElement()) {
            QString s = name().toString().toLower();
            if (s == QLatin1String("profile")) {
                readProfile();
            } else if (s == QLatin1String("dcf")) {
                QString ref = attributes().value(QLatin1String("ref")).toString();
                addFile(ref);
                m_contents.append(ContentItem(attributes().value(QLatin1String("title")).toString(),
                    ref, 0));
                readDCF();
            } else {
                raiseError();
            }
        }
    }
}

void AdpReader::readProfile()
{
    while (!atEnd()) {
        readNext();
        if (isStartElement()) {
            if (name().toString().toLower() == QLatin1String("property")) {
                QString prop = attributes().value(QLatin1String("name")).toString().toLower();
                m_properties[prop] = readElementText();
            } else {
                raiseError();
            }
        } else if (isEndElement()) {
            break;
        }
    }
}

void AdpReader::readDCF()
{
    int depth = 0;
    while (!atEnd()) {
        readNext();
        QString str = name().toString().toLower();
        if (isStartElement()) {
            if (str == QLatin1String("section")) {
                QString ref = attributes().value(QLatin1String("ref")).toString();
                addFile(ref);
                m_contents.append(ContentItem(attributes().value(QLatin1String("title")).toString(),
                    ref, ++depth));
            } else if (str == QLatin1String("keyword")) {
                QString ref = attributes().value(QLatin1String("ref")).toString();
                addFile(ref);
                m_keywords.append(KeywordItem(readElementText(), ref));
            } else {
                raiseError();
            }
        } else if (isEndElement()) {
            if (str == QLatin1String("section"))
                --depth;
            else if (str == QLatin1String("dcf"))
                break;
        }
    }
}

void AdpReader::addFile(const QString &file)
{
    QString s = file;
    if (s.startsWith(QLatin1String("./")))
        s = s.mid(2);
    int i = s.indexOf(QLatin1Char('#'));
    if (i > -1)
        s = s.left(i);
    if (!m_files.contains(s))
        m_files.insert(s);
}

QT_END_NAMESPACE
