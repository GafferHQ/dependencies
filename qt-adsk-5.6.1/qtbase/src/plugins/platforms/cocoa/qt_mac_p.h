/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QT_MAC_P_H
#define QT_MAC_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qmacdefines_mac.h"

#ifdef __OBJC__
#include <Cocoa/Cocoa.h>
#include <objc/runtime.h>
#endif

#include <CoreServices/CoreServices.h>

#include "QtCore/qglobal.h"
#include "QtCore/qvariant.h"
#include "QtCore/qmimedata.h"
#include "QtCore/qpointer.h"
#include "QtCore/qloggingcategory.h"
#include "private/qcore_mac_p.h"


#include "QtGui/qpainter.h"

#include <Carbon/Carbon.h>

QT_BEGIN_NAMESPACE
class QWidget;
class QDragMoveEvent;

/* Event masks */
// internal Qt types

enum {
    //AE types
    typeAEClipboardChanged = 1,
    //types
    typeQWidget = 1,  /* QWidget *  */
    //params
    kEventParamQWidget = 'qwid',   /* typeQWidget */
    //events
    kEventQtRequestContext = 13,
    kEventQtRequestMenubarUpdate = 14,
    kEventQtRequestShowSheet = 17,
    kEventQtRequestActivate = 18,
    kEventQtRequestWindowChange = 20
};

// Simple class to manage short-lived regions
class QMacSmartQuickDrawRegion
{
    RgnHandle qdRgn;
    Q_DISABLE_COPY(QMacSmartQuickDrawRegion)
public:
    explicit QMacSmartQuickDrawRegion(RgnHandle rgn) : qdRgn(rgn) {}
    ~QMacSmartQuickDrawRegion() {
        extern void qt_mac_dispose_rgn(RgnHandle); // qregion_mac.cpp
        qt_mac_dispose_rgn(qdRgn);
    }
    operator RgnHandle() {
        return qdRgn;
    }
};

QString qt_mac_removeMnemonics(const QString &original); //implemented in qmacstyle_mac.cpp

class Q_WIDGETS_EXPORT QMacWindowChangeEvent
{
private:
    static QList<QMacWindowChangeEvent*> *change_events;
public:
    QMacWindowChangeEvent() {
    }
    virtual ~QMacWindowChangeEvent() {
    }
    static inline void exec(bool ) {
    }
protected:
    virtual void windowChanged() = 0;
    virtual void flushWindowChanged() = 0;
};

class QMacCGContext
{
    CGContextRef context;
public:
    QMacCGContext(QPainter *p); //qpaintengine_mac.mm
    inline QMacCGContext() { context = 0; }
    inline QMacCGContext(const QPaintDevice *pdev) {
        extern CGContextRef qt_mac_cg_context(const QPaintDevice *);
        context = qt_mac_cg_context(pdev);
    }
    inline QMacCGContext(CGContextRef cg, bool takeOwnership=false) {
        context = cg;
        if(!takeOwnership)
            CGContextRetain(context);
    }
    inline QMacCGContext(const QMacCGContext &copy) : context(0) { *this = copy; }
    inline ~QMacCGContext() {
        if(context)
            CGContextRelease(context);
    }
    inline bool isNull() const { return context; }
    inline operator CGContextRef() { return context; }
    inline QMacCGContext &operator=(const QMacCGContext &copy) {
        if(context)
            CGContextRelease(context);
        context = copy.context;
        CGContextRetain(context);
        return *this;
    }
    inline QMacCGContext &operator=(CGContextRef cg) {
        if(context)
            CGContextRelease(context);
        context = cg;
        CGContextRetain(context); //we do not take ownership
        return *this;
    }
};

class QMacInternalPasteboardMime;
class QMimeData;


extern QPaintDevice *qt_mac_safe_pdev; //qapplication_mac.cpp

extern OSWindowRef qt_mac_window_for(const QWidget*); //qwidget_mac.mm
extern OSViewRef qt_mac_nativeview_for(const QWidget *); //qwidget_mac.mm
extern QPoint qt_mac_nativeMapFromParent(const QWidget *child, const QPoint &pt); //qwidget_mac.mm

#ifdef check
# undef check
#endif

QFont qfontForThemeFont(ThemeFontID themeID);

QColor qcolorForThemeTextColor(ThemeTextColor themeColor);

struct QMacDndAnswerRecord {
    QRect rect;
    Qt::KeyboardModifiers modifiers;
    Qt::MouseButtons buttons;
    Qt::DropAction lastAction;
    unsigned int lastOperation;
    void clear() {
        rect = QRect();
        modifiers = Qt::NoModifier;
        buttons = Qt::NoButton;
        lastAction = Qt::IgnoreAction;
        lastOperation = 0;
    }
};
extern QMacDndAnswerRecord qt_mac_dnd_answer_rec;
void qt_mac_copy_answer_rect(const QDragMoveEvent &event);
bool qt_mac_mouse_inside_answer_rect(QPoint mouse);

QT_END_NAMESPACE

#endif // QT_MAC_P_H
