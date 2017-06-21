/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKSPINBOXVALIDATOR_P_H
#define QQUICKSPINBOXVALIDATOR_P_H

#include <QtGui/qvalidator.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QQuickSpinBoxValidator : public QValidator, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(qreal minimumValue READ minimumValue WRITE setMinimumValue NOTIFY minimumValueChanged)
    Q_PROPERTY(qreal maximumValue READ maximumValue WRITE setMaximumValue NOTIFY maximumValueChanged)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals NOTIFY decimalsChanged)
    Q_PROPERTY(qreal stepSize READ stepSize WRITE setStepSize NOTIFY stepSizeChanged)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix NOTIFY prefixChanged)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix NOTIFY suffixChanged)

public:
    explicit QQuickSpinBoxValidator(QObject *parent = 0);
    virtual ~QQuickSpinBoxValidator();

    QString text() const;

    qreal value() const;
    void setValue(qreal value);

    qreal minimumValue() const;
    void setMinimumValue(qreal min);

    qreal maximumValue() const;
    void setMaximumValue(qreal max);

    int decimals() const;
    void setDecimals(int decimals);

    qreal stepSize() const;
    void setStepSize(qreal step);

    QString prefix() const;
    void setPrefix(const QString &prefix);

    QString suffix() const;
    void setSuffix(const QString &suffix);

    void fixup(QString &input) const;
    State validate(QString &input, int &pos) const;

    void classBegin() { }
    void componentComplete();

public Q_SLOTS:
    void increment();
    void decrement();

Q_SIGNALS:
    void valueChanged();
    void minimumValueChanged();
    void maximumValueChanged();
    void decimalsChanged();
    void stepSizeChanged();
    void prefixChanged();
    void suffixChanged();
    void textChanged();

protected:
    QString textFromValue(qreal value) const;

private:
    qreal m_value;
    qreal m_step;
    QString m_prefix;
    QString m_suffix;
    bool m_initialized;
    QDoubleValidator m_validator;

    Q_DISABLE_COPY(QQuickSpinBoxValidator)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickSpinBoxValidator)

#endif // QQUICKSPINBOXVALIDATOR_P_H
