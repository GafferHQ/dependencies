/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include <QStack>
#include <QVector>
#include <QPainter>
#include <QTextLayout>
#include <QDebug>
#include <qmath.h>
#include "private/qdeclarativestyledtext_p.h"

/*
    QDeclarativeStyledText supports few tags:

    <b></b> - bold
    <i></i> - italic
    <br> - new line
    <font color="color_name" size="1-7"></font>

    The opening and closing tags must be correctly nested.
*/

QT_BEGIN_NAMESPACE

class QDeclarativeStyledTextPrivate
{
public:
    QDeclarativeStyledTextPrivate(const QString &t, QTextLayout &l) : text(t), layout(l), baseFont(layout.font()) {}

    void parse();
    bool parseTag(const QChar *&ch, const QString &textIn, QString &textOut, QTextCharFormat &format);
    bool parseCloseTag(const QChar *&ch, const QString &textIn);
    void parseEntity(const QChar *&ch, const QString &textIn, QString &textOut);
    bool parseFontAttributes(const QChar *&ch, const QString &textIn, QTextCharFormat &format);
    QPair<QStringRef,QStringRef> parseAttribute(const QChar *&ch, const QString &textIn);
    QStringRef parseValue(const QChar *&ch, const QString &textIn);

    inline void skipSpace(const QChar *&ch) {
        while (ch->isSpace() && !ch->isNull())
            ++ch;
    }

    QString text;
    QTextLayout &layout;
    QFont baseFont;

    static const QChar lessThan;
    static const QChar greaterThan;
    static const QChar equals;
    static const QChar singleQuote;
    static const QChar doubleQuote;
    static const QChar slash;
    static const QChar ampersand;
};

const QChar QDeclarativeStyledTextPrivate::lessThan(QLatin1Char('<'));
const QChar QDeclarativeStyledTextPrivate::greaterThan(QLatin1Char('>'));
const QChar QDeclarativeStyledTextPrivate::equals(QLatin1Char('='));
const QChar QDeclarativeStyledTextPrivate::singleQuote(QLatin1Char('\''));
const QChar QDeclarativeStyledTextPrivate::doubleQuote(QLatin1Char('\"'));
const QChar QDeclarativeStyledTextPrivate::slash(QLatin1Char('/'));
const QChar QDeclarativeStyledTextPrivate::ampersand(QLatin1Char('&'));

QDeclarativeStyledText::QDeclarativeStyledText(const QString &string, QTextLayout &layout)
: d(new QDeclarativeStyledTextPrivate(string, layout))
{
}

QDeclarativeStyledText::~QDeclarativeStyledText()
{
    delete d;
}

void QDeclarativeStyledText::parse(const QString &string, QTextLayout &layout)
{
    if (string.isEmpty())
        return;
    QDeclarativeStyledText styledText(string, layout);
    styledText.d->parse();
}

void QDeclarativeStyledTextPrivate::parse()
{
    QList<QTextLayout::FormatRange> ranges;
    QStack<QTextCharFormat> formatStack;

    QString drawText;
    drawText.reserve(text.count());

    int textStart = 0;
    int textLength = 0;
    int rangeStart = 0;
    const QChar *ch = text.constData();
    while (!ch->isNull()) {
        if (*ch == lessThan) {
            if (textLength)
                drawText.append(QStringRef(&text, textStart, textLength));
            if (rangeStart != drawText.length() && formatStack.count()) {
                QTextLayout::FormatRange formatRange;
                formatRange.format = formatStack.top();
                formatRange.start = rangeStart;
                formatRange.length = drawText.length() - rangeStart;
                ranges.append(formatRange);
            }
            rangeStart = drawText.length();
            ++ch;
            if (*ch == slash) {
                ++ch;
                if (parseCloseTag(ch, text)) {
                    if (formatStack.count())
                        formatStack.pop();
                }
            } else {
                QTextCharFormat format;
                if (formatStack.count())
                    format = formatStack.top();
                if (parseTag(ch, text, drawText, format))
                    formatStack.push(format);
            }
            textStart = ch - text.constData() + 1;
            textLength = 0;
        } else if (*ch == ampersand) {
            ++ch;
            drawText.append(QStringRef(&text, textStart, textLength));
            parseEntity(ch, text, drawText);
            textStart = ch - text.constData() + 1;
            textLength = 0;
        } else {
            ++textLength;
        }
        if (!ch->isNull())
            ++ch;
    }
    if (textLength)
        drawText.append(QStringRef(&text, textStart, textLength));
    if (rangeStart != drawText.length() && formatStack.count()) {
        QTextLayout::FormatRange formatRange;
        formatRange.format = formatStack.top();
        formatRange.start = rangeStart;
        formatRange.length = drawText.length() - rangeStart;
        ranges.append(formatRange);
    }

    layout.setText(drawText);
    layout.setAdditionalFormats(ranges);
}

bool QDeclarativeStyledTextPrivate::parseTag(const QChar *&ch, const QString &textIn, QString &textOut, QTextCharFormat &format)
{
    skipSpace(ch);

    int tagStart = ch - textIn.constData();
    int tagLength = 0;
    while (!ch->isNull()) {
        if (*ch == greaterThan) {
            QStringRef tag(&textIn, tagStart, tagLength);
            const QChar char0 = tag.at(0);
            if (char0 == QLatin1Char('b')) {
                if (tagLength == 1)
                    format.setFontWeight(QFont::Bold);
                else if (tagLength == 2 && tag.at(1) == QLatin1Char('r')) {
                    textOut.append(QChar(QChar::LineSeparator));
                    return false;
                }
            } else if (char0 == QLatin1Char('i')) {
                if (tagLength == 1)
                    format.setFontItalic(true);
            }
            return true;
        } else if (ch->isSpace()) {
            // may have params.
            QStringRef tag(&textIn, tagStart, tagLength);
            if (tag == QLatin1String("font"))
                return parseFontAttributes(ch, textIn, format);
            if (*ch == greaterThan || ch->isNull())
                continue;
        } else if (*ch != slash){
            tagLength++;
        }
        ++ch;
    }

    return false;
}

bool QDeclarativeStyledTextPrivate::parseCloseTag(const QChar *&ch, const QString &textIn)
{
    skipSpace(ch);

    int tagStart = ch - textIn.constData();
    int tagLength = 0;
    while (!ch->isNull()) {
        if (*ch == greaterThan) {
            QStringRef tag(&textIn, tagStart, tagLength);
            const QChar char0 = tag.at(0);
            if (char0 == QLatin1Char('b')) {
                if (tagLength == 1)
                    return true;
                else if (tag.at(1) == QLatin1Char('r') && tagLength == 2)
                    return true;
            } else if (char0 == QLatin1Char('i')) {
                if (tagLength == 1)
                    return true;
            } else if (tag == QLatin1String("font")) {
                return true;
            }
            return false;
        } else if (!ch->isSpace()){
            tagLength++;
        }
        ++ch;
    }

    return false;
}

void QDeclarativeStyledTextPrivate::parseEntity(const QChar *&ch, const QString &textIn, QString &textOut)
{
    int entityStart = ch - textIn.constData();
    int entityLength = 0;
    while (!ch->isNull()) {
        if (*ch == QLatin1Char(';')) {
            QStringRef entity(&textIn, entityStart, entityLength);
            if (entity == QLatin1String("gt"))
                textOut += QChar(62);
            else if (entity == QLatin1String("lt"))
                textOut += QChar(60);
            else if (entity == QLatin1String("amp"))
                textOut += QChar(38);
            return;
        }
        ++entityLength;
        ++ch;
    }
}

bool QDeclarativeStyledTextPrivate::parseFontAttributes(const QChar *&ch, const QString &textIn, QTextCharFormat &format)
{
    bool valid = false;
    QPair<QStringRef,QStringRef> attr;
    do {
        attr = parseAttribute(ch, textIn);
        if (attr.first == QLatin1String("color")) {
            valid = true;
            format.setForeground(QColor(attr.second.toString()));
        } else if (attr.first == QLatin1String("size")) {
            valid = true;
            int size = attr.second.toString().toInt();
            if (attr.second.at(0) == QLatin1Char('-') || attr.second.at(0) == QLatin1Char('+'))
                size += 3;
            if (size >= 1 && size <= 7) {
                static const qreal scaling[] = { 0.7, 0.8, 1.0, 1.2, 1.5, 2.0, 2.4 };
                format.setFontPointSize(baseFont.pointSize() * scaling[size-1]);
            }
        }
    } while (!ch->isNull() && !attr.first.isEmpty());

    return valid;
}

QPair<QStringRef,QStringRef> QDeclarativeStyledTextPrivate::parseAttribute(const QChar *&ch, const QString &textIn)
{
    skipSpace(ch);

    int attrStart = ch - textIn.constData();
    int attrLength = 0;
    while (!ch->isNull()) {
        if (*ch == greaterThan) {
            break;
        } else if (*ch == equals) {
            ++ch;
            if (*ch != singleQuote && *ch != doubleQuote) {
                while (*ch != greaterThan && !ch->isNull())
                    ++ch;
                break;
            }
            ++ch;
            if (!attrLength)
                break;
            QStringRef attr(&textIn, attrStart, attrLength);
            QStringRef val = parseValue(ch, textIn);
            if (!val.isEmpty())
                return QPair<QStringRef,QStringRef>(attr,val);
            break;
        } else {
            ++attrLength;
        }
        ++ch;
    }

    return QPair<QStringRef,QStringRef>();
}

QStringRef QDeclarativeStyledTextPrivate::parseValue(const QChar *&ch, const QString &textIn)
{
    int valStart = ch - textIn.constData();
    int valLength = 0;
    while (*ch != singleQuote && *ch != doubleQuote && !ch->isNull()) {
        ++valLength;
        ++ch;
    }
    if (ch->isNull())
        return QStringRef();
    ++ch; // skip quote

    return QStringRef(&textIn, valStart, valLength);
}

QT_END_NAMESPACE
