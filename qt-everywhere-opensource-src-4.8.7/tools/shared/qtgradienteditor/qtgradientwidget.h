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

#ifndef QTGRADIENTWIDGET_H
#define QTGRADIENTWIDGET_H

#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class QtGradientWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool backgroundCheckered READ isBackgroundCheckered WRITE setBackgroundCheckered)
public:
    QtGradientWidget(QWidget *parent = 0);
    ~QtGradientWidget();

    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    int heightForWidth(int w) const;

    bool isBackgroundCheckered() const;
    void setBackgroundCheckered(bool checkered);

    QGradientStops gradientStops() const;

    void setGradientType(QGradient::Type type);
    QGradient::Type gradientType() const;

    void setGradientSpread(QGradient::Spread spread);
    QGradient::Spread gradientSpread() const;

    void setStartLinear(const QPointF &point);
    QPointF startLinear() const;

    void setEndLinear(const QPointF &point);
    QPointF endLinear() const;

    void setCentralRadial(const QPointF &point);
    QPointF centralRadial() const;

    void setFocalRadial(const QPointF &point);
    QPointF focalRadial() const;

    void setRadiusRadial(qreal radius);
    qreal radiusRadial() const;

    void setCentralConical(const QPointF &point);
    QPointF centralConical() const;

    void setAngleConical(qreal angle);
    qreal angleConical() const;

public slots:
    void setGradientStops(const QGradientStops &stops);
signals:

    void startLinearChanged(const QPointF &point);
    void endLinearChanged(const QPointF &point);
    void centralRadialChanged(const QPointF &point);
    void focalRadialChanged(const QPointF &point);
    void radiusRadialChanged(qreal radius);
    void centralConicalChanged(const QPointF &point);
    void angleConicalChanged(qreal angle);

protected:
    void paintEvent(QPaintEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseDoubleClickEvent(QMouseEvent *e);

private:
    QScopedPointer<class QtGradientWidgetPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QtGradientWidget)
    Q_DISABLE_COPY(QtGradientWidget)
};

QT_END_NAMESPACE

#endif
