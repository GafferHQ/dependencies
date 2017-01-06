/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "q3toolbar/q3toolbar_plugin.h"
#include "q3iconview/q3iconview_plugin.h"
#include "q3wizard/q3wizard_plugin.h"
#include "q3mainwindow/q3mainwindow_plugin.h"
#include "q3widgetstack/q3widgetstack_plugin.h"
#include "q3listview/q3listview_plugin.h"
#include "q3table/q3table_plugin.h"
#include "q3listbox/q3listbox_plugin.h"
#include "q3listview/q3listview_plugin.h"
#include "q3textedit/q3textedit_plugin.h"
#include "q3widgets/q3widget_plugins.h"

#include <QtDesigner/QDesignerCustomWidgetCollectionInterface>
#include <QtCore/qplugin.h>
#include <QtCore/qdebug.h>
#include <QtGui/QIcon>

QT_BEGIN_NAMESPACE

class Qt3SupportWidgets: public QObject, public QDesignerCustomWidgetCollectionInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)
public:
    Qt3SupportWidgets(QObject *parent = 0);

    virtual QList<QDesignerCustomWidgetInterface*> customWidgets() const;

private:
    QList<QDesignerCustomWidgetInterface*> m_plugins;
};

Qt3SupportWidgets::Qt3SupportWidgets(QObject *parent)
    : QObject(parent)
{
    const QIcon qt3Icon(QLatin1String(":/trolltech/formeditor/images/qt3logo.png"));
    m_plugins.append(new Q3ToolBarPlugin(qt3Icon, this));
    m_plugins.append(new Q3IconViewPlugin(qt3Icon, this));
    m_plugins.append(new Q3GroupBoxPlugin(qt3Icon, this));
    m_plugins.append(new Q3FramePlugin(qt3Icon, this));
    m_plugins.append(new Q3WizardPlugin(qt3Icon, this));
    m_plugins.append(new Q3MainWindowPlugin(qt3Icon, this));
    m_plugins.append(new Q3WidgetStackPlugin(qt3Icon, this));
    m_plugins.append(new Q3ButtonGroupPlugin(qt3Icon, this));
    m_plugins.append(new Q3TablePlugin(qt3Icon, this));
    m_plugins.append(new Q3ListBoxPlugin(qt3Icon, this));
    m_plugins.append(new Q3ListViewPlugin(qt3Icon, this));
    m_plugins.append(new Q3ComboBoxPlugin(qt3Icon, this));
    m_plugins.append(new Q3TextEditPlugin(qt3Icon, this));
    m_plugins.append(new Q3DateEditPlugin(qt3Icon, this));
    m_plugins.append(new Q3TimeEditPlugin(qt3Icon, this));
    m_plugins.append(new Q3DateTimeEditPlugin(qt3Icon, this));
    m_plugins.append(new Q3ProgressBarPlugin(qt3Icon, this));
    m_plugins.append(new Q3TextBrowserPlugin(qt3Icon, this));
}

QList<QDesignerCustomWidgetInterface*> Qt3SupportWidgets::customWidgets() const
{
    return m_plugins;
}

Q_EXPORT_PLUGIN(Qt3SupportWidgets)

QT_END_NAMESPACE

#include "qt3supportwidgets.moc"
