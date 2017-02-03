/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef QEASINGCURVE_H
#define QEASINGCURVE_H

#include <QtCore/qglobal.h>
#include <QtCore/qobjectdefs.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Core)

class QEasingCurvePrivate;
class Q_CORE_EXPORT QEasingCurve
{
    Q_GADGET
    Q_ENUMS(Type)
public:
    enum Type {
        Linear,
        InQuad, OutQuad, InOutQuad, OutInQuad,
        InCubic, OutCubic, InOutCubic, OutInCubic,
        InQuart, OutQuart, InOutQuart, OutInQuart,
        InQuint, OutQuint, InOutQuint, OutInQuint,
        InSine, OutSine, InOutSine, OutInSine,
        InExpo, OutExpo, InOutExpo, OutInExpo,
        InCirc, OutCirc, InOutCirc, OutInCirc,
        InElastic, OutElastic, InOutElastic, OutInElastic,
        InBack, OutBack, InOutBack, OutInBack,
        InBounce, OutBounce, InOutBounce, OutInBounce,
        InCurve, OutCurve, SineCurve, CosineCurve,
        Custom, NCurveTypes
    };

    QEasingCurve(Type type = Linear);
    QEasingCurve(const QEasingCurve &other);
    ~QEasingCurve();

    QEasingCurve &operator=(const QEasingCurve &other);
    bool operator==(const QEasingCurve &other) const;
    inline bool operator!=(const QEasingCurve &other) const
    { return !(this->operator==(other)); }

    qreal amplitude() const;
    void setAmplitude(qreal amplitude);

    qreal period() const;
    void setPeriod(qreal period);

    qreal overshoot() const;
    void setOvershoot(qreal overshoot);

    Type type() const;
    void setType(Type type);
    typedef qreal (*EasingFunction)(qreal progress);
    void setCustomType(EasingFunction func);
    EasingFunction customType() const;

    qreal valueForProgress(qreal progress) const;
private:
    QEasingCurvePrivate *d_ptr;
#ifndef QT_NO_DEBUG_STREAM
    friend Q_CORE_EXPORT QDebug operator<<(QDebug debug, const QEasingCurve &item);
#endif
#ifndef QT_NO_DATASTREAM
    friend Q_CORE_EXPORT QDataStream &operator<<(QDataStream &, const QEasingCurve&);
    friend Q_CORE_EXPORT QDataStream &operator>>(QDataStream &, QEasingCurve &);
#endif
};

#ifndef QT_NO_DEBUG_STREAM
Q_CORE_EXPORT QDebug operator<<(QDebug debug, const QEasingCurve &item);
#endif

#ifndef QT_NO_DATASTREAM
Q_CORE_EXPORT QDataStream &operator<<(QDataStream &, const QEasingCurve&);
Q_CORE_EXPORT QDataStream &operator>>(QDataStream &, QEasingCurve &);
#endif

QT_END_NAMESPACE

QT_END_HEADER

#endif
