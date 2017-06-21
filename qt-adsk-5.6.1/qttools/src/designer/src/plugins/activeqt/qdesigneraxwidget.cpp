/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "qdesigneraxwidget.h"

#include <QtCore/QMetaProperty>
#include <QtCore/QDebug>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>

#include <ActiveQt/QAxWidget>

#include <qt_windows.h>
#include <olectl.h>
#include <qaxtypes.h>

enum { debugAxWidget = 0 };

QT_BEGIN_NAMESPACE

/* XPM */
const char *widgetIconXPM[]={
"22 22 6 1",
"a c #000000",
"# c #808080",
"+ c #aaa5a0",
"b c #dddddd",
"* c #d4d0c8",
". c none",
".........#aa#...#aa#..",
".........abba...abba..",
".........abba...abba..",
".........#aa#...#aa#..",
"..........aa.....aa...",
"..........aa.....aa...",
"..........aa.....aa...",
".......aaaaaaaaaaaaaaa",
".......a*************a",
".......a************#a",
".......a***********+#a",
".......a***********+#a",
".......a***********+#a",
"#aa#...a***********+#a",
"abbaaaaa***********+#a",
"abbaaaaa***********+#a",
"#aa#...a***********+#a",
".......a***********+#a",
".......a***********+#a",
".......a**++++++++++#a",
".......a*############a",
".......aaaaaaaaaaaaaaa"};

QDesignerAxWidget::QDesignerAxWidget(QWidget *parent) :
    QWidget(parent),
    m_defaultSize(80, 70),
    m_drawFlags(DrawIndicator|DrawFrame|DrawControl),
    m_axobject(0),
    m_axImage(widgetIcon())
{
}

QDesignerAxWidget::~QDesignerAxWidget()
{
    delete m_axobject;
}

QPixmap QDesignerAxWidget::widgetIcon()
{
   return QPixmap(widgetIconXPM);
}

QString QDesignerAxWidget::control() const
{
    if (m_axobject)
        return m_axobject->control();
    return QString();
}

void QDesignerAxWidget::setControl(const QString &clsid)
{
    if (clsid == control())
        return;
     if (clsid.isEmpty()) {
         resetControl();
     } else {
         loadControl(clsid);
     }
}
void QDesignerAxWidget::resetControl()
{
    if (!m_axobject)
        return;
    delete m_axobject;
    m_axobject = 0;
    update();
}

bool QDesignerAxWidget::loadControl(const QString &clsid)
{
    if (clsid.isEmpty())
        return false;
    if (m_axobject)
        resetControl();
    m_axobject = new QAxWidget();

    if (!m_axobject->setControl(clsid)) {
        delete m_axobject;
        m_axobject = 0;
        return false;
    }
    update();
    return true;
}

QSize QDesignerAxWidget::sizeHint() const
{
    if (m_axobject)
        return m_axobject->sizeHint();
    return m_defaultSize;
}

QSize QDesignerAxWidget::minimumSizeHint() const
{
    if (m_axobject)
        return m_axobject->minimumSizeHint();
    return QWidget::minimumSizeHint();
}

void QDesignerAxWidget::paintEvent(QPaintEvent * /*event */)
{
    QPainter p(this);
    const QRect r = contentsRect();
    const int contentsWidth = r.width();
    const int contentsHeight= r.height();
    if (m_axobject) { // QAxWidget has no concept of sizeHint()
        if (m_drawFlags & DrawControl) {
            m_axobject->resize(size());
            m_axobject->render(&p, pos());
        }
        if (m_drawFlags & DrawIndicator) {
            static const QString loaded = tr("Control loaded");
            QColor patternColor(Qt::green);
            if (m_drawFlags & DrawControl)
                patternColor.setAlpha(80);
            p.setBrush(QBrush(patternColor, Qt::BDiagPattern));
            p.setPen(Qt::black);
            if (r.height() > 5)
                p.drawText(5,contentsHeight - 5, loaded);
        }
    }
    if (m_drawFlags & DrawFrame) {
        p.drawRect(r.adjusted(0, 0, -1, -1));
    }
    if (m_drawFlags & DrawIndicator) {
        if (contentsWidth > m_axImage.width() && contentsHeight > m_axImage.height())
            p.drawPixmap((contentsWidth - m_axImage.width()) / 2,
                         (contentsHeight-m_axImage.height()) / 2, m_axImage);
    }
}

// --------- QDesignerAxPluginWidget
QDesignerAxPluginWidget::QDesignerAxPluginWidget(QWidget *parent) :
        QDesignerAxWidget(parent)
{
}

QDesignerAxPluginWidget::~QDesignerAxPluginWidget()
{
}

const QMetaObject *QDesignerAxPluginWidget::metaObject() const
{
    if (const QAxWidget *aw = axobject())
        return aw->metaObject();

    return QDesignerAxWidget::metaObject();
}

#ifndef QT_NO_EXCEPTIONS

static QString msgComException(const QObject *o, const QMetaObject::Call call, int index)
{
    return QDesignerAxWidget::tr("A COM exception occurred when executing a meta call of type %1, index %2 of \"%3\".").
            arg(call).arg(index).arg(o->objectName());
}

#endif // QT_NO_EXCEPTIONS

int QDesignerAxPluginWidget::qt_metacall(QMetaObject::Call call, int signal, void **argv)
{
    QAxWidget *aw = axobject();
    if (!aw)
        return QDesignerAxWidget::qt_metacall(call,signal,argv);


    const QMetaObject *mo = metaObject();
    // Have base class handle inherited stuff (geometry, enabled...)
    const bool inherited = call == QMetaObject::InvokeMetaMethod ?
                           (signal < mo->methodOffset()) : (signal < mo->propertyOffset());
    if (inherited)
        return QDesignerAxWidget::qt_metacall(call, signal, argv);

    int rc = -1;
#ifndef QT_NO_EXCEPTIONS
    try {
#endif
        if (debugAxWidget)
               if (call != QMetaObject::InvokeMetaMethod)
                   qDebug() << objectName() << call << signal << mo->property(signal).name();
        switch (call) {
        case QMetaObject::QueryPropertyStored: // Pretend all changed properties are stored for them to be saved
            if (m_propValues.contains(signal))
                if (argv[0])
                    *reinterpret_cast< bool*>(argv[0]) = true;
            break;
        case QMetaObject::ResetProperty:
            rc = aw->qt_metacall(call, signal, argv);
            update();
            m_propValues.remove(signal);
            break;
        case QMetaObject::WriteProperty:
            rc = aw->qt_metacall(call, signal, argv);
            update();
            m_propValues.insert(signal, true);
            break;
        default:
            rc = aw->qt_metacall(call, signal, argv);
            break;
        }
#ifndef QT_NO_EXCEPTIONS
    } catch(...) {
        qWarning(msgComException(this, call, signal).toUtf8());
     }
#endif
    return rc;
}

QT_END_NAMESPACE
