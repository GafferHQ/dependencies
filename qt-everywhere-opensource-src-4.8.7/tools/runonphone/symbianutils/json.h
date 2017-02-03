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

#ifndef SYMBIANUTILS_JSON_H
#define SYMBIANUTILS_JSON_H

#include "symbianutils_global.h"

#include <QtCore/QByteArray>
#include <QtCore/QStringList>
#include <QtCore/QVector>

namespace Coda {

class SYMBIANUTILS_EXPORT JsonValue
{
public:
    JsonValue() : m_type(Invalid) {}
    explicit JsonValue(const QByteArray &str) { fromString(str); }

    QByteArray m_name;
    QByteArray m_data;
    QList<JsonValue> m_children;

    enum Type {
        Invalid,
        String,
        Number,
        Boolean,
        Object,
        NullObject,
        Array
    };

    Type m_type;

    inline Type type() const { return m_type; }
    inline QByteArray name() const { return m_name; }
    inline bool hasName(const char *name) const { return m_name == name; }

    inline bool isValid() const { return m_type != Invalid; }
    inline bool isNumber() const { return m_type == Number; }
    inline bool isString() const { return m_type == String; }
    inline bool isObject() const { return m_type == Object; }
    inline bool isArray() const { return m_type == Array; }


    inline QByteArray data() const { return m_data; }
    inline const QList<JsonValue> &children() const { return m_children; }
    inline int childCount() const { return m_children.size(); }

    const JsonValue &childAt(int index) const { return m_children[index]; }
    JsonValue &childAt(int index) { return m_children[index]; }
    JsonValue findChild(const char *name) const;

    QByteArray toString(bool multiline = false, int indent = 0) const;
    void fromString(const QByteArray &str);
    void setStreamOutput(const QByteArray &name, const QByteArray &content);

    QVariant toVariant() const;

private:
    static QByteArray parseCString(const char *&from, const char *to);
    static QByteArray parseNumber(const char *&from, const char *to);
    static QByteArray escapeCString(const QByteArray &ba);
    static QString escapeCString(const QString &ba);
    void parsePair(const char *&from, const char *to);
    void parseValue(const char *&from, const char *to);
    void parseObject(const char *&from, const char *to);
    void parseArray(const char *&from, const char *to);

    void dumpChildren(QByteArray *str, bool multiline, int indent) const;
};

/* Thin wrapper around QByteArray for formatting JSON input. Use as in:
 * JsonInputStream(byteArray) << '{' <<  "bla" << ':' << "blup" << '}';
 * Note that strings get double quotes and JSON-escaping, characters should be
 * used for the array/hash delimiters.
 * */
class SYMBIANUTILS_EXPORT JsonInputStream {
public:
    explicit JsonInputStream(QByteArray &a) : m_target(a) {}

    JsonInputStream &operator<<(char c) {  m_target.append(c); return *this; }
    JsonInputStream &operator<<(const char *c)  { appendCString(c); return *this; }
    JsonInputStream &operator<<(const QByteArray &a)  { appendCString(a.constData()); return *this; }
    JsonInputStream &operator<<(const QString &c) { appendString(c); return *this; }

    // Format as array
    JsonInputStream &operator<<(const QStringList &c);

    // Format as array
    JsonInputStream &operator<<(const QVector<QByteArray> &ba);

    JsonInputStream &operator<<(bool b);

    JsonInputStream &operator<<(int i)
        { m_target.append(QByteArray::number(i)); return *this; }
    JsonInputStream &operator<<(unsigned i)
        { m_target.append(QByteArray::number(i)); return *this; }
    JsonInputStream &operator<<(quint64 i)
        { m_target.append(QByteArray::number(i)); return *this; }

private:
    void appendString(const QString &);
    void appendCString(const char *c);

    QByteArray &m_target;
};

} // namespace Coda

#endif // SYMBIANUTILS_JSON_H
