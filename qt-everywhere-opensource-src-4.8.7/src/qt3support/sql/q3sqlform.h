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

#ifndef Q3SQLFORM_H
#define Q3SQLFORM_H

#include <QtCore/qobject.h>
#include <QtCore/qmap.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Qt3Support)

#ifndef QT_NO_SQL_FORM

class QSqlField;
class QSqlRecord;
class Q3SqlEditorFactory;
class Q3SqlPropertyMap;
class QWidget;
class Q3SqlFormPrivate;

class Q_COMPAT_EXPORT Q3SqlForm : public QObject
{
    Q_OBJECT
public:
    Q3SqlForm(QObject * parent = 0);
    ~Q3SqlForm();

    virtual void insert(QWidget * widget, const QString& field);
    virtual void remove(const QString& field);
    int         count() const;

    QWidget *   widget(int i) const;
    QSqlField * widgetToField(QWidget * widget) const;
    QWidget *   fieldToWidget(QSqlField * field) const;

    void        installPropertyMap(Q3SqlPropertyMap * map);

    virtual void setRecord(QSqlRecord* buf);

public Q_SLOTS:
    virtual void readField(QWidget * widget);
    virtual void writeField(QWidget * widget);
    virtual void readFields();
    virtual void writeFields();

    virtual void clear();
    virtual void clearValues();

protected:
    virtual void insert(QWidget * widget, QSqlField * field);
    virtual void remove(QWidget * widget);
    void clearMap();

private:
    Q_DISABLE_COPY(Q3SqlForm)

    virtual void sync();
    Q3SqlFormPrivate* d;
};

#endif // QT_NO_SQL_FORM

QT_END_NAMESPACE

QT_END_HEADER

#endif // Q3SQLFORM_H
