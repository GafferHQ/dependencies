/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
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

#include "private/qdeclarativeitemsmodule_p.h"

#include <QtGui/qaction.h>
#include <QtGui/qvalidator.h>
#include <QtGui/qgraphicseffect.h>

#include "private/qdeclarativeevents_p_p.h"
#include "private/qdeclarativescalegrid_p_p.h"
#include "private/qdeclarativeanimatedimage_p.h"
#include "private/qdeclarativeborderimage_p.h"
#include "private/qdeclarativepositioners_p.h"
#include "private/qdeclarativemousearea_p.h"
#include "private/qdeclarativeflickable_p.h"
#include "private/qdeclarativeflickable_p_p.h"
#include "private/qdeclarativeflipable_p.h"
#include "private/qdeclarativefocuspanel_p.h"
#include "private/qdeclarativefocusscope_p.h"
#include "private/qdeclarativegridview_p.h"
#include "private/qdeclarativeimage_p.h"
#include "private/qdeclarativeitem_p.h"
#include "private/qdeclarativelayoutitem_p.h"
#include "private/qdeclarativelistview_p.h"
#include "private/qdeclarativeloader_p.h"
#include "private/qdeclarativemousearea_p.h"
#include "private/qdeclarativepath_p.h"
#include "private/qdeclarativepathview_p.h"
#include "private/qdeclarativerectangle_p.h"
#include "private/qdeclarativerepeater_p.h"
#include "private/qdeclarativetranslate_p.h"
#include "private/qdeclarativetext_p.h"
#include "private/qdeclarativetextedit_p.h"
#include "private/qdeclarativetextinput_p.h"
#include "private/qdeclarativevisualitemmodel_p.h"
#include "private/qdeclarativegraphicswidget_p.h"
#ifdef QT_WEBKIT_LIB
#include "private/qdeclarativewebview_p.h"
#include "private/qdeclarativewebview_p_p.h"
#endif
#include "private/qdeclarativeanchors_p.h"
#include "private/qdeclarativepincharea_p.h"

static QDeclarativePrivate::AutoParentResult qgraphicsobject_autoParent(QObject *obj, QObject *parent)
{
    QGraphicsObject* gobj = qobject_cast<QGraphicsObject*>(obj);
    if (!gobj)
        return QDeclarativePrivate::IncompatibleObject;

    QGraphicsObject* gparent = qobject_cast<QGraphicsObject*>(parent);
    if (!gparent)
        return QDeclarativePrivate::IncompatibleParent;

    gobj->setParentItem(gparent);
    return QDeclarativePrivate::Parented;
}

void QDeclarativeItemModule::defineModule()
{
    if (QApplication::type() == QApplication::Tty)
        return;

    QDeclarativePrivate::RegisterAutoParent autoparent = { 0, &qgraphicsobject_autoParent };
    QDeclarativePrivate::qmlregister(QDeclarativePrivate::AutoParentRegistration, &autoparent);
#ifdef QT_NO_MOVIE
    qmlRegisterTypeNotAvailable("QtQuick",1,0,"AnimatedImage",
        qApp->translate("QDeclarativeAnimatedImage","Qt was built without support for QMovie"));
#else
    qmlRegisterType<QDeclarativeAnimatedImage>("QtQuick",1,0,"AnimatedImage");
#endif
    qmlRegisterType<QDeclarativeBorderImage>("QtQuick",1,0,"BorderImage");
    qmlRegisterType<QDeclarativeColumn>("QtQuick",1,0,"Column");
    qmlRegisterType<QDeclarativeDrag>("QtQuick",1,0,"Drag");
    qmlRegisterType<QDeclarativeFlickable>("QtQuick",1,0,"Flickable");
    qmlRegisterType<QDeclarativeFlipable>("QtQuick",1,0,"Flipable");
    qmlRegisterType<QDeclarativeFlow>("QtQuick",1,0,"Flow");
    qmlRegisterType<QDeclarativeFocusPanel>("QtQuick",1,0,"FocusPanel");
    qmlRegisterType<QDeclarativeFocusScope>("QtQuick",1,0,"FocusScope");
    qmlRegisterType<QDeclarativeGradient>("QtQuick",1,0,"Gradient");
    qmlRegisterType<QDeclarativeGradientStop>("QtQuick",1,0,"GradientStop");
    qmlRegisterType<QDeclarativeGrid>("QtQuick",1,0,"Grid");
    qmlRegisterType<QDeclarativeGridView>("QtQuick",1,0,"GridView");
    qmlRegisterType<QDeclarativeImage>("QtQuick",1,0,"Image");
    qmlRegisterType<QDeclarativeItem>("QtQuick",1,0,"Item");
    qmlRegisterType<QDeclarativeLayoutItem>("QtQuick",1,0,"LayoutItem");
    qmlRegisterType<QDeclarativeListView>("QtQuick",1,0,"ListView");
    qmlRegisterType<QDeclarativeLoader>("QtQuick",1,0,"Loader");
    qmlRegisterType<QDeclarativeMouseArea>("QtQuick",1,0,"MouseArea");
    qmlRegisterType<QDeclarativePath>("QtQuick",1,0,"Path");
    qmlRegisterType<QDeclarativePathAttribute>("QtQuick",1,0,"PathAttribute");
    qmlRegisterType<QDeclarativePathCubic>("QtQuick",1,0,"PathCubic");
    qmlRegisterType<QDeclarativePathLine>("QtQuick",1,0,"PathLine");
    qmlRegisterType<QDeclarativePathPercent>("QtQuick",1,0,"PathPercent");
    qmlRegisterType<QDeclarativePathQuad>("QtQuick",1,0,"PathQuad");
    qmlRegisterType<QDeclarativePathView>("QtQuick",1,0,"PathView");
#ifndef QT_NO_VALIDATOR
    qmlRegisterType<QIntValidator>("QtQuick",1,0,"IntValidator");
    qmlRegisterType<QDoubleValidator>("QtQuick",1,0,"DoubleValidator");
    qmlRegisterType<QRegExpValidator>("QtQuick",1,0,"RegExpValidator");
#endif
    qmlRegisterType<QDeclarativeRectangle>("QtQuick",1,0,"Rectangle");
    qmlRegisterType<QDeclarativeRepeater>("QtQuick",1,0,"Repeater");
    qmlRegisterType<QGraphicsRotation>("QtQuick",1,0,"Rotation");
    qmlRegisterType<QDeclarativeRow>("QtQuick",1,0,"Row");
    qmlRegisterType<QDeclarativeTranslate>("QtQuick",1,0,"Translate");
    qmlRegisterType<QGraphicsScale>("QtQuick",1,0,"Scale");
    qmlRegisterType<QDeclarativeText>("QtQuick",1,0,"Text");
    qmlRegisterType<QDeclarativeTextEdit>("QtQuick",1,0,"TextEdit");
#ifndef QT_NO_LINEEDIT
    qmlRegisterType<QDeclarativeTextInput>("QtQuick",1,0,"TextInput");
#endif
    qmlRegisterType<QDeclarativeViewSection>("QtQuick",1,0,"ViewSection");
    qmlRegisterType<QDeclarativeVisualDataModel>("QtQuick",1,0,"VisualDataModel");
    qmlRegisterType<QDeclarativeVisualItemModel>("QtQuick",1,0,"VisualItemModel");

    qmlRegisterType<QDeclarativeAnchors>();
    qmlRegisterType<QDeclarativeKeyEvent>();
    qmlRegisterType<QDeclarativeMouseEvent>();
    qmlRegisterType<QGraphicsObject>();
    qmlRegisterType<QGraphicsWidget>("QtQuick",1,0,"QGraphicsWidget");
    qmlRegisterExtendedType<QGraphicsWidget,QDeclarativeGraphicsWidget>("QtQuick",1,0,"QGraphicsWidget");
    qmlRegisterType<QGraphicsTransform>();
    qmlRegisterType<QDeclarativePathElement>();
    qmlRegisterType<QDeclarativeCurve>();
    qmlRegisterType<QDeclarativeScaleGrid>();
#ifndef QT_NO_VALIDATOR
    qmlRegisterType<QValidator>();
#endif
    qmlRegisterType<QDeclarativeVisualModel>();
#ifndef QT_NO_ACTION
    qmlRegisterType<QAction>();
#endif
    qmlRegisterType<QDeclarativePen>();
    qmlRegisterType<QDeclarativeFlickableVisibleArea>();
#ifndef QT_NO_GRAPHICSEFFECT
    qmlRegisterType<QGraphicsEffect>();
#endif

    qmlRegisterUncreatableType<QDeclarativeKeyNavigationAttached>("QtQuick",1,0,"KeyNavigation",QDeclarativeKeyNavigationAttached::tr("KeyNavigation is only available via attached properties"));
    qmlRegisterUncreatableType<QDeclarativeKeysAttached>("QtQuick",1,0,"Keys",QDeclarativeKeysAttached::tr("Keys is only available via attached properties"));

    // QtQuick 1.1 items
    qmlRegisterType<QDeclarativePinchArea>("QtQuick",1,1,"PinchArea");
    qmlRegisterType<QDeclarativePinch>("QtQuick",1,1,"Pinch");
    qmlRegisterType<QDeclarativePinchEvent>();
    qmlRegisterType<QDeclarativeItem,1>("QtQuick",1,1,"Item");
    qmlRegisterType<QDeclarativeMouseArea,1>("QtQuick",1,1,"MouseArea");
    qmlRegisterType<QDeclarativeFlickable,1>("QtQuick",1,1,"Flickable");
    qmlRegisterType<QDeclarativeListView,1>("QtQuick",1,1,"ListView");
    qmlRegisterType<QDeclarativeGridView,1>("QtQuick",1,1,"GridView");
    qmlRegisterType<QDeclarativeRow,1>("QtQuick",1,1,"Row");
    qmlRegisterType<QDeclarativeGrid,1>("QtQuick",1,1,"Grid");
    qmlRegisterType<QDeclarativeFlow,1>("QtQuick",1,1,"Flow");
    qmlRegisterType<QDeclarativeRepeater,1>("QtQuick",1,1,"Repeater");
    qmlRegisterType<QDeclarativeText,1>("QtQuick",1,1,"Text");
    qmlRegisterType<QDeclarativeTextEdit,1>("QtQuick",1,1,"TextEdit");
#ifndef QT_NO_LINEEDIT
    qmlRegisterType<QDeclarativeTextInput,1>("QtQuick",1,1,"TextInput");
#endif
    qmlRegisterRevision<QDeclarativeImageBase,1>("QtQuick",1,1);
    qmlRegisterRevision<QDeclarativeImplicitSizeItem,0>("QtQuick",1,0);
    qmlRegisterRevision<QDeclarativeImplicitSizeItem,1>("QtQuick",1,1);
    qmlRegisterRevision<QDeclarativeImplicitSizePaintedItem,0>("QtQuick",1,0);
    qmlRegisterRevision<QDeclarativeImplicitSizePaintedItem,1>("QtQuick",1,1);
    qmlRegisterUncreatableType<QDeclarativeLayoutMirroringAttached>("QtQuick",1,1,"LayoutMirroring", QDeclarativeLayoutMirroringAttached::tr("LayoutMirroring is only available via attached properties"));
}

void QDeclarativeItemModule::defineModuleCompat()
{
    if (QApplication::type() == QApplication::Tty)
        return;

#ifdef QT_NO_MOVIE
    qmlRegisterTypeNotAvailable("Qt",4,7,"AnimatedImage",
        qApp->translate("QDeclarativeAnimatedImage","Qt was built without support for QMovie"));
#else
    qmlRegisterType<QDeclarativeAnimatedImage>("Qt",4,7,"AnimatedImage");
#endif
    qmlRegisterType<QDeclarativeBorderImage>("Qt",4,7,"BorderImage");
    qmlRegisterType<QDeclarativeColumn>("Qt",4,7,"Column");
    qmlRegisterType<QDeclarativeDrag>("Qt",4,7,"Drag");
    qmlRegisterType<QDeclarativeFlickable>("Qt",4,7,"Flickable");
    qmlRegisterType<QDeclarativeFlipable>("Qt",4,7,"Flipable");
    qmlRegisterType<QDeclarativeFlow>("Qt",4,7,"Flow");
    qmlRegisterType<QDeclarativeFocusPanel>("Qt",4,7,"FocusPanel");
    qmlRegisterType<QDeclarativeFocusScope>("Qt",4,7,"FocusScope");
    qmlRegisterType<QDeclarativeGradient>("Qt",4,7,"Gradient");
    qmlRegisterType<QDeclarativeGradientStop>("Qt",4,7,"GradientStop");
    qmlRegisterType<QDeclarativeGrid>("Qt",4,7,"Grid");
    qmlRegisterType<QDeclarativeGridView>("Qt",4,7,"GridView");
    qmlRegisterType<QDeclarativeImage>("Qt",4,7,"Image");
    qmlRegisterType<QDeclarativeItem>("Qt",4,7,"Item");
    qmlRegisterType<QDeclarativeLayoutItem>("Qt",4,7,"LayoutItem");
    qmlRegisterType<QDeclarativeListView>("Qt",4,7,"ListView");
    qmlRegisterType<QDeclarativeLoader>("Qt",4,7,"Loader");
    qmlRegisterType<QDeclarativeMouseArea>("Qt",4,7,"MouseArea");
    qmlRegisterType<QDeclarativePath>("Qt",4,7,"Path");
    qmlRegisterType<QDeclarativePathAttribute>("Qt",4,7,"PathAttribute");
    qmlRegisterType<QDeclarativePathCubic>("Qt",4,7,"PathCubic");
    qmlRegisterType<QDeclarativePathLine>("Qt",4,7,"PathLine");
    qmlRegisterType<QDeclarativePathPercent>("Qt",4,7,"PathPercent");
    qmlRegisterType<QDeclarativePathQuad>("Qt",4,7,"PathQuad");
    qmlRegisterType<QDeclarativePathView>("Qt",4,7,"PathView");
#ifndef QT_NO_VALIDATOR
    qmlRegisterType<QIntValidator>("Qt",4,7,"IntValidator");
    qmlRegisterType<QDoubleValidator>("Qt",4,7,"DoubleValidator");
    qmlRegisterType<QRegExpValidator>("Qt",4,7,"RegExpValidator");
#endif
    qmlRegisterType<QDeclarativeRectangle>("Qt",4,7,"Rectangle");
    qmlRegisterType<QDeclarativeRepeater>("Qt",4,7,"Repeater");
    qmlRegisterType<QGraphicsRotation>("Qt",4,7,"Rotation");
    qmlRegisterType<QDeclarativeRow>("Qt",4,7,"Row");
    qmlRegisterType<QDeclarativeTranslate>("Qt",4,7,"Translate");
    qmlRegisterType<QGraphicsScale>("Qt",4,7,"Scale");
    qmlRegisterType<QDeclarativeText>("Qt",4,7,"Text");
    qmlRegisterType<QDeclarativeTextEdit>("Qt",4,7,"TextEdit");
#ifndef QT_NO_LINEEDIT
    qmlRegisterType<QDeclarativeTextInput>("Qt",4,7,"TextInput");
#endif
    qmlRegisterType<QDeclarativeViewSection>("Qt",4,7,"ViewSection");
    qmlRegisterType<QDeclarativeVisualDataModel>("Qt",4,7,"VisualDataModel");
    qmlRegisterType<QDeclarativeVisualItemModel>("Qt",4,7,"VisualItemModel");

    qmlRegisterType<QGraphicsWidget>("Qt",4,7,"QGraphicsWidget");
    qmlRegisterExtendedType<QGraphicsWidget,QDeclarativeGraphicsWidget>("Qt",4,7,"QGraphicsWidget");

    qmlRegisterUncreatableType<QDeclarativeKeyNavigationAttached>("Qt",4,7,"KeyNavigation",QDeclarativeKeyNavigationAttached::tr("KeyNavigation is only available via attached properties"));
    qmlRegisterUncreatableType<QDeclarativeKeysAttached>("Qt",4,7,"Keys",QDeclarativeKeysAttached::tr("Keys is only available via attached properties"));
}
