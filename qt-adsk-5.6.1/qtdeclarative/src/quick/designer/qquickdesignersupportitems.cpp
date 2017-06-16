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

#include "qquickdesignersupportitems_p.h"
#include "qquickdesignersupportproperties_p.h"

#include <private/qabstractanimation_p.h>
#include <private/qobject_p.h>
#include <private/qquickbehavior_p.h>
#include <private/qquicktext_p.h>
#include <private/qquicktextinput_p.h>
#include <private/qquicktextedit_p.h>
#include <private/qquicktransition_p.h>

#include <private/qquickanimation_p.h>
#include <private/qqmlmetatype_p.h>
#include <private/qqmltimer_p.h>

QT_BEGIN_NAMESPACE

static void (*fixResourcePathsForObjectCallBack)(QObject*) = 0;

static void stopAnimation(QObject *object)
{
    if (object == 0)
        return;

    QQuickTransition *transition = qobject_cast<QQuickTransition*>(object);
    QQuickAbstractAnimation *animation = qobject_cast<QQuickAbstractAnimation*>(object);
    QQmlTimer *timer = qobject_cast<QQmlTimer*>(object);
    if (transition) {
       transition->setFromState(QString());
       transition->setToState(QString());
    } else if (animation) {
//        QQuickScriptAction *scriptAimation = qobject_cast<QQuickScriptAction*>(animation);
//        if (scriptAimation) FIXME
//            scriptAimation->setScript(QQmlScriptString());
        animation->setLoops(1);
        animation->complete();
        animation->setDisableUserControl();
    } else if (timer) {
        timer->blockSignals(true);
    }
}

static void allSubObjects(QObject *object, QObjectList &objectList)
{
    // don't add null pointer and stop if the object is already in the list
    if (!object || objectList.contains(object))
        return;

    objectList.append(object);

    for (int index = QObject::staticMetaObject.propertyOffset();
         index < object->metaObject()->propertyCount();
         index++) {
        QMetaProperty metaProperty = object->metaObject()->property(index);

        // search recursive in property objects
        if (metaProperty.isReadable()
                && metaProperty.isWritable()
                && QQmlMetaType::isQObject(metaProperty.userType())) {
            if (qstrcmp(metaProperty.name(), "parent")) {
                QObject *propertyObject = QQmlMetaType::toQObject(metaProperty.read(object));
                allSubObjects(propertyObject, objectList);
            }

        }

        // search recursive in property object lists
        if (metaProperty.isReadable()
                && QQmlMetaType::isList(metaProperty.userType())) {
            QQmlListReference list(object, metaProperty.name());
            if (list.canCount() && list.canAt()) {
                for (int i = 0; i < list.count(); i++) {
                    QObject *propertyObject = list.at(i);
                    allSubObjects(propertyObject, objectList);

                }
            }
        }
    }

    // search recursive in object children list
    Q_FOREACH (QObject *childObject, object->children()) {
        allSubObjects(childObject, objectList);
    }

    // search recursive in quick item childItems list
    QQuickItem *quickItem = qobject_cast<QQuickItem*>(object);
    if (quickItem) {
        Q_FOREACH (QQuickItem *childItem, quickItem->childItems()) {
            allSubObjects(childItem, objectList);
        }
    }
}

void QQuickDesignerSupportItems::tweakObjects(QObject *object)
{
    QObjectList objectList;
    allSubObjects(object, objectList);
    Q_FOREACH (QObject* childObject, objectList) {
        stopAnimation(childObject);
        if (fixResourcePathsForObjectCallBack)
            fixResourcePathsForObjectCallBack(childObject);
    }
}

static QObject *createDummyWindow(QQmlEngine *engine)
{
    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/qtquickplugin/mockfiles/Window.qml")));
    return component.create();
}

static bool isWindowMetaObject(const QMetaObject *metaObject)
{
    if (metaObject) {
        if (metaObject->className() == QByteArrayLiteral("QWindow"))
            return true;

        return isWindowMetaObject(metaObject->superClass());
    }

    return false;
}

static bool isWindow(QObject *object) {
    if (object)
        return isWindowMetaObject(object->metaObject());

    return false;
}

static QQmlType *getQmlType(const QString &typeName, int majorNumber, int minorNumber)
{
     return QQmlMetaType::qmlType(typeName, majorNumber, minorNumber);
}

static bool isCrashingType(QQmlType *type)
{
    if (type) {
        if (type->qmlTypeName() == QStringLiteral("QtMultimedia/MediaPlayer"))
            return true;

        if (type->qmlTypeName() == QStringLiteral("QtMultimedia/Audio"))
            return true;

        if (type->qmlTypeName() == QStringLiteral("QtQuick.Controls/MenuItem"))
            return true;

        if (type->qmlTypeName() == QStringLiteral("QtQuick.Controls/Menu"))
            return true;

        if (type->qmlTypeName() == QStringLiteral("QtQuick/Timer"))
            return true;
    }

    return false;
}

QObject *QQuickDesignerSupportItems::createPrimitive(const QString &typeName, int majorNumber, int minorNumber, QQmlContext *context)
{
    ComponentCompleteDisabler disableComponentComplete;

    Q_UNUSED(disableComponentComplete)

    QObject *object = 0;
    QQmlType *type = getQmlType(typeName, majorNumber, minorNumber);

    if (isCrashingType(type)) {
        object = new QObject;
    } else if (type) {
        if ( type->isComposite()) {
             object = createComponent(type->sourceUrl(), context);
        } else
        {
            if (type->typeName() == "QQmlComponent") {
                object = new QQmlComponent(context->engine(), 0);
            } else  {
                object = type->create();
            }
        }

        if (isWindow(object)) {
            delete object;
            object = createDummyWindow(context->engine());
        }

    }

    if (!object) {
        qWarning() << "QuickDesigner: Cannot create an object of type"
                   << QString::fromLatin1("%1 %2,%3").arg(typeName).arg(majorNumber).arg(minorNumber)
                   << "- type isn't known to declarative meta type system";
    }

    tweakObjects(object);

    if (object && QQmlEngine::contextForObject(object) == 0)
        QQmlEngine::setContextForObject(object, context);

    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);

    return object;
}

QObject *QQuickDesignerSupportItems::createComponent(const QUrl &componentUrl, QQmlContext *context)
{
    ComponentCompleteDisabler disableComponentComplete;
    Q_UNUSED(disableComponentComplete)

    QQmlComponent component(context->engine(), componentUrl);

    QObject *object = component.beginCreate(context);
    tweakObjects(object);
    component.completeCreate();
    QQmlEngine::setObjectOwnership(object, QQmlEngine::CppOwnership);

    if (component.isError()) {
        qWarning() << "Error in:" << Q_FUNC_INFO << componentUrl;
        Q_FOREACH (const QQmlError &error, component.errors())
            qWarning() << error;
    }
    return object;
}

bool QQuickDesignerSupportItems::objectWasDeleted(QObject *object)
{
    return QObjectPrivate::get(object)->wasDeleted;
}

void QQuickDesignerSupportItems::disableNativeTextRendering(QQuickItem *item)
{
    QQuickText *text = qobject_cast<QQuickText*>(item);
    if (text)
        text->setRenderType(QQuickText::QtRendering);

    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(item);
    if (textInput)
        textInput->setRenderType(QQuickTextInput::QtRendering);

    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(item);
    if (textEdit)
        textEdit->setRenderType(QQuickTextEdit::QtRendering);
}

void QQuickDesignerSupportItems::disableTextCursor(QQuickItem *item)
{
    Q_FOREACH (QQuickItem *childItem, item->childItems())
        disableTextCursor(childItem);

    QQuickTextInput *textInput = qobject_cast<QQuickTextInput*>(item);
    if (textInput)
        textInput->setCursorVisible(false);

    QQuickTextEdit *textEdit = qobject_cast<QQuickTextEdit*>(item);
    if (textEdit)
        textEdit->setCursorVisible(false);
}

void QQuickDesignerSupportItems::disableTransition(QObject *object)
{
    QQuickTransition *transition = qobject_cast<QQuickTransition*>(object);
    Q_ASSERT(transition);
    const QString invalidState = QLatin1String("invalidState");
    transition->setToState(invalidState);
    transition->setFromState(invalidState);
}

void QQuickDesignerSupportItems::disableBehaivour(QObject *object)
{
    QQuickBehavior* behavior = qobject_cast<QQuickBehavior*>(object);
    Q_ASSERT(behavior);
    behavior->setEnabled(false);
}

void QQuickDesignerSupportItems::stopUnifiedTimer()
{
    QUnifiedTimer::instance()->setSlowdownFactor(0.00001);
    QUnifiedTimer::instance()->setSlowModeEnabled(true);
}

void QQuickDesignerSupportItems::registerFixResourcePathsForObjectCallBack(void (*callback)(QObject *))
{
    fixResourcePathsForObjectCallBack = callback;
}

QT_END_NAMESPACE


