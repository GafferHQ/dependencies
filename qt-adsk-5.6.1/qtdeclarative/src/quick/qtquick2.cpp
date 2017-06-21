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

#include "qtquick2_p.h"
#include <private/qqmlengine_p.h>
#include <private/qquickutilmodule_p.h>
#include <private/qquickvaluetypes_p.h>
#include <private/qquickitemsmodule_p.h>
#include <private/qquickaccessiblefactory_p.h>

#include <private/qqmldebugconnector_p.h>
#include <private/qqmldebugserviceinterfaces_p.h>
#include <private/qqmldebugstatesdelegate_p.h>
#include <private/qqmlbinding_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qquickprofiler_p.h>
#include <private/qquickapplication_p.h>
#include <QtQuick/private/qquickpropertychanges_p.h>
#include <QtQuick/private/qquickstate_p.h>
#include <qqmlproperty.h>
#include <QtCore/QPointer>

static void initResources()
{
    Q_INIT_RESOURCE(scenegraph);
}

QT_BEGIN_NAMESPACE

class QQmlQtQuick2DebugStatesDelegate : public QQmlDebugStatesDelegate
{
public:
    QQmlQtQuick2DebugStatesDelegate();
    virtual ~QQmlQtQuick2DebugStatesDelegate();
    virtual void buildStatesList(bool cleanList, const QList<QPointer<QObject> > &instances);
    virtual void updateBinding(QQmlContext *context,
                               const QQmlProperty &property,
                               const QVariant &expression, bool isLiteralValue,
                               const QString &fileName, int line, int column,
                               bool *isBaseState);
    virtual bool setBindingForInvalidProperty(QObject *object,
                                              const QString &propertyName,
                                              const QVariant &expression,
                                              bool isLiteralValue);
    virtual void resetBindingForInvalidProperty(QObject *object,
                                                const QString &propertyName);

private:
    void buildStatesList(QObject *obj);

    QList<QPointer<QQuickState> > m_allStates;
};

QQmlQtQuick2DebugStatesDelegate::QQmlQtQuick2DebugStatesDelegate()
{
}

QQmlQtQuick2DebugStatesDelegate::~QQmlQtQuick2DebugStatesDelegate()
{
}

void QQmlQtQuick2DebugStatesDelegate::buildStatesList(bool cleanList,
                                                      const QList<QPointer<QObject> > &instances)
{
    if (cleanList)
        m_allStates.clear();

    //only root context has all instances
    for (int ii = 0; ii < instances.count(); ++ii) {
        buildStatesList(instances.at(ii));
    }
}

void QQmlQtQuick2DebugStatesDelegate::buildStatesList(QObject *obj)
{
    if (QQuickState *state = qobject_cast<QQuickState *>(obj)) {
        m_allStates.append(state);
    }

    QObjectList children = obj->children();
    for (int ii = 0; ii < children.count(); ++ii) {
        buildStatesList(children.at(ii));
    }
}

void QQmlQtQuick2DebugStatesDelegate::updateBinding(QQmlContext *context,
                                                            const QQmlProperty &property,
                                                            const QVariant &expression, bool isLiteralValue,
                                                            const QString &fileName, int line, int column,
                                                            bool *inBaseState)
{
    typedef QPointer<QQuickState> QuickStatePointer;
    QObject *object = property.object();
    QString propertyName = property.name();
    foreach (const QuickStatePointer& statePointer, m_allStates) {
        if (QQuickState *state = statePointer.data()) {
            // here we assume that the revert list on itself defines the base state
            if (state->isStateActive() && state->containsPropertyInRevertList(object, propertyName)) {
                *inBaseState = false;

                QQmlBinding *newBinding = 0;
                if (!isLiteralValue) {
                    newBinding = new QQmlBinding(expression.toString(), object,
                                                 QQmlContextData::get(context), fileName,
                                                 line, column);
                    newBinding->setTarget(property);
                }

                state->changeBindingInRevertList(object, propertyName, newBinding);

                if (isLiteralValue)
                    state->changeValueInRevertList(object, propertyName, expression);
            }
        }
    }
}

bool QQmlQtQuick2DebugStatesDelegate::setBindingForInvalidProperty(QObject *object,
                                                                           const QString &propertyName,
                                                                           const QVariant &expression,
                                                                           bool isLiteralValue)
{
    if (QQuickPropertyChanges *propertyChanges = qobject_cast<QQuickPropertyChanges *>(object)) {
        if (isLiteralValue)
            propertyChanges->changeValue(propertyName, expression);
        else
            propertyChanges->changeExpression(propertyName, expression.toString());
        return true;
    } else {
        return false;
    }
}

void QQmlQtQuick2DebugStatesDelegate::resetBindingForInvalidProperty(QObject *object, const QString &propertyName)
{
    if (QQuickPropertyChanges *propertyChanges = qobject_cast<QQuickPropertyChanges *>(object)) {
        propertyChanges->removeProperty(propertyName);
    }
}


void QQmlQtQuick2Module::defineModule()
{
    initResources();

    QQuick_initializeProviders();

    QQuickUtilModule::defineModule();
    QQmlEnginePrivate::defineQtQuick2Module();
    QQuickItemsModule::defineModule();

    qmlRegisterUncreatableType<QQuickApplication>("QtQuick",2,0,"Application", QQuickApplication::tr("Application is an abstract class"));

    QQuickValueTypes::registerValueTypes();

#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installFactory(&qQuickAccessibleFactory);
#endif

    QQmlEngineDebugService *debugService = QQmlDebugConnector::service<QQmlEngineDebugService>();
    if (debugService)
        debugService->setStatesDelegate(new QQmlQtQuick2DebugStatesDelegate);

    QQmlProfilerService *profilerService = QQmlDebugConnector::service<QQmlProfilerService>();
    if (profilerService)
        QQuickProfiler::initialize(profilerService);
}

void QQmlQtQuick2Module::undefineModule()
{
    QQuick_deinitializeProviders();
}

QT_END_NAMESPACE

