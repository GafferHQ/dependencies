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

#ifndef Q3WIZARD_CONTAINER_H
#define Q3WIZARD_CONTAINER_H

#include <QtDesigner/QDesignerContainerExtension>
#include <QtDesigner/QExtensionFactory>
#include <QtDesigner/QDesignerExtraInfoExtension>
#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/private/qdesigner_propertysheet_p.h>

#include <QtCore/QPointer>
#include <Qt3Support/Q3Wizard>

QT_BEGIN_NAMESPACE

class Q3Wizard;

class Q3WizardHelper : public QObject
{
    Q_OBJECT
public:
    explicit Q3WizardHelper(Q3Wizard *wizard);
private slots:
    void slotCurrentChanged();
private:
    Q3Wizard *m_wizard;
};

class Q3WizardExtraInfo: public QObject, public QDesignerExtraInfoExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerExtraInfoExtension)
public:
    explicit Q3WizardExtraInfo(Q3Wizard *wizard, QDesignerFormEditorInterface *core, QObject *parent);

    virtual QWidget *widget() const;
    virtual Q3Wizard *wizard() const;
    virtual QDesignerFormEditorInterface *core() const;

    virtual bool saveUiExtraInfo(DomUI *ui);
    virtual bool loadUiExtraInfo(DomUI *ui);

    virtual bool saveWidgetExtraInfo(DomWidget *ui_widget);
    virtual bool loadWidgetExtraInfo(DomWidget *ui_widget);

private:
    QPointer<Q3Wizard> m_wizard;
    QPointer<QDesignerFormEditorInterface> m_core;
};

class Q3WizardExtraInfoFactory: public QExtensionFactory
{
    Q_OBJECT
public:
    Q3WizardExtraInfoFactory(QDesignerFormEditorInterface *core, QExtensionManager *parent = 0);

protected:
    virtual QObject *createExtension(QObject *object, const QString &iid, QObject *parent) const;

private:
    QDesignerFormEditorInterface *m_core;
};

class Q3WizardContainer: public QObject, public QDesignerContainerExtension
{
    Q_OBJECT
    Q_INTERFACES(QDesignerContainerExtension)
public:
    explicit Q3WizardContainer(Q3Wizard *wizard, QObject *parent = 0);

    virtual int count() const;
    virtual QWidget *widget(int index) const;
    virtual int currentIndex() const;
    virtual void setCurrentIndex(int index);
    virtual void addWidget(QWidget *widget);
    virtual void insertWidget(int index, QWidget *widget);
    virtual void remove(int index);

private:
    Q3Wizard *m_wizard;
};

class Q3WizardContainerFactory: public QExtensionFactory
{
    Q_OBJECT
public:
    explicit Q3WizardContainerFactory(QExtensionManager *parent = 0);

protected:
    virtual QObject *createExtension(QObject *object, const QString &iid, QObject *parent) const;
};

class Q3WizardPropertySheet : public QDesignerPropertySheet {
public:
    explicit Q3WizardPropertySheet(Q3Wizard *object, QObject *parent = 0);

    virtual void setProperty(int index, const QVariant &value);
    virtual QVariant property(int index) const;
    virtual bool reset(int index);

private:
    Q3Wizard *m_wizard;
};

typedef QDesignerPropertySheetFactory<Q3Wizard, Q3WizardPropertySheet> Q3WizardPropertySheetFactory;

QT_END_NAMESPACE

#endif // Q3WIZARD_CONTAINER_H
