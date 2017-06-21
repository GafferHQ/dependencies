/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
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
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickbehavior_p.h"

#include "qquickanimation_p.h"
#include <qqmlcontext.h>
#include <qqmlinfo.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmlengine_p.h>
#include <private/qabstractanimationjob_p.h>
#include <private/qquicktransition_p.h>

#include <private/qquickanimatorjob_p.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QQuickBehaviorPrivate : public QObjectPrivate, public QAnimationJobChangeListener
{
    Q_DECLARE_PUBLIC(QQuickBehavior)
public:
    QQuickBehaviorPrivate() : animation(0), animationInstance(0), enabled(true), finalized(false)
      , blockRunningChanged(false) {}

    virtual void animationStateChanged(QAbstractAnimationJob *, QAbstractAnimationJob::State newState, QAbstractAnimationJob::State oldState);

    QQmlProperty property;
    QVariant targetValue;
    QPointer<QQuickAbstractAnimation> animation;
    QAbstractAnimationJob *animationInstance;
    bool enabled;
    bool finalized;
    bool blockRunningChanged;
};

/*!
    \qmltype Behavior
    \instantiates QQuickBehavior
    \inqmlmodule QtQuick
    \ingroup qtquick-transitions-animations
    \ingroup qtquick-interceptors
    \brief Defines a default animation for a property change

    A Behavior defines the default animation to be applied whenever a
    particular property value changes.

    For example, the following Behavior defines a NumberAnimation to be run
    whenever the \l Rectangle's \c width value changes. When the MouseArea
    is clicked, the \c width is changed, triggering the behavior's animation:

    \snippet qml/behavior.qml 0

    Note that a property cannot have more than one assigned Behavior. To provide
    multiple animations within a Behavior, use ParallelAnimation or
    SequentialAnimation.

    If a \l{Qt Quick States}{state change} has a \l Transition that matches the same property as a
    Behavior, the \l Transition animation overrides the Behavior for that
    state change. For general advice on using Behaviors to animate state changes, see
    \l{Using Qt Quick Behaviors with States}.

    \sa {Animation and Transitions in Qt Quick}, {Qt Quick Examples - Animation#Behaviors}{Behavior example}, {Qt QML}
*/


QQuickBehavior::QQuickBehavior(QObject *parent)
    : QObject(*(new QQuickBehaviorPrivate), parent)
{
}

QQuickBehavior::~QQuickBehavior()
{
    Q_D(QQuickBehavior);
    delete d->animationInstance;
}

/*!
    \qmlproperty Animation QtQuick::Behavior::animation
    \default

    This property holds the animation to run when the behavior is triggered.
*/

QQuickAbstractAnimation *QQuickBehavior::animation()
{
    Q_D(QQuickBehavior);
    return d->animation;
}

void QQuickBehavior::setAnimation(QQuickAbstractAnimation *animation)
{
    Q_D(QQuickBehavior);
    if (d->animation) {
        qmlInfo(this) << tr("Cannot change the animation assigned to a Behavior.");
        return;
    }

    d->animation = animation;
    if (d->animation) {
        d->animation->setDefaultTarget(d->property);
        d->animation->setDisableUserControl();
    }
}


void QQuickBehaviorPrivate::animationStateChanged(QAbstractAnimationJob *, QAbstractAnimationJob::State newState,QAbstractAnimationJob::State)
{
    if (!blockRunningChanged)
        animation->notifyRunningChanged(newState == QAbstractAnimationJob::Running);
}

/*!
    \qmlproperty bool QtQuick::Behavior::enabled

    This property holds whether the behavior will be triggered when the tracked
    property changes value.

    By default a Behavior is enabled.
*/

bool QQuickBehavior::enabled() const
{
    Q_D(const QQuickBehavior);
    return d->enabled;
}

void QQuickBehavior::setEnabled(bool enabled)
{
    Q_D(QQuickBehavior);
    if (d->enabled == enabled)
        return;
    d->enabled = enabled;
    emit enabledChanged();
}

void QQuickBehavior::write(const QVariant &value)
{
    Q_D(QQuickBehavior);
    bool bypass = !d->enabled || !d->finalized || QQmlEnginePrivate::designerMode();
    if (!bypass)
        qmlExecuteDeferred(this);
    if (!d->animation || bypass) {
        if (d->animationInstance)
            d->animationInstance->stop();
        QQmlPropertyPrivate::write(d->property, value, QQmlPropertyPrivate::BypassInterceptor | QQmlPropertyPrivate::DontRemoveBinding);
        d->targetValue = value;
        return;
    }

    bool behaviorActive = d->animation->isRunning();
    if (behaviorActive && value == d->targetValue)
        return;

    d->targetValue = value;

    if (d->animationInstance
            && (d->animationInstance->duration() != -1
                || d->animationInstance->isRenderThreadProxy())
            && !d->animationInstance->isStopped()) {
        d->blockRunningChanged = true;
        d->animationInstance->stop();
    }
    // Render thread animations use "stop" to synchronize the property back
    // to the item, so we need to read the value after.
    const QVariant &currentValue = d->property.read();

    // Don't unnecessarily wake up the animation system if no real animation
    // is needed (value has not changed). If the Behavior was already
    // running, let it continue as normal to ensure correct behavior and state.
    if (!behaviorActive && d->targetValue == currentValue) {
        QQmlPropertyPrivate::write(d->property, value, QQmlPropertyPrivate::BypassInterceptor | QQmlPropertyPrivate::DontRemoveBinding);
        return;
    }

    QQuickStateOperation::ActionList actions;
    QQuickStateAction action;
    action.property = d->property;
    action.fromValue = currentValue;
    action.toValue = value;
    actions << action;

    QList<QQmlProperty> after;
    QAbstractAnimationJob *prev = d->animationInstance;
    d->animationInstance = d->animation->transition(actions, after, QQuickAbstractAnimation::Forward);

    if (d->animationInstance && d->animation->threadingModel() == QQuickAbstractAnimation::RenderThread)
        d->animationInstance = new QQuickAnimatorProxyJob(d->animationInstance, d->animation);

    if (prev && prev != d->animationInstance)
        delete prev;

    if (d->animationInstance) {
        if (d->animationInstance != prev)
            d->animationInstance->addAnimationChangeListener(d, QAbstractAnimationJob::StateChange);
        d->animationInstance->start();
        d->blockRunningChanged = false;
    }
    if (!after.contains(d->property))
        QQmlPropertyPrivate::write(d->property, value, QQmlPropertyPrivate::BypassInterceptor | QQmlPropertyPrivate::DontRemoveBinding);
}

void QQuickBehavior::setTarget(const QQmlProperty &property)
{
    Q_D(QQuickBehavior);
    d->property = property;
    if (d->animation)
        d->animation->setDefaultTarget(property);

    QQmlEnginePrivate *engPriv = QQmlEnginePrivate::get(qmlEngine(this));
    static int finalizedIdx = -1;
    if (finalizedIdx < 0)
        finalizedIdx = metaObject()->indexOfSlot("componentFinalized()");
    engPriv->registerFinalizeCallback(this, finalizedIdx);
}

void QQuickBehavior::componentFinalized()
{
    Q_D(QQuickBehavior);
    d->finalized = true;
}

QT_END_NAMESPACE
