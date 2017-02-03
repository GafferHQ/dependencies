/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#include "lupdate.h"

#include <translator.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QString>

#include <QtXml/QXmlAttributes>
#include <QtXml/QXmlDefaultHandler>
#include <QtXml/QXmlLocator>
#include <QtXml/QXmlParseException>


QT_BEGIN_NAMESPACE

class LU {
    Q_DECLARE_TR_FUNCTIONS(LUpdate)
};

class UiReader : public QXmlDefaultHandler
{
public:
    UiReader(Translator &translator, ConversionData &cd)
      : m_translator(translator), m_cd(cd), m_lineNumber(-1), m_isTrString(false),
        m_needUtf8(translator.codecName() != "UTF-8")
    {}

    bool startElement(const QString &namespaceURI, const QString &localName,
        const QString &qName, const QXmlAttributes &atts);
    bool endElement(const QString &namespaceURI, const QString &localName,
        const QString &qName);
    bool characters(const QString &ch);
    bool fatalError(const QXmlParseException &exception);

    void setDocumentLocator(QXmlLocator *locator) { m_locator = locator; }

private:
    void flush();

    Translator &m_translator;
    ConversionData &m_cd;
    QString m_context;
    QString m_source;
    QString m_comment;
    QString m_extracomment;
    QXmlLocator *m_locator;

    QString m_accum;
    int m_lineNumber;
    bool m_isTrString;
    bool m_needUtf8;
};

bool UiReader::startElement(const QString &namespaceURI,
    const QString &localName, const QString &qName, const QXmlAttributes &atts)
{
    Q_UNUSED(namespaceURI);
    Q_UNUSED(localName);

    if (qName == QLatin1String("item")) { // UI3 menu entries
        flush();
        if (!atts.value(QLatin1String("text")).isEmpty()) {
            m_source = atts.value(QLatin1String("text"));
            m_isTrString = true;
            if (!m_cd.m_noUiLines)
                m_lineNumber = m_locator->lineNumber();
        }
    } else if (qName == QLatin1String("string")) {
        flush();
        if (atts.value(QLatin1String("notr")).isEmpty() ||
            atts.value(QLatin1String("notr")) != QLatin1String("true")) {
            m_isTrString = true;
            m_comment = atts.value(QLatin1String("comment"));
            m_extracomment = atts.value(QLatin1String("extracomment"));
            if (!m_cd.m_noUiLines)
                m_lineNumber = m_locator->lineNumber();
        } else {
            m_isTrString = false;
        }
    }
    m_accum.clear();
    return true;
}

bool UiReader::endElement(const QString &namespaceURI,
    const QString &localName, const QString &qName)
{
    Q_UNUSED(namespaceURI);
    Q_UNUSED(localName);

    m_accum.replace(QLatin1String("\r\n"), QLatin1String("\n"));

    if (qName == QLatin1String("class")) { // UI "header"
        if (m_context.isEmpty())
            m_context = m_accum;
    } else if (qName == QLatin1String("string") && m_isTrString) {
        m_source = m_accum;
    } else if (qName == QLatin1String("comment")) { // FIXME: what's that?
        m_comment = m_accum;
        flush();
    } else if (qName == QLatin1String("function")) { // UI3 embedded code
        fetchtrInlinedCpp(m_accum, m_translator, m_context);
    } else {
        flush();
    }
    return true;
}

bool UiReader::characters(const QString &ch)
{
    m_accum += ch;
    return true;
}

bool UiReader::fatalError(const QXmlParseException &exception)
{
    QString msg = LU::tr("XML error: Parse error at line %1, column %2 (%3).")
        .arg(exception.lineNumber()).arg(exception.columnNumber())
        .arg(exception.message());
    m_cd.appendError(msg);
    return false;
}

void UiReader::flush()
{
    if (!m_context.isEmpty() && !m_source.isEmpty()) {
        TranslatorMessage msg(m_context, m_source,
           m_comment, QString(), m_cd.m_sourceFileName,
           m_lineNumber, QStringList());
        msg.setExtraComment(m_extracomment);
        if (m_needUtf8 && msg.needs8Bit())
            msg.setUtf8(true);
        m_translator.extend(msg);
    }
    m_source.clear();
    m_comment.clear();
    m_extracomment.clear();
}

bool loadUI(Translator &translator, const QString &filename, ConversionData &cd)
{
    cd.m_sourceFileName = filename;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        cd.appendError(LU::tr("Cannot open %1: %2").arg(filename, file.errorString()));
        return false;
    }
    QXmlInputSource in(&file);
    QXmlSimpleReader reader;
    reader.setFeature(QLatin1String("http://xml.org/sax/features/namespaces"), false);
    reader.setFeature(QLatin1String("http://xml.org/sax/features/namespace-prefixes"), true);
    reader.setFeature(QLatin1String(
            "http://trolltech.com/xml/features/report-whitespace-only-CharData"), false);
    UiReader handler(translator, cd);
    reader.setContentHandler(&handler);
    reader.setErrorHandler(&handler);
    bool result = reader.parse(in);
    if (!result)
        cd.appendError(LU::tr("Parse error in UI file"));
    reader.setContentHandler(0);
    reader.setErrorHandler(0);
    return result;
}

QT_END_NAMESPACE
