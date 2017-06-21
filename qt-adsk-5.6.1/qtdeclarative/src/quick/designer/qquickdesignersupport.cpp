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

#include "qquickdesignersupport_p.h"
#include <private/qquickitem_p.h>

#include <QtQuick/private/qquickshadereffectsource_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQml/private/qabstractanimationjob_p.h>
#include <private/qqmlengine_p.h>
#include <private/qquickview_p.h>
#include <private/qsgrenderloop_p.h>
#include <QtQuick/private/qquickstategroup_p.h>
#include <QtGui/QImage>
#include <private/qqmlvme_p.h>
#include <private/qqmlcomponentattached_p.h>
#include <private/qqmldata_p.h>

#include "qquickdesignerwindowmanager_p.h"


QT_BEGIN_NAMESPACE

QQuickDesignerSupport::QQuickDesignerSupport()
{
}

QQuickDesignerSupport::~QQuickDesignerSupport()
{
    typedef QHash<QQuickItem*, QSGLayer*>::iterator ItemTextureHashIt;

    for (ItemTextureHashIt iterator = m_itemTextureHash.begin(), end = m_itemTextureHash.end(); iterator != end; ++iterator) {
        QSGLayer *texture = iterator.value();
        QQuickItem *item = iterator.key();
        QQuickItemPrivate::get(item)->derefFromEffectItem(true);
        delete texture;
    }
}

void QQuickDesignerSupport::refFromEffectItem(QQuickItem *referencedItem, bool hide)
{
    if (referencedItem == 0)
        return;

    QQuickItemPrivate::get(referencedItem)->refFromEffectItem(hide);
    QQuickWindowPrivate::get(referencedItem->window())->updateDirtyNode(referencedItem);

    Q_ASSERT(QQuickItemPrivate::get(referencedItem)->rootNode());

    if (!m_itemTextureHash.contains(referencedItem)) {
        QSGRenderContext *rc = QQuickWindowPrivate::get(referencedItem->window())->context;
        QSGLayer *texture = rc->sceneGraphContext()->createLayer(rc);

        texture->setLive(true);
        texture->setItem(QQuickItemPrivate::get(referencedItem)->rootNode());
        texture->setRect(referencedItem->boundingRect());
        texture->setSize(referencedItem->boundingRect().size().toSize());
        texture->setRecursive(true);
#ifndef QT_OPENGL_ES
        if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL)
            texture->setFormat(GL_RGBA8);
        else
            texture->setFormat(GL_RGBA);
#else
        texture->setFormat(GL_RGBA);
#endif
        texture->setHasMipmaps(false);

        m_itemTextureHash.insert(referencedItem, texture);
    }
}

void QQuickDesignerSupport::derefFromEffectItem(QQuickItem *referencedItem, bool unhide)
{
    if (referencedItem == 0)
        return;

    delete m_itemTextureHash.take(referencedItem);
    QQuickItemPrivate::get(referencedItem)->derefFromEffectItem(unhide);
}

QImage QQuickDesignerSupport::renderImageForItem(QQuickItem *referencedItem, const QRectF &boundingRect, const QSize &imageSize)
{
    if (referencedItem == 0 || referencedItem->parentItem() == 0) {
        qDebug() << __FILE__ << __LINE__ << "Warning: Item can be rendered.";
        return QImage();
    }

    QSGLayer *renderTexture = m_itemTextureHash.value(referencedItem);

    Q_ASSERT(renderTexture);
    if (renderTexture == 0)
         return QImage();
    renderTexture->setRect(boundingRect);
    renderTexture->setSize(imageSize);
    renderTexture->setItem(QQuickItemPrivate::get(referencedItem)->rootNode());
    renderTexture->markDirtyTexture();
    renderTexture->updateTexture();

    QImage renderImage = renderTexture->toImage();
    renderImage = renderImage.mirrored(false, true);

    if (renderImage.size().isEmpty())
        qDebug() << __FILE__ << __LINE__ << "Warning: Image is empty.";

    return renderImage;
}

bool QQuickDesignerSupport::isDirty(QQuickItem *referencedItem, DirtyType dirtyType)
{
    if (referencedItem == 0)
        return false;

    return QQuickItemPrivate::get(referencedItem)->dirtyAttributes & dirtyType;
}

void QQuickDesignerSupport::addDirty(QQuickItem *referencedItem, QQuickDesignerSupport::DirtyType dirtyType)
{
    if (referencedItem == 0)
        return;

    QQuickItemPrivate::get(referencedItem)->dirtyAttributes |= dirtyType;
}

void QQuickDesignerSupport::resetDirty(QQuickItem *referencedItem)
{
    if (referencedItem == 0)
        return;

    QQuickItemPrivate::get(referencedItem)->dirtyAttributes = 0x0;
    QQuickItemPrivate::get(referencedItem)->removeFromDirtyList();
}

QTransform QQuickDesignerSupport::windowTransform(QQuickItem *referencedItem)
{
    if (referencedItem == 0)
        return QTransform();

    return QQuickItemPrivate::get(referencedItem)->itemToWindowTransform();
}

QTransform QQuickDesignerSupport::parentTransform(QQuickItem *referencedItem)
{
    if (referencedItem == 0)
        return QTransform();

    QTransform parentTransform;

    QQuickItemPrivate::get(referencedItem)->itemToParentTransform(parentTransform);

    return parentTransform;
}

QString propertyNameForAnchorLine(const QQuickAnchorLine::AnchorLine &anchorLine)
{
    switch (anchorLine) {
        case QQuickAnchorLine::Left: return QLatin1String("left");
        case QQuickAnchorLine::Right: return QLatin1String("right");
        case QQuickAnchorLine::Top: return QLatin1String("top");
        case QQuickAnchorLine::Bottom: return QLatin1String("bottom");
        case QQuickAnchorLine::HCenter: return QLatin1String("horizontalCenter");
        case QQuickAnchorLine::VCenter: return QLatin1String("verticalCenter");
        case QQuickAnchorLine::Baseline: return QLatin1String("baseline");
        case QQuickAnchorLine::Invalid:
        default: return QString();
    }
}

bool isValidAnchorName(const QString &name)
{
    static QStringList anchorNameList(QStringList() << QLatin1String("anchors.top")
                                                    << QLatin1String("anchors.left")
                                                    << QLatin1String("anchors.right")
                                                    << QLatin1String("anchors.bottom")
                                                    << QLatin1String("anchors.verticalCenter")
                                                    << QLatin1String("anchors.horizontalCenter")
                                                    << QLatin1String("anchors.fill")
                                                    << QLatin1String("anchors.centerIn")
                                                    << QLatin1String("anchors.baseline"));

    return anchorNameList.contains(name);
}

bool QQuickDesignerSupport::isAnchoredTo(QQuickItem *fromItem, QQuickItem *toItem)
{
    QQuickItemPrivate *fromItemPrivate = QQuickItemPrivate::get(fromItem);
    QQuickAnchors *anchors = fromItemPrivate->anchors();
    return anchors->fill() == toItem
            || anchors->centerIn() == toItem
            || anchors->bottom().item == toItem
            || anchors->top().item == toItem
            || anchors->left().item == toItem
            || anchors->right().item == toItem
            || anchors->verticalCenter().item == toItem
            || anchors->horizontalCenter().item == toItem
            || anchors->baseline().item == toItem;
}

bool QQuickDesignerSupport::areChildrenAnchoredTo(QQuickItem *fromItem, QQuickItem *toItem)
{
    Q_FOREACH (QQuickItem *childItem, fromItem->childItems()) {
        if (childItem) {
            if (isAnchoredTo(childItem, toItem))
                return true;

            if (areChildrenAnchoredTo(childItem, toItem))
                return true;
        }
    }

    return false;
}

QQuickAnchors *anchors(QQuickItem *item)
{
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
    return itemPrivate->anchors();
}

QQuickAnchors::Anchor anchorLineFlagForName(const QString &name)
{
    if (name == QLatin1String("anchors.top"))
        return QQuickAnchors::TopAnchor;

    if (name == QLatin1String("anchors.left"))
        return QQuickAnchors::LeftAnchor;

    if (name == QLatin1String("anchors.bottom"))
         return QQuickAnchors::BottomAnchor;

    if (name == QLatin1String("anchors.right"))
        return QQuickAnchors::RightAnchor;

    if (name == QLatin1String("anchors.horizontalCenter"))
        return QQuickAnchors::HCenterAnchor;

    if (name == QLatin1String("anchors.verticalCenter"))
         return QQuickAnchors::VCenterAnchor;

    if (name == QLatin1String("anchors.baseline"))
         return QQuickAnchors::BaselineAnchor;


    Q_ASSERT_X(false, Q_FUNC_INFO, "wrong anchor name - this should never happen");
    return QQuickAnchors::LeftAnchor;
}

bool QQuickDesignerSupport::hasAnchor(QQuickItem *item, const QString &name)
{
    if (!isValidAnchorName(name))
        return false;

    if (name == QLatin1String("anchors.fill"))
        return anchors(item)->fill() != 0;

    if (name == QLatin1String("anchors.centerIn"))
        return anchors(item)->centerIn() != 0;

    if (name == QLatin1String("anchors.right"))
        return anchors(item)->right().item != 0;

    if (name == QLatin1String("anchors.top"))
        return anchors(item)->top().item != 0;

    if (name == QLatin1String("anchors.left"))
        return anchors(item)->left().item != 0;

    if (name == QLatin1String("anchors.bottom"))
        return anchors(item)->bottom().item != 0;

    if (name == QLatin1String("anchors.horizontalCenter"))
        return anchors(item)->horizontalCenter().item != 0;

    if (name == QLatin1String("anchors.verticalCenter"))
        return anchors(item)->verticalCenter().item != 0;

    if (name == QLatin1String("anchors.baseline"))
        return anchors(item)->baseline().item != 0;

    return anchors(item)->usedAnchors().testFlag(anchorLineFlagForName(name));
}

QQuickItem *QQuickDesignerSupport::anchorFillTargetItem(QQuickItem *item)
{
    return anchors(item)->fill();
}

QQuickItem *QQuickDesignerSupport::anchorCenterInTargetItem(QQuickItem *item)
{
    return anchors(item)->centerIn();
}



QPair<QString, QObject*> QQuickDesignerSupport::anchorLineTarget(QQuickItem *item, const QString &name, QQmlContext *context)
{
    QObject *targetObject = 0;
    QString targetName;

    if (name == QLatin1String("anchors.fill")) {
        targetObject = anchors(item)->fill();
    } else if (name == QLatin1String("anchors.centerIn")) {
        targetObject = anchors(item)->centerIn();
    } else {
        QQmlProperty metaProperty(item, name, context);
        if (!metaProperty.isValid())
            return QPair<QString, QObject*>();

        QQuickAnchorLine anchorLine = metaProperty.read().value<QQuickAnchorLine>();
        if (anchorLine.anchorLine != QQuickAnchorLine::Invalid) {
            targetObject = anchorLine.item;
            targetName = propertyNameForAnchorLine(anchorLine.anchorLine);
        }

    }

    return QPair<QString, QObject*>(targetName, targetObject);
}

void QQuickDesignerSupport::resetAnchor(QQuickItem *item, const QString &name)
{
    if (name == QLatin1String("anchors.fill")) {
        anchors(item)->resetFill();
    } else if (name == QLatin1String("anchors.centerIn")) {
        anchors(item)->resetCenterIn();
    } else if (name == QLatin1String("anchors.top")) {
        anchors(item)->resetTop();
    } else if (name == QLatin1String("anchors.left")) {
        anchors(item)->resetLeft();
    } else if (name == QLatin1String("anchors.right")) {
        anchors(item)->resetRight();
    } else if (name == QLatin1String("anchors.bottom")) {
        anchors(item)->resetBottom();
    } else if (name == QLatin1String("anchors.horizontalCenter")) {
        anchors(item)->resetHorizontalCenter();
    } else if (name == QLatin1String("anchors.verticalCenter")) {
        anchors(item)->resetVerticalCenter();
    } else if (name == QLatin1String("anchors.baseline")) {
        anchors(item)->resetBaseline();
    }
}

void QQuickDesignerSupport::emitComponentCompleteSignalForAttachedProperty(QQuickItem *item)
{
    QQmlData *data = QQmlData::get(item);
    if (data && data->context) {
        QQmlComponentAttached *componentAttached = data->context->componentAttached;
        if (componentAttached) {
            emit componentAttached->completed();
        }
    }
}

QList<QObject*> QQuickDesignerSupport::statesForItem(QQuickItem *item)
{
    QList<QObject*> objectList;
    QList<QQuickState *> stateList = QQuickItemPrivate::get(item)->_states()->states();

    objectList.reserve(stateList.size());
    Q_FOREACH (QQuickState* state, stateList)
        objectList.append(state);

    return objectList;
}

bool QQuickDesignerSupport::isComponentComplete(QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->componentComplete;
}

int QQuickDesignerSupport::borderWidth(QQuickItem *item)
{
    QQuickRectangle *rectangle = qobject_cast<QQuickRectangle*>(item);
    if (rectangle)
        return rectangle->border()->width();

    return 0;
}

void QQuickDesignerSupport::refreshExpressions(QQmlContext *context)
{
    QQmlContextPrivate::get(context)->data->refreshExpressions();
}

void QQuickDesignerSupport::setRootItem(QQuickView *view, QQuickItem *item)
{
    QQuickViewPrivate::get(view)->setRootObject(item);
}

bool QQuickDesignerSupport::isValidWidth(QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->heightValid;
}

bool QQuickDesignerSupport::isValidHeight(QQuickItem *item)
{
    return QQuickItemPrivate::get(item)->widthValid;
}

void QQuickDesignerSupport::updateDirtyNode(QQuickItem *item)
{
    if (item->window())
        QQuickWindowPrivate::get(item->window())->updateDirtyNode(item);
}

void QQuickDesignerSupport::activateDesignerWindowManager()
{
    QSGRenderLoop::setInstance(new QQuickDesignerWindowManager);
}

void QQuickDesignerSupport::activateDesignerMode()
{
    QQmlEnginePrivate::activateDesignerMode();
}

void QQuickDesignerSupport::disableComponentComplete()
{
    QQmlVME::disableComponentComplete();
}

void QQuickDesignerSupport::enableComponentComplete()
{
    QQmlVME::enableComponentComplete();
}

void QQuickDesignerSupport::createOpenGLContext(QQuickWindow *window)
{
    QQuickDesignerWindowManager::createOpenGLContext(window);
}

void QQuickDesignerSupport::polishItems(QQuickWindow *window)
{
    QQuickWindowPrivate::get(window)->polishItems();
}

ComponentCompleteDisabler::ComponentCompleteDisabler()
{
    QQuickDesignerSupport::disableComponentComplete();
}

ComponentCompleteDisabler::~ComponentCompleteDisabler()
{
    QQuickDesignerSupport::enableComponentComplete();
}


QT_END_NAMESPACE
