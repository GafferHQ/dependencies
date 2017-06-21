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

#include "qdesigner_formwindow.h"
#include "qdesigner_workbench.h"
#include "formwindowbase_p.h"

// sdk
#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerPropertySheetExtension>
#include <QtDesigner/QDesignerPropertyEditorInterface>
#include <QtDesigner/QDesignerFormWindowManagerInterface>
#include <QtDesigner/QDesignerTaskMenuExtension>
#include <QtDesigner/QExtensionManager>

#include <QtCore/QEvent>
#include <QtCore/QFile>

#include <QtWidgets/QAction>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QUndoCommand>
#include <QtGui/QWindowStateChangeEvent>

QT_BEGIN_NAMESPACE

QDesignerFormWindow::QDesignerFormWindow(QDesignerFormWindowInterface *editor, QDesignerWorkbench *workbench, QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags),
      m_editor(editor),
      m_workbench(workbench),
      m_action(new QAction(this)),
      m_initialized(false),
      m_windowTitleInitialized(false)
{
    Q_ASSERT(workbench);

    setMaximumSize(0xFFF, 0xFFF);
    QDesignerFormEditorInterface *core = workbench->core();

    if (m_editor) {
        m_editor->setParent(this);
    } else {
        m_editor = core->formWindowManager()->createFormWindow(this);
    }

    QVBoxLayout *l = new QVBoxLayout(this);
    l->setMargin(0);
    l->addWidget(m_editor);

    m_action->setCheckable(true);

    connect(m_editor->commandHistory(), &QUndoStack::indexChanged, this, &QDesignerFormWindow::updateChanged);
    connect(m_editor.data(), &QDesignerFormWindowInterface::geometryChanged,
            this, &QDesignerFormWindow::slotGeometryChanged);
}

QDesignerFormWindow::~QDesignerFormWindow()
{
    if (workbench())
        workbench()->removeFormWindow(this);
}

QAction *QDesignerFormWindow::action() const
{
    return m_action;
}

void QDesignerFormWindow::changeEvent(QEvent *e)
{
    switch (e->type()) {
        case QEvent::WindowTitleChange:
            m_action->setText(windowTitle().remove(QStringLiteral("[*]")));
            break;
        case QEvent::WindowIconChange:
            m_action->setIcon(windowIcon());
            break;
    case QEvent::WindowStateChange: {
        const  QWindowStateChangeEvent *wsce =  static_cast<const QWindowStateChangeEvent *>(e);
        const bool wasMinimized = Qt::WindowMinimized & wsce->oldState();
        const bool isMinimizedNow = isMinimized();
        if (wasMinimized != isMinimizedNow )
            emit minimizationStateChanged(m_editor, isMinimizedNow);
    }
        break;
        default:
            break;
    }
    QWidget::changeEvent(e);
}

QRect QDesignerFormWindow::geometryHint() const
{
    const QPoint point(0, 0);
    // If we have a container, we want to be just as big.
    // QMdiSubWindow attempts to resize its children to sizeHint() when switching user interface modes.
    if (QWidget *mainContainer = m_editor->mainContainer())
        return QRect(point, mainContainer->size());

    return QRect(point, sizeHint());
}

QDesignerFormWindowInterface *QDesignerFormWindow::editor() const
{
    return m_editor;
}

QDesignerWorkbench *QDesignerFormWindow::workbench() const
{
    return m_workbench;
}

void QDesignerFormWindow::firstShow()
{
    // Set up handling of file name changes and set initial title.
    if (!m_windowTitleInitialized) {
        m_windowTitleInitialized = true;
        if (m_editor) {
            connect(m_editor.data(), &QDesignerFormWindowInterface::fileNameChanged,
                    this, &QDesignerFormWindow::updateWindowTitle);
            updateWindowTitle(m_editor->fileName());
            updateChanged();
        }
    }
    show();
}

int QDesignerFormWindow::getNumberOfUntitledWindows() const
{
    const int totalWindows = m_workbench->formWindowCount();
    if (!totalWindows)
        return 0;

    int maxUntitled = 0;
    // Find the number of untitled windows excluding ourselves.
    // Do not fall for 'untitled.ui', match with modified place holder.
    // This will cause some problems with i18n, but for now I need the string to be "static"
    QRegExp rx(QStringLiteral("untitled( (\\d+))?\\[\\*\\]"));
    for (int i = 0; i < totalWindows; ++i) {
        QDesignerFormWindow *fw =  m_workbench->formWindow(i);
        if (fw != this) {
            const QString title = m_workbench->formWindow(i)->windowTitle();
            if (rx.indexIn(title) != -1) {
                if (maxUntitled == 0)
                    ++maxUntitled;
                if (rx.captureCount() > 1) {
                    const QString numberCapture = rx.cap(2);
                    if (!numberCapture.isEmpty())
                        maxUntitled = qMax(numberCapture.toInt(), maxUntitled);
                }
            }
        }
    }
    return maxUntitled;
}

void QDesignerFormWindow::updateWindowTitle(const QString &fileName)
{
    if (!m_windowTitleInitialized) {
        m_windowTitleInitialized = true;
        if (m_editor)
            connect(m_editor.data(), &QDesignerFormWindowInterface::fileNameChanged,
                    this, &QDesignerFormWindow::updateWindowTitle);
    }

    QString fileNameTitle;
    if (fileName.isEmpty()) {
        fileNameTitle = QStringLiteral("untitled");
        if (const int maxUntitled = getNumberOfUntitledWindows()) {
            fileNameTitle += QLatin1Char(' ');
            fileNameTitle += QString::number(maxUntitled + 1);
        }
    } else {
        fileNameTitle = QFileInfo(fileName).fileName();
    }

    if (const QWidget *mc = m_editor->mainContainer()) {
        setWindowIcon(mc->windowIcon());
        setWindowTitle(tr("%1 - %2[*]").arg(mc->windowTitle()).arg(fileNameTitle));
    } else {
        setWindowTitle(fileNameTitle);
    }
}

void QDesignerFormWindow::closeEvent(QCloseEvent *ev)
{
    if (m_editor->isDirty()) {
        raise();
        QMessageBox box(QMessageBox::Information, tr("Save Form?"),
                tr("Do you want to save the changes to this document before closing?"),
                QMessageBox::Discard | QMessageBox::Cancel | QMessageBox::Save, m_editor);
        box.setInformativeText(tr("If you don't save, your changes will be lost."));
        box.setWindowModality(Qt::WindowModal);
        static_cast<QPushButton *>(box.button(QMessageBox::Save))->setDefault(true);

        switch (box.exec()) {
            case QMessageBox::Save: {
                bool ok = workbench()->saveForm(m_editor);
                ev->setAccepted(ok);
                m_editor->setDirty(!ok);
                break;
            }
            case QMessageBox::Discard:
                m_editor->setDirty(false); // Not really necessary, but stops problems if we get close again.
                ev->accept();
                break;
            case QMessageBox::Cancel:
                ev->ignore();
                break;
        }
    }
}

void QDesignerFormWindow::updateChanged()
{
    // Sometimes called after form window destruction.
    if (m_editor) {
        setWindowModified(m_editor->isDirty());
        updateWindowTitle(m_editor->fileName());
    }
}

void QDesignerFormWindow::resizeEvent(QResizeEvent *rev)
{
    if(m_initialized) {
        m_editor->setDirty(true);
        setWindowModified(true);
    }

    m_initialized = true;
    QWidget::resizeEvent(rev);
}

void QDesignerFormWindow::slotGeometryChanged()
{
    // If the form window changes, re-update the geometry of the current widget in the property editor.
    // Note that in the case of layouts, non-maincontainer widgets must also be updated,
    // so, do not do it for the main container only
    const QDesignerFormEditorInterface *core = m_editor->core();
    QObject *object = core->propertyEditor()->object();
    if (object == 0 || !object->isWidgetType())
        return;
    static const QString geometryProperty = QStringLiteral("geometry");
    const QDesignerPropertySheetExtension *sheet = qt_extension<QDesignerPropertySheetExtension*>(core->extensionManager(), object);
    const int geometryIndex = sheet->indexOf(geometryProperty);
    if (geometryIndex == -1)
        return;
    core->propertyEditor()->setPropertyValue(geometryProperty, sheet->property(geometryIndex));
}

QT_END_NAMESPACE
