/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include <QtCore/qpropertyanimation.h>
#include <QtGui/qwidget.h>
#include <QtGui/private/qmainwindowlayout_p.h>

#include "qwidgetanimator_p.h"

QT_BEGIN_NAMESPACE

QWidgetAnimator::QWidgetAnimator(QMainWindowLayout *layout) : m_mainWindowLayout(layout)
{
}

void QWidgetAnimator::abort(QWidget *w)
{
#ifndef QT_NO_ANIMATION
    AnimationMap::iterator it = m_animation_map.find(w);
    if (it == m_animation_map.end())
        return;
    QPropertyAnimation *anim = *it;
    m_animation_map.erase(it);
    if (anim) {
        anim->stop();
    }
#ifndef QT_NO_MAINWINDOW
    m_mainWindowLayout->animationFinished(w);
#endif
#else
    Q_UNUSED(w); //there is no animation to abort
#endif //QT_NO_ANIMATION
}

#ifndef QT_NO_ANIMATION
void QWidgetAnimator::animationFinished()
{
    QPropertyAnimation *anim = qobject_cast<QPropertyAnimation*>(sender());
    abort(static_cast<QWidget*>(anim->targetObject()));
}
#endif //QT_NO_ANIMATION

void QWidgetAnimator::animate(QWidget *widget, const QRect &_final_geometry, bool animate)
{
    QRect r = widget->geometry();
    if (r.right() < 0 || r.bottom() < 0)
        r = QRect();

    animate = animate && !r.isNull() && !_final_geometry.isNull();

    // might make the wigdet go away by sending it to negative space
    const QRect final_geometry = _final_geometry.isValid() || widget->isWindow() ? _final_geometry :
        QRect(QPoint(-500 - widget->width(), -500 - widget->height()), widget->size());

#ifndef QT_NO_ANIMATION
    AnimationMap::const_iterator it = m_animation_map.constFind(widget);
    if (it != m_animation_map.constEnd() && (*it)->endValue().toRect() == final_geometry)
        return;

    QPropertyAnimation *anim = new QPropertyAnimation(widget, "geometry", widget);
    anim->setDuration(animate ? 200 : 0);
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    anim->setEndValue(final_geometry);
    m_animation_map[widget] = anim;
    connect(anim, SIGNAL(finished()), SLOT(animationFinished()));
    anim->start(QPropertyAnimation::DeleteWhenStopped);
#else
    //we do it in one shot
    widget->setGeometry(final_geometry);
#ifndef QT_NO_MAINWINDOW
    m_mainWindowLayout->animationFinished(widget);
#endif //QT_NO_MAINWINDOW
#endif //QT_NO_ANIMATION
}

bool QWidgetAnimator::animating() const
{
    return !m_animation_map.isEmpty();
}

QT_END_NAMESPACE
