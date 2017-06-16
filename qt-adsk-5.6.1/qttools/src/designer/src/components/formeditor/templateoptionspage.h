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

#ifndef QDESIGNER_TEMPLATEOPTIONS_H
#define QDESIGNER_TEMPLATEOPTIONS_H

#include <QtDesigner/QDesignerOptionsPageInterface>

#include <QtCore/QPointer>
#include <QtCore/QStringList>

#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class QDesignerFormEditorInterface;

namespace qdesigner_internal {

namespace Ui {
    class TemplateOptionsWidget;
}

/* Present the user with a list of form template paths to save
 * form templates. */
class TemplateOptionsWidget : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(TemplateOptionsWidget)
public:
    explicit TemplateOptionsWidget(QDesignerFormEditorInterface *core,
                                              QWidget *parent = 0);
    ~TemplateOptionsWidget();


    QStringList templatePaths() const;
    void setTemplatePaths(const QStringList &l);

    static QString chooseTemplatePath(QDesignerFormEditorInterface *core, QWidget *parent);

private slots:
    void addTemplatePath();
    void removeTemplatePath();
    void templatePathSelectionChanged();

private:
    QDesignerFormEditorInterface *m_core;
    Ui::TemplateOptionsWidget *m_ui;
};

class TemplateOptionsPage : public QDesignerOptionsPageInterface
{
     Q_DISABLE_COPY(TemplateOptionsPage)
public:
    explicit TemplateOptionsPage(QDesignerFormEditorInterface *core);

    QString name() const Q_DECL_OVERRIDE;
    QWidget *createPage(QWidget *parent) Q_DECL_OVERRIDE;
    void apply() Q_DECL_OVERRIDE;
    void finish() Q_DECL_OVERRIDE;

private:
    QDesignerFormEditorInterface *m_core;
    QStringList m_initialTemplatePaths;
    QPointer<TemplateOptionsWidget> m_widget;
};

}

QT_END_NAMESPACE

#endif // QDESIGNER_TEMPLATEOPTIONS_H
