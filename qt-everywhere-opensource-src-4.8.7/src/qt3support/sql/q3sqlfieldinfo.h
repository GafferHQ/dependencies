/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3Support module of the Qt Toolkit.
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

#ifndef Q3SQLFIELDINFO_H
#define Q3SQLFIELDINFO_H

#ifndef QT_NO_SQL

#include <QtCore/qglobal.h>
#include <QtSql/qsqlfield.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Qt3Support)

/* Q3SqlFieldInfo Class
   obsoleted, use QSqlField instead
*/

class Q_COMPAT_EXPORT Q3SqlFieldInfo
{
    // class is obsoleted, won't change anyways,
    // so no d pointer
    int req, len, prec, tID;
    uint gen: 1;
    uint trim: 1;
    uint calc: 1;
    QString nm;
    QVariant::Type typ;
    QVariant defValue;

public:
    Q3SqlFieldInfo(const QString& name = QString(),
                   QVariant::Type typ = QVariant::Invalid,
                   int required = -1,
                   int len = -1,
                   int prec = -1,
                   const QVariant& defValue = QVariant(),
                   int sqlType = 0,
                   bool generated = true,
                   bool trim = false,
                   bool calculated = false) :
        req(required), len(len), prec(prec), tID(sqlType),
        gen(generated), trim(trim), calc(calculated),
        nm(name), typ(typ), defValue(defValue) {}

    virtual ~Q3SqlFieldInfo() {}

    Q3SqlFieldInfo(const QSqlField & other)
    {
        nm = other.name();
        typ = other.type();
        switch (other.requiredStatus()) {
        case QSqlField::Unknown: req = -1; break;
        case QSqlField::Required: req = 1; break;
        case QSqlField::Optional: req = 0; break;
        }
        len = other.length();
        prec = other.precision();
        defValue = other.defaultValue();
        tID = other.typeID();
        gen = other.isGenerated();
        calc = false;
        trim = false;
    }

    bool operator==(const Q3SqlFieldInfo& f) const
    {
        return (nm == f.nm &&
                typ == f.typ &&
                req == f.req &&
                len == f.len &&
                prec == f.prec &&
                defValue == f.defValue &&
                tID == f.tID &&
                gen == f.gen &&
                trim == f.trim &&
                calc == f.calc);
    }

    QSqlField toField() const
    { QSqlField f(nm, typ);
      f.setRequiredStatus(QSqlField::RequiredStatus(req));
      f.setLength(len);
      f.setPrecision(prec);
      f.setDefaultValue(defValue);
      f.setSqlType(tID);
      f.setGenerated(gen);
      return f;
    }
    int isRequired() const
    { return req; }
    QVariant::Type type() const
    { return typ; }
    int length() const
    { return len; }
    int precision() const
    { return prec; }
    QVariant defaultValue() const
    { return defValue; }
    QString name() const
    { return nm; }
    int typeID() const
    { return tID; }
    bool isGenerated() const
    { return gen; }
    bool isTrim() const
    { return trim; }
    bool isCalculated() const
    { return calc; }

    virtual void setTrim(bool trim)
    { this->trim = trim; }
    virtual void setGenerated(bool generated)
    { gen = generated; }
    virtual void setCalculated(bool calculated)
    { calc = calculated; }

};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QT_NO_SQL

#endif // Q3SQLFIELDINFO_H
