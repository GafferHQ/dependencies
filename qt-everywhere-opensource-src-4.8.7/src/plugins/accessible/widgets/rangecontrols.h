/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef RANGECONTROLS_H
#define RANGECONTROLS_H

#include <QtGui/qaccessiblewidget.h>
#include <QtGui/qaccessible2.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

class QAbstractSpinBox;
class QAbstractSlider;
class QScrollBar;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QDial;

#ifndef QT_NO_SPINBOX
class QAccessibleAbstractSpinBox: public QAccessibleWidgetEx, public QAccessibleValueInterface
{
    Q_ACCESSIBLE_OBJECT
public:
    explicit QAccessibleAbstractSpinBox(QWidget *w);

    enum SpinBoxElements {
        SpinBoxSelf        = 0,
        Editor,
        ValueUp,
        ValueDown
    };

    int childCount() const;
    QRect rect(int child) const;

    int navigate(RelationFlag rel, int entry, QAccessibleInterface **target) const;

    QString text(Text t, int child) const;
    Role role(int child) const;

    bool doAction(int action, int child, const QVariantList &params);

    QVariant invokeMethodEx(Method method, int child, const QVariantList &params);

    // QAccessibleValueInterface
    QVariant currentValue();
    void setCurrentValue(const QVariant &value);
    QVariant maximumValue();
    QVariant minimumValue();

protected:
    QAbstractSpinBox *abstractSpinBox() const;
};

class QAccessibleSpinBox : public QAccessibleAbstractSpinBox
{
public:
    explicit QAccessibleSpinBox(QWidget *w);

    State state(int child) const;

    bool doAction(int action, int child, const QVariantList &params);

protected:
    QSpinBox *spinBox() const;
};

class QAccessibleDoubleSpinBox : public QAccessibleWidgetEx
{
public:
    explicit QAccessibleDoubleSpinBox(QWidget *widget);

    enum DoubleSpinBoxElements {
        SpinBoxSelf = 0,
        Editor,
        ValueUp,
        ValueDown
    };

    int childCount() const;
    QRect rect(int child) const;
    int navigate(RelationFlag rel, int entry, QAccessibleInterface **target) const;
    QVariant invokeMethodEx(QAccessible::Method method, int child, const QVariantList &params);
    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;

protected:
    QDoubleSpinBox *doubleSpinBox() const;
};
#endif // QT_NO_SPINBOX

class QAccessibleAbstractSlider: public QAccessibleWidgetEx, public QAccessibleValueInterface
{
    Q_ACCESSIBLE_OBJECT
public:
    explicit QAccessibleAbstractSlider(QWidget *w, Role r = Slider);

    QVariant invokeMethodEx(Method method, int child, const QVariantList &params);

    // QAccessibleValueInterface
    QVariant currentValue();
    void setCurrentValue(const QVariant &value);
    QVariant maximumValue();
    QVariant minimumValue();

protected:
    QAbstractSlider *abstractSlider() const;
};

#ifndef QT_NO_SCROLLBAR
class QAccessibleScrollBar : public QAccessibleAbstractSlider
{
public:
    explicit QAccessibleScrollBar(QWidget *w);

    enum ScrollBarElements {
        ScrollBarSelf        = 0,
        LineUp,
        PageUp,
        Position,
        PageDown,
        LineDown
    };

    int childCount() const;

    QRect rect(int child) const;
    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;

protected:
    QScrollBar *scrollBar() const;
};
#endif // QT_NO_SCROLLBAR

#ifndef QT_NO_SLIDER
class QAccessibleSlider : public QAccessibleAbstractSlider
{
public:
    explicit QAccessibleSlider(QWidget *w);

    enum SliderElements {
        SliderSelf  = 0,
        PageLeft,
        Position,
        PageRight
    };

    int childCount() const;

    QRect rect(int child) const;
    QString text(Text t, int child) const;
    Role role(int child) const;
    State state(int child) const;

    int defaultAction(int child) const;
    QString actionText(int action, Text t, int child) const;

protected:
    QSlider *slider() const;
};
#endif // QT_NO_SLIDER

#ifndef QT_NO_DIAL
class QAccessibleDial : public QAccessibleWidgetEx
{
public:
    explicit QAccessibleDial(QWidget *w);

    enum DialElements {
        Self  = 0,
        SpeedoMeter,
        SliderHandle
    };

    int childCount() const;
    QRect rect(int child) const;
    QString text(Text textType, int child) const;
    Role role(int child) const;
    State state(int child) const;
    QVariant invokeMethodEx(Method method, int child, const QVariantList &params);

protected:
    QDial *dial() const;
};
#endif // QT_NO_DIAL

#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // RANGECONTROLS_H
