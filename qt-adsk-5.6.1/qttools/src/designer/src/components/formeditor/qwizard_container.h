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

#ifndef QWIZARD_CONTAINER_H
#define QWIZARD_CONTAINER_H

#include <QtDesigner/QDesignerContainerExtension>

#include <qdesigner_propertysheet_p.h>
#include <extensionfactory_p.h>

#include <QtWidgets/QWizard>
#include <QtWidgets/QWizardPage>

QT_BEGIN_NAMESPACE

class QWizardPage;

namespace qdesigner_internal {

// Container for QWizard. Care must be taken to position
// the  QWizard at some valid page after removal/insertion
// as it is not used to having its pages ripped out.
class QWizardContainer: public QObject, public QDesignerContainerExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    explicit QWizardContainer(QWizard *widget, QObject *parent = 0);

    int count() const Q_DECL_OVERRIDE;
    QWidget *widget(int index) const Q_DECL_OVERRIDE;
    int currentIndex() const Q_DECL_OVERRIDE;
    void setCurrentIndex(int index) Q_DECL_OVERRIDE;
    void addWidget(QWidget *widget) Q_DECL_OVERRIDE;
    void insertWidget(int index, QWidget *widget) Q_DECL_OVERRIDE;
    void remove(int index) Q_DECL_OVERRIDE;

private:
    QWizard *m_wizard;
};

// QWizardPagePropertySheet: Introduces a attribute string fake property
// "pageId" that allows for specifying enumeration values (uic only).
// This breaks the pattern of having a "currentSth" property for the
// container, but was deemed to make sense here since the Page has
// its own "title" properties.
class QWizardPagePropertySheet: public QDesignerPropertySheet
{
    Q_OBJECT
public:
    explicit QWizardPagePropertySheet(QWizardPage *object, QObject *parent = 0);

    bool reset(int index) Q_DECL_OVERRIDE;

    static const char *pageIdProperty;

private:
    const int m_pageIdIndex;
};

// QWizardPropertySheet: Hides the "startId" property. It cannot be used
// as QWizard cannot handle setting it as a property before the actual
// page is added.

class QWizardPropertySheet: public QDesignerPropertySheet
{
    Q_OBJECT
public:
    explicit QWizardPropertySheet(QWizard *object, QObject *parent = 0);
    bool isVisible(int index) const Q_DECL_OVERRIDE;

private:
    const QString m_startId;
};

// Factories
typedef QDesignerPropertySheetFactory<QWizard, QWizardPropertySheet>  QWizardPropertySheetFactory;
typedef QDesignerPropertySheetFactory<QWizardPage, QWizardPagePropertySheet>  QWizardPagePropertySheetFactory;
typedef ExtensionFactory<QDesignerContainerExtension,  QWizard,  QWizardContainer> QWizardContainerFactory;
}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // QWIZARD_CONTAINER_H
