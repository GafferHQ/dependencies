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

#ifndef Q3RANGECONTROL_H
#define Q3RANGECONTROL_H

#include <QtCore/qglobal.h>
#include <QtGui/qwidget.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Qt3SupportLight)

#ifndef QT_NO_RANGECONTROL

class Q3RangeControlPrivate;

class Q_COMPAT_EXPORT Q3RangeControl
{
public:
    Q3RangeControl();
    Q3RangeControl(int minValue, int maxValue,
                   int lineStep, int pageStep, int value);
    virtual ~Q3RangeControl();

    int                value()                const;
    void        setValue(int);
    void        addPage();
    void        subtractPage();
    void        addLine();
    void        subtractLine();

    int                minValue()        const;
    int                maxValue()        const;
    void        setRange(int minValue, int maxValue);
    void        setMinValue(int minVal);
    void        setMaxValue(int minVal);

    int                lineStep()        const;
    int                pageStep()        const;
    void        setSteps(int line, int page);

    int                bound(int) const;

protected:
    int                positionFromValue(int val, int space) const;
    int                valueFromPosition(int pos, int space) const;
    void        directSetValue(int val);
    int                prevValue()        const;

    virtual void valueChange();
    virtual void rangeChange();
    virtual void stepChange();

private:
    int                minVal, maxVal;
    int                line, page;
    int                val, prevVal;

    Q3RangeControlPrivate * d;

private:
    Q_DISABLE_COPY(Q3RangeControl)
};


inline int Q3RangeControl::value() const
{ return val; }

inline int Q3RangeControl::prevValue() const
{ return prevVal; }

inline int Q3RangeControl::minValue() const
{ return minVal; }

inline int Q3RangeControl::maxValue() const
{ return maxVal; }

inline int Q3RangeControl::lineStep() const
{ return line; }

inline int Q3RangeControl::pageStep() const
{ return page; }


#endif // QT_NO_RANGECONTROL

#ifndef QT_NO_SPINWIDGET

class Q3SpinWidgetPrivate;
class Q_COMPAT_EXPORT Q3SpinWidget : public QWidget
{
    Q_OBJECT
public:
    Q3SpinWidget(QWidget* parent=0, const char* name=0);
    ~Q3SpinWidget();

    void         setEditWidget(QWidget * widget);
    QWidget *         editWidget();

    QRect upRect() const;
    QRect downRect() const;

    void setUpEnabled(bool on);
    void setDownEnabled(bool on);

    bool isUpEnabled() const;
    bool isDownEnabled() const;

    enum ButtonSymbols { UpDownArrows, PlusMinus };
    virtual void        setButtonSymbols(ButtonSymbols bs);
    ButtonSymbols        buttonSymbols() const;

    void arrange();

Q_SIGNALS:
    void stepUpPressed();
    void stepDownPressed();

public Q_SLOTS:
    void stepUp();
    void stepDown();

protected:
    void mousePressEvent(QMouseEvent *e);
    void resizeEvent(QResizeEvent* ev);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
#ifndef QT_NO_WHEELEVENT
    void wheelEvent(QWheelEvent *);
#endif
    void changeEvent(QEvent *);
    void paintEvent(QPaintEvent *);

private Q_SLOTS:
    void timerDone();
    void timerDoneEx();

private:
    Q3SpinWidgetPrivate * d;

    void updateDisplay();

private:
    Q_DISABLE_COPY(Q3SpinWidget)
};

#endif // QT_NO_RANGECONTROL

QT_END_NAMESPACE

QT_END_HEADER

#endif // Q3RANGECONTROL_H
