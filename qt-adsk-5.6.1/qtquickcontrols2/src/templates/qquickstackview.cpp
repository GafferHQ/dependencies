/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Templates module of the Qt Toolkit.
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

#include "qquickstackview_p.h"
#include "qquickstackview_p_p.h"

#include <QtQml/qjsvalue.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlinfo.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype StackView
    \inherits Control
    \instantiates QQuickStackView
    \inqmlmodule Qt.labs.controls
    \ingroup qtlabscontrols-navigation
    \ingroup qtlabscontrols-containers
    \brief Provides a stack-based navigation model.

    \image qtlabscontrols-stackview-wireframe.png

    StackView can be used with a set of inter-linked information pages. For
    example, an email application with separate views to list latest emails,
    view a specific email, and list/view the attachments. The email list view
    is pushed onto the stack as users open an email, and popped out as they
    choose to go back.

    The following snippet demonstrates a simple use case, where the \c mainView
    is pushed onto and popped out of the stack on relevant button click:

    \qml
    ApplicationWindow {
        title: qsTr("Hello World")
        width: 640
        height: 480
        visible: true

        StackView {
            id: stack
            initialItem: mainView
            anchors.fill: parent
        }

        Component {
            id: mainView

            Row {
                spacing: 10

                Button {
                    text: "Push"
                    onClicked: stack.push(mainView)
                }
                Button {
                    text: "Pop"
                    enabled: stack.depth > 1
                    onClicked: stack.pop()

                }
                Text {
                    text: stack.depth
                }
            }
        }
    }
    \endqml

    \section1 Using StackView in an Application

    Using StackView in an application is as simple as adding it as a child to
    a Window. The stack is usually anchored to the edges of the window, except
    at the top or bottom where it might be anchored to a status bar, or some
    other similar UI component. The stack can then be  used by invoking its
    navigation methods. The first item to show in the StackView is the one
    that was assigned to \l initialItem, or the topmost item if \l initialItem
    is not set.

    \section1 Basic Navigation

    StackView supports three primary navigation operations: push(), pop(), and
    replace(). These correspond to classic stack operations where "push" adds
    an item to the top of a stack, "pop" removes the top item from the
    stack, and "replace" is like a pop followed by a push, which replaces the
    topmost item with the new item. The topmost item in the stack
    corresponds to the one that is \l{StackView::currentItem}{currently}
    visible on screen. Logically, "push" navigates forward or deeper into the
    application UI, "pop" navigates backward, and "replace" replaces the
    \l currentItem.

    Sometimes, it is necessary to go back more than a single step in the stack.
    For example, to return to a "main" item or some kind of section item in the
    application. In such cases, it is possible to specify an item as a
    parameter for pop(). This is called an "unwind" operation, where the stack
    unwinds till the specified item. If the item is not found, stack unwinds
    until it is left with one item, which becomes the \l currentItem. To
    explicitly unwind to the bottom of the stack, it is recommended to use
    \l{pop()}{pop(null)}, although any non-existent item will do.

    Given the stack [A, B, C]:

    \list
    \li \l{push()}{push(D)} => [A, B, C, D] - "push" transition animation
        between C and D
    \li pop() => [A, B] - "pop" transition animation between C and B
    \li \l{replace()}{replace(D)} => [A, B, D] - "replace" transition between
        C and D
    \li \l{pop()}{pop(A)} => [A] - "pop" transition between C and A
    \endlist

    \note When the stack is empty, a push() operation will not have a
    transition animation because there is nothing to transition from (typically
    on application start-up). A pop() operation on a stack with depth 1 or
    0 does nothing. In such cases, the stack can be emptied using the clear()
    method.

    \section1 Deep Linking

    \e{Deep linking} means launching an application into a particular state. For
    example, a newspaper application could be launched into showing a
    particular article, bypassing the topmost item. In terms of StackView, deep linking means the ability to modify
    the state of the stack, so much so that it is possible to push a set of
    items to the top of the stack, or to completely reset the stack to a given
    state.

    The API for deep linking in StackView is the same as for basic navigation.
    Pushing an array instead of a single item adds all the items in that array
    to the stack. The transition animation, however, is applied only for the
    last item in the array. The normal semantics of push() apply for deep
    linking, that is, it adds whatever is pushed onto the stack.

    \note Only the last item of the array is loaded. The rest of the items are
    loaded only when needed, either on subsequent calls to pop or on request to
    get an item using get().

    This gives us the following result, given the stack [A, B, C]:

    \list
    \li \l{push()}{push([D, E, F])} => [A, B, C, D, E, F] - "push" transition
        animation between C and F
    \li \l{replace()}{replace([D, E, F])} => [A, B, D, E, F] - "replace"
        transition animation between C and F
    \li \l{clear()} followed by \l{push()}{push([D, E, F])} => [D, E, F] - no
        transition animation for pushing items as the stack was empty.
    \endlist

    \section1 Finding Items

    An Item for which the application does not have a reference can be found
    by calling find(). The method needs a callback function, which is invoked
    for each item in the stack (starting at the top) until a match is found.
    If the callback returns \c true, find() stops and returns the matching
    item, otherwise \c null is returned.

    The code below searches the stack for an item named "order_id" and unwinds
    to that item.

    \badcode
    stackView.pop(stackView.find(function(item) {
        return item.name == "order_id";
    }));
    \endcode

    You can also get to an item in the stack using \l {get()}{get(index)}.

    \badcode
    previousItem = stackView.get(myItem.StackView.index - 1));
    \endcode

    \section1 Transitions

    For each push or pop operation, different transition animations are applied
    to entering and exiting items. These animations define how the entering item
    should animate in, and the exiting item should animate out. The animations
    can be customized by assigning different \l{Transition}s for the
    \l pushEnter, \l pushExit, \l popEnter, \l popExit, \l replaceEnter, and
    \l replaceExit properties of StackView.

    \note The transition animations affect each others' transitional behavior.
    Customizing the animation for one and leaving the other may give unexpected
    results.

    The following snippet defines a simple fade transition for push and pop
    operations:

    \qml
    StackView {
        id: stackview
        anchors.fill: parent

        pushEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to:1
                duration: 200
            }
        }
        pushExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to:0
                duration: 200
            }
        }
        popEnter: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to:1
                duration: 200
            }
        }
        popExit: Transition {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to:0
                duration: 200
            }
        }
    }
    \endqml

    \note Using anchors on the items added to a StackView is not supported.
          Typically push, pop, and replace transitions animate the position,
          which is not possible when anchors are applied. Notice that this
          only applies to the root of the item. Using anchors for its children
          works as expected.

    \labs

    \sa {Customizing StackView}, {Navigation Controls}, {Container Controls}
*/

QQuickStackView::QQuickStackView(QQuickItem *parent) :
    QQuickControl(*(new QQuickStackViewPrivate), parent)
{
    setFlag(ItemIsFocusScope);
}

QQuickStackView::~QQuickStackView()
{
    Q_D(QQuickStackView);
    if (d->transitioner) {
        d->transitioner->setChangeListener(Q_NULLPTR);
        delete d->transitioner;
    }
    qDeleteAll(d->removals);
    qDeleteAll(d->elements);
}

QQuickStackAttached *QQuickStackView::qmlAttachedProperties(QObject *object)
{
    QQuickItem *item = qobject_cast<QQuickItem *>(object);
    if (!item) {
        qmlInfo(object) << "StackView must be attached to an Item";
        return Q_NULLPTR;
    }
    return new QQuickStackAttached(item);
}

/*!
    \qmlproperty bool Qt.labs.controls::StackView::busy
    \readonly
    This property holds whether a transition is running.
*/
bool QQuickStackView::isBusy() const
{
    Q_D(const QQuickStackView);
    return d->busy;
}

/*!
    \qmlproperty int Qt.labs.controls::StackView::depth
    \readonly
    This property holds the number of items currently pushed onto the stack.
*/
int QQuickStackView::depth() const
{
    Q_D(const QQuickStackView);
    return d->elements.count();
}

/*!
    \qmlproperty Item Qt.labs.controls::StackView::currentItem
    \readonly
    This property holds the current top-most item in the stack.
*/
QQuickItem *QQuickStackView::currentItem() const
{
    Q_D(const QQuickStackView);
    return d->currentItem;
}

/*!
    \qmlmethod Item Qt.labs.controls::StackView::get(index, behavior)

    Returns the item at position \a index in the stack, or \c null if the index
    is out of bounds.

    Supported behavior values:
    \value StackView.DontLoad The item is not forced to load (and \c null is returned if not yet loaded).
    \value StackView.ForceLoad The item is forced to load.
*/
QQuickItem *QQuickStackView::get(int index, LoadBehavior behavior)
{
    Q_D(QQuickStackView);
    QQuickStackElement *element = d->elements.value(index);
    if (element) {
        if (behavior == ForceLoad)
            element->load(this);
        return element->item;
    }
    return Q_NULLPTR;
}

/*!
    \qmlmethod Item Qt.labs.controls::StackView::find(callback, behavior)

    Search for a specific item inside the stack. The \a callback function is called
    for each item in the stack (with the item and index as arguments) until the callback
    function returns \c true. The return value is the item found. For example:

    \code
    stackView.find(function(item, index) {
        return item.isTheOne
    })
    \endcode

    Supported behavior values:
    \value StackView.DontLoad Unloaded items are skipped (the callback function is not called for them).
    \value StackView.ForceLoad Unloaded items are forced to load.
*/
QQuickItem *QQuickStackView::find(const QJSValue &callback, LoadBehavior behavior)
{
    Q_D(QQuickStackView);
    QJSValue func(callback);
    QQmlEngine *engine = qmlEngine(this);
    if (!engine || !func.isCallable()) // TODO: warning?
        return Q_NULLPTR;

    for (int i = d->elements.count() - 1; i >= 0; --i) {
        QQuickStackElement *element = d->elements.at(i);
        if (behavior == ForceLoad)
            element->load(this);
        if (element->item) {
            QJSValue rv = func.call(QJSValueList() << engine->newQObject(element->item) << i);
            if (rv.toBool())
                return element->item;
        }
    }

    return Q_NULLPTR;
}

/*!
    \qmlmethod Item Qt.labs.controls::StackView::push(item, properties, operation)

    Pushes an \a item onto the stack using the specified \a operation, and
    optionally applies a set of \a properties on the item. The item can be
    an \l Item, \l Component, or a \l [QML] url. Returns the item that became
    current.

    Pushing a single item:
    \code
    stackView.push(rect)

    // or with properties:
    stackView.push(rect, {"color": "red"})
    \endcode

    Multiple items can be pushed at the same time either by passing them as
    additional arguments, or as an array. The last item becomes the current
    item. Each item can be followed by a set of properties to apply.

    Passing a variable amount of arguments:
    \code
    stackView.push(rect1, rect2, rect3)

    // or with properties:
    stackView.push(rect1, {"color": "red"}, rect2, {"color": "green"}, rect3, {"color": "blue"})
    \endcode

    Pushing an array of items:
    \code
    stackView.push([rect1, rect2, rect3])

    // or with properties:
    stackView.push([rect1, {"color": "red"}, rect2, {"color": "green"}, rect3, {"color": "blue"}])
    \endcode

    An \a operation can be optionally specified as the last argument. Supported
    operations:

    \value StackView.Transition An operation with transitions.
    \value StackView.Immediate An immediate operation without transitions.

    \sa initialItem
*/
void QQuickStackView::push(QQmlV4Function *args)
{
    Q_D(QQuickStackView);
    if (args->length() <= 0) {
        qmlInfo(this) << "push: missing arguments";
        args->setReturnValue(QV4::Encode::null());
        return;
    }

    QV4::ExecutionEngine *v4 = args->v4engine();
    QV4::Scope scope(v4);

    Operation operation = d->elements.isEmpty() ? Immediate : Transition;
    QV4::ScopedValue lastArg(scope, (*args)[args->length() - 1]);
    if (lastArg->isInt32())
        operation = static_cast<Operation>(lastArg->toInt32());

    QList<QQuickStackElement *> elements = d->parseElements(args);
    if (elements.isEmpty()) {
        qmlInfo(this) << "push: nothing to push";
        args->setReturnValue(QV4::Encode::null());
        return;
    }

    QQuickStackElement *exit = Q_NULLPTR;
    if (!d->elements.isEmpty())
        exit = d->elements.top();

    if (d->pushElements(elements)) {
        emit depthChanged();
        QQuickStackElement *enter = d->elements.top();
        d->pushTransition(enter, exit, boundingRect(), operation == Immediate);
        d->setCurrentItem(enter->item);
    }

    if (d->currentItem) {
        QV4::ScopedValue rv(scope, QV4::QObjectWrapper::wrap(v4, d->currentItem));
        args->setReturnValue(rv->asReturnedValue());
    } else {
        args->setReturnValue(QV4::Encode::null());
    }
}

/*!
    \qmlmethod Item Qt.labs.controls::StackView::pop(item, operation)

    Pops one or more items off the stack. Returns the last item removed from the stack.

    If the \a item argument is specified, all items down to (but not
    including) \a item will be popped. If \a item is \c null, all
    items down to (but not including) the first item is popped.
    If not specified, only the current item is popped.

    An \a operation can be optionally specified as the last argument. Supported
    operations:

    \value StackView.Transition An operation with transitions.
    \value StackView.Immediate An immediate operation without transitions.

    Examples:
    \code
    stackView.pop()
    stackView.pop(someItem, StackView.Immediate)
    stackView.pop(StackView.Immediate)
    stackView.pop(null)
    \endcode

    \sa clear()
*/
void QQuickStackView::pop(QQmlV4Function *args)
{
    Q_D(QQuickStackView);
    int argc = args->length();
    if (d->elements.count() <= 1 || argc > 2) {
        if (argc > 2)
            qmlInfo(this) << "pop: too many arguments";
        args->setReturnValue(QV4::Encode::null());
        return;
    }

    QQuickStackElement *exit = d->elements.pop();
    QQuickStackElement *enter = d->elements.top();

    QV4::ExecutionEngine *v4 = args->v4engine();
    QV4::Scope scope(v4);

    if (argc > 0) {
        QV4::ScopedValue value(scope, (*args)[0]);
        if (value->isNull()) {
            enter = d->elements.value(0);
        } else if (!value->isUndefined() && !value->isInt32()) {
            enter = d->findElement(value);
            if (!enter) {
                qmlInfo(this) << "pop: unknown argument: " << value->toQString(); // TODO: safe?
                args->setReturnValue(QV4::Encode::null());
                d->elements.push(exit); // restore
                return;
            }
        }
    }

    Operation operation = Transition;
    if (argc > 0) {
        QV4::ScopedValue lastArg(scope, (*args)[argc - 1]);
        if (lastArg->isInt32())
            operation = static_cast<Operation>(lastArg->toInt32());
    }

    QQuickItem *previousItem = Q_NULLPTR;

    if (d->popElements(enter)) {
        if (exit)
            previousItem = exit->item;
        emit depthChanged();
        d->popTransition(enter, exit, boundingRect(), operation == Immediate);
        d->setCurrentItem(enter->item);
    }

    if (previousItem) {
        QV4::ScopedValue rv(scope, QV4::QObjectWrapper::wrap(v4, previousItem));
        args->setReturnValue(rv->asReturnedValue());
    } else {
        args->setReturnValue(QV4::Encode::null());
    }
}

/*!
    \qmlmethod Item Qt.labs.controls::StackView::replace(target, item, properties, operation)

    Replaces one or more items on the stack with the specified \a item and
    \a operation, and optionally applies a set of \a properties on the
    item. The item can be an \l Item, \l Component, or a \l [QML] url.
    Returns the item that became current.

    If the \a target argument is specified, all items down to the \target
    item will be replaced. If \a target is \c null, all items in the stack
    will be replaced. If not specified, only the top item will be replaced.

    Replace the top item:
    \code
    stackView.replace(rect)

    // or with properties:
    stackView.replace(rect, {"color": "red"})
    \endcode

    Multiple items can be replaced at the same time either by passing them as
    additional arguments, or as an array. Each item can be followed by a set
    of properties to apply.

    Passing a variable amount of arguments:
    \code
    stackView.replace(rect1, rect2, rect3)

    // or with properties:
    stackView.replace(rect1, {"color": "red"}, rect2, {"color": "green"}, rect3, {"color": "blue"})
    \endcode

    Replacing an array of items:
    \code
    stackView.replace([rect1, rect2, rect3])

    // or with properties:
    stackView.replace([rect1, {"color": "red"}, rect2, {"color": "green"}, rect3, {"color": "blue"}])
    \endcode

    An \a operation can be optionally specified as the last argument. Supported
    operations:

    \value StackView.Transition An operation with transitions.
    \value StackView.Immediate An immediate operation without transitions.

    \sa push()
*/
void QQuickStackView::replace(QQmlV4Function *args)
{
    Q_D(QQuickStackView);
    if (args->length() <= 0) {
        qmlInfo(this) << "replace: missing arguments";
        args->setReturnValue(QV4::Encode::null());
        return;
    }

    QV4::ExecutionEngine *v4 = args->v4engine();
    QV4::Scope scope(v4);

    Operation operation = d->elements.isEmpty() ? Immediate : Transition;
    QV4::ScopedValue lastArg(scope, (*args)[args->length() - 1]);
    if (lastArg->isInt32())
        operation = static_cast<Operation>(lastArg->toInt32());

    QQuickStackElement *target = Q_NULLPTR;
    QV4::ScopedValue firstArg(scope, (*args)[0]);
    if (firstArg->isNull())
        target = d->elements.value(0);
    else if (!firstArg->isInt32())
        target = d->findElement(firstArg);

    QList<QQuickStackElement *> elements = d->parseElements(args, target ? 1 : 0);
    if (elements.isEmpty()) {
        qmlInfo(this) << "replace: nothing to push";
        args->setReturnValue(QV4::Encode::null());
        return;
    }

    int depth = d->elements.count();
    QQuickStackElement* exit = Q_NULLPTR;
    if (!d->elements.isEmpty())
        exit = d->elements.pop();

    if (exit != target ? d->replaceElements(target, elements) : d->pushElements(elements)) {
        if (depth != d->elements.count())
            emit depthChanged();
        QQuickStackElement *enter = d->elements.top();
        d->replaceTransition(enter, exit, boundingRect(), operation == Immediate);
        d->setCurrentItem(enter->item);
    }

    if (d->currentItem) {
        QV4::ScopedValue rv(scope, QV4::QObjectWrapper::wrap(v4, d->currentItem));
        args->setReturnValue(rv->asReturnedValue());
    } else {
        args->setReturnValue(QV4::Encode::null());
    }
}

/*!
    \qmlmethod void Qt.labs.controls::StackView::clear()

    Removes all items from the stack. No animations are applied.
*/
void QQuickStackView::clear()
{
    Q_D(QQuickStackView);
    d->setCurrentItem(Q_NULLPTR);
    qDeleteAll(d->elements);
    d->elements.clear();
    emit depthChanged();
}

/*!
    \qmlproperty var Qt.labs.controls::StackView::initialItem

    This property holds the initial item that should be shown when the StackView
    is created. The initial item can be an \l Item, \l Component, or a \l [QML] url.
    Specifying an initial item is equivalent to:
    \code
    Component.onCompleted: stackView.push(myInitialItem)
    \endcode

    \sa push()
*/
QVariant QQuickStackView::initialItem() const
{
    Q_D(const QQuickStackView);
    return d->initialItem;
}

void QQuickStackView::setInitialItem(const QVariant &item)
{
    Q_D(QQuickStackView);
    d->initialItem = item;
}

/*!
    \qmlproperty Transition Qt.labs.controls::StackView::popEnter

    This property holds the transition that is applied to the item that
    enters the stack when another item is popped off of it.

    \sa {Customizing StackView}
*/
QQuickTransition *QQuickStackView::popEnter() const
{
    Q_D(const QQuickStackView);
    if (d->transitioner)
        return d->transitioner->removeDisplacedTransition;
    return Q_NULLPTR;
}

void QQuickStackView::setPopEnter(QQuickTransition *enter)
{
    Q_D(QQuickStackView);
    d->ensureTransitioner();
    if (d->transitioner->removeDisplacedTransition != enter) {
        d->transitioner->removeDisplacedTransition = enter;
        emit popEnterChanged();
    }
}

/*!
    \qmlproperty Transition Qt.labs.controls::StackView::popExit

    This property holds the transition that is applied to the item that
    exits the stack when the item is popped off of it.

    \sa {Customizing StackView}
*/
QQuickTransition *QQuickStackView::popExit() const
{
    Q_D(const QQuickStackView);
    if (d->transitioner)
        return d->transitioner->removeTransition;
    return Q_NULLPTR;
}

void QQuickStackView::setPopExit(QQuickTransition *exit)
{
    Q_D(QQuickStackView);
    d->ensureTransitioner();
    if (d->transitioner->removeTransition != exit) {
        d->transitioner->removeTransition = exit;
        emit popExitChanged();
    }
}

/*!
    \qmlproperty Transition Qt.labs.controls::StackView::pushEnter

    This property holds the transition that is applied to the item that
    enters the stack when the item is pushed onto it.

    \sa {Customizing StackView}
*/
QQuickTransition *QQuickStackView::pushEnter() const
{
    Q_D(const QQuickStackView);
    if (d->transitioner)
        return d->transitioner->addTransition;
    return Q_NULLPTR;
}

void QQuickStackView::setPushEnter(QQuickTransition *enter)
{
    Q_D(QQuickStackView);
    d->ensureTransitioner();
    if (d->transitioner->addTransition != enter) {
        d->transitioner->addTransition = enter;
        emit pushEnterChanged();
    }
}

/*!
    \qmlproperty Transition Qt.labs.controls::StackView::pushExit

    This property holds the transition that is applied to the item that
    exits the stack when another item is pushed onto it.

    \sa {Customizing StackView}
*/
QQuickTransition *QQuickStackView::pushExit() const
{
    Q_D(const QQuickStackView);
    if (d->transitioner)
        return d->transitioner->addDisplacedTransition;
    return Q_NULLPTR;
}

void QQuickStackView::setPushExit(QQuickTransition *exit)
{
    Q_D(QQuickStackView);
    d->ensureTransitioner();
    if (d->transitioner->addDisplacedTransition != exit) {
        d->transitioner->addDisplacedTransition = exit;
        emit pushExitChanged();
    }
}

/*!
    \qmlproperty Transition Qt.labs.controls::StackView::replaceEnter

    This property holds the transition that is applied to the item that
    enters the stack when another item is replaced by it.

    \sa {Customizing StackView}
*/
QQuickTransition *QQuickStackView::replaceEnter() const
{
    Q_D(const QQuickStackView);
    if (d->transitioner)
        return d->transitioner->moveTransition;
    return Q_NULLPTR;
}

void QQuickStackView::setReplaceEnter(QQuickTransition *enter)
{
    Q_D(QQuickStackView);
    d->ensureTransitioner();
    if (d->transitioner->moveTransition != enter) {
        d->transitioner->moveTransition = enter;
        emit replaceEnterChanged();
    }
}

/*!
    \qmlproperty Transition Qt.labs.controls::StackView::replaceExit

    This property holds the transition that is applied to the item that
    exits the stack when it is replaced by another item.

    \sa {Customizing StackView}
*/
QQuickTransition *QQuickStackView::replaceExit() const
{
    Q_D(const QQuickStackView);
    if (d->transitioner)
        return d->transitioner->moveDisplacedTransition;
    return Q_NULLPTR;
}

void QQuickStackView::setReplaceExit(QQuickTransition *exit)
{
    Q_D(QQuickStackView);
    d->ensureTransitioner();
    if (d->transitioner->moveDisplacedTransition != exit) {
        d->transitioner->moveDisplacedTransition = exit;
        emit replaceExitChanged();
    }
}

void QQuickStackView::componentComplete()
{
    QQuickControl::componentComplete();

    Q_D(QQuickStackView);
    QQuickStackElement *element = Q_NULLPTR;
    if (QObject *o = d->initialItem.value<QObject *>())
        element = QQuickStackElement::fromObject(o, this);
    else if (d->initialItem.canConvert<QString>())
        element = QQuickStackElement::fromString(d->initialItem.toString(), this);
    if (d->pushElement(element)) {
        emit depthChanged();
        d->setCurrentItem(element->item);
        element->setStatus(QQuickStackView::Active);
    }
}

void QQuickStackView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickControl::geometryChanged(newGeometry, oldGeometry);

    Q_D(QQuickStackView);
    foreach (QQuickStackElement *element, d->elements) {
        if (element->item) {
            if (!element->widthValid)
                element->item->setWidth(newGeometry.width());
            if (!element->heightValid)
                element->item->setHeight(newGeometry.height());
        }
    }
}

bool QQuickStackView::childMouseEventFilter(QQuickItem *item, QEvent *event)
{
    // in order to block accidental user interaction while busy/transitioning,
    // StackView filters out childrens' mouse events. therefore we block all
    // press events. however, since push() may be called from signal handlers
    // such as onPressed or onDoubleClicked, we must let the current mouse
    // grabber item receive the respective mouse release event to avoid
    // breaking its state (QTBUG-50305).
    if (event->type() == QEvent::MouseButtonPress)
        return true;
    QQuickWindow *window = item->window();
    return window && !window->mouseGrabberItem();
}

void QQuickStackAttachedPrivate::itemParentChanged(QQuickItem *item, QQuickItem *parent)
{
    Q_Q(QQuickStackAttached);
    int oldIndex = element ? element->index : -1;
    QQuickStackView *oldView = element ? element->view : Q_NULLPTR;
    QQuickStackView::Status oldStatus = element ? element->status : QQuickStackView::Inactive;

    QQuickStackView *newView = qobject_cast<QQuickStackView *>(parent);
    element = newView ? QQuickStackViewPrivate::get(newView)->findElement(item) : Q_NULLPTR;

    int newIndex = element ? element->index : -1;
    QQuickStackView::Status newStatus = element ? element->status : QQuickStackView::Inactive;

    if (oldIndex != newIndex)
        emit q->indexChanged();
    if (oldView != newView)
        emit q->viewChanged();
    if (oldStatus != newStatus)
        emit q->statusChanged();
}

QQuickStackAttached::QQuickStackAttached(QQuickItem *parent) :
    QObject(*(new QQuickStackAttachedPrivate), parent)
{
    Q_D(QQuickStackAttached);
    QQuickItemPrivate::get(parent)->addItemChangeListener(d, QQuickItemPrivate::Parent);
    d->itemParentChanged(parent, parent->parentItem());
}

QQuickStackAttached::~QQuickStackAttached()
{
    Q_D(QQuickStackAttached);
    QQuickItem *parentItem = static_cast<QQuickItem *>(parent());
    QQuickItemPrivate::get(parentItem)->removeItemChangeListener(d, QQuickItemPrivate::Parent);
}

/*!
    \qmlattachedproperty int Qt.labs.controls::StackView::index
    \readonly

    This attached property holds the stack index of the item it's
    attached to, or \c -1 if the item is not in a stack.
*/
int QQuickStackAttached::index() const
{
    Q_D(const QQuickStackAttached);
    return d->element ? d->element->index : -1;
}

/*!
    \qmlattachedproperty StackView Qt.labs.controls::StackView::view
    \readonly

    This attached property holds the stack view of the item it's
    attached to, or \c null if the item is not in a stack.
*/
QQuickStackView *QQuickStackAttached::view() const
{
    Q_D(const QQuickStackAttached);
    return d->element ? d->element->view : Q_NULLPTR;
}

/*!
    \qmlattachedproperty enumeration Qt.labs.controls::StackView::status
    \readonly

    This attached property holds the stack status of the item it's
    attached to, or \c StackView.Inactive if the item is not in a stack.

    Available values:
    \value StackView.Inactive The item is inactive (or not in a stack).
    \value StackView.Deactivating The item is being deactivated (popped off).
    \value StackView.Activating The item is being activated (becoming the current item).
    \value StackView.Active The item is active, that is, the current item.
*/
QQuickStackView::Status QQuickStackAttached::status() const
{
    Q_D(const QQuickStackAttached);
    return d->element ? d->element->status : QQuickStackView::Inactive;
}

QT_END_NAMESPACE
