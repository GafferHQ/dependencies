/***************************************************************************
**
** Copyright (C) 2012 Research In Motion
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QQNXFILEDIALOGHELPER_H
#define QQNXFILEDIALOGHELPER_H

#include <qpa/qplatformdialoghelper.h>


QT_BEGIN_NAMESPACE

class QQnxIntegration;

class QQnxFilePicker;

class QQnxFileDialogHelper : public QPlatformFileDialogHelper
{
    Q_OBJECT
public:
    explicit QQnxFileDialogHelper(const QQnxIntegration *);
    ~QQnxFileDialogHelper();

    void exec();

    bool show(Qt::WindowFlags flags, Qt::WindowModality modality, QWindow *parent);
    void hide();

    bool defaultNameFilterDisables() const;
    void setDirectory(const QUrl &directory) Q_DECL_OVERRIDE;
    QUrl directory() const Q_DECL_OVERRIDE;
    void selectFile(const QUrl &fileName) Q_DECL_OVERRIDE;
    QList<QUrl> selectedFiles() const Q_DECL_OVERRIDE;
    void setFilter();
    void selectNameFilter(const QString &filter);
    QString selectedNameFilter() const;

    QQnxFilePicker *nativeDialog() const { return m_dialog; }

Q_SIGNALS:
    void dialogClosed();

private Q_SLOTS:
    void emitSignals();

private:
    void setNameFilter(const QString &filter);
    void setNameFilters(const QStringList &filters);

    const QQnxIntegration *m_integration;
    QQnxFilePicker *m_dialog;
    QFileDialogOptions::AcceptMode m_acceptMode;
    QString m_selectedFilter;
};

QT_END_NAMESPACE

#endif // QQNXFILEDIALOGHELPER_H
