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

#ifndef QQUICKRANGEMODEL_P_H
#define QQUICKRANGEMODEL_P_H

#include <QtCore/qobject.h>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class QQuickRangeModelPrivate;

class QQuickRangeModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal value READ value WRITE setValue NOTIFY valueChanged USER true)
    Q_PROPERTY(qreal minimumValue READ minimum WRITE setMinimum NOTIFY minimumChanged)
    Q_PROPERTY(qreal maximumValue READ maximum WRITE setMaximum NOTIFY maximumChanged)
    Q_PROPERTY(qreal stepSize READ stepSize WRITE setStepSize NOTIFY stepSizeChanged)
    Q_PROPERTY(qreal position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(qreal positionAtMinimum READ positionAtMinimum WRITE setPositionAtMinimum NOTIFY positionAtMinimumChanged)
    Q_PROPERTY(qreal positionAtMaximum READ positionAtMaximum WRITE setPositionAtMaximum NOTIFY positionAtMaximumChanged)
    Q_PROPERTY(bool inverted READ inverted WRITE setInverted NOTIFY invertedChanged)

public:
    QQuickRangeModel(QObject *parent = 0);
    virtual ~QQuickRangeModel();

    void setRange(qreal min, qreal max);
    void setPositionRange(qreal min, qreal max);

    void setStepSize(qreal stepSize);
    qreal stepSize() const;

    void setMinimum(qreal min);
    qreal minimum() const;

    void setMaximum(qreal max);
    qreal maximum() const;

    void setPositionAtMinimum(qreal posAtMin);
    qreal positionAtMinimum() const;

    void setPositionAtMaximum(qreal posAtMax);
    qreal positionAtMaximum() const;

    void setInverted(bool inverted);
    bool inverted() const;

    qreal value() const;
    qreal position() const;

    Q_INVOKABLE qreal valueForPosition(qreal position) const;
    Q_INVOKABLE qreal positionForValue(qreal value) const;

public Q_SLOTS:
    void toMinimum();
    void toMaximum();
    void setValue(qreal value);
    void setPosition(qreal position);
    void increaseSingleStep();
    void decreaseSingleStep();

Q_SIGNALS:
    void valueChanged(qreal value);
    void positionChanged(qreal position);

    void stepSizeChanged(qreal stepSize);

    void invertedChanged(bool inverted);

    void minimumChanged(qreal min);
    void maximumChanged(qreal max);
    void positionAtMinimumChanged(qreal min);
    void positionAtMaximumChanged(qreal max);

protected:
    QQuickRangeModel(QQuickRangeModelPrivate &dd, QObject *parent);
    QQuickRangeModelPrivate* d_ptr;

private:
    Q_DISABLE_COPY(QQuickRangeModel)
    Q_DECLARE_PRIVATE(QQuickRangeModel)

};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickRangeModel)

#endif // QQUICKRANGEMODEL_P_H
