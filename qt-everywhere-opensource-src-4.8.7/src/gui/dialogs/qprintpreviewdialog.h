/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QPRINTPREVIEWDIALOG_H
#define QPRINTPREVIEWDIALOG_H

#include <QtGui/qdialog.h>

#ifndef QT_NO_PRINTPREVIEWDIALOG

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Gui)

class QGraphicsView;
class QPrintPreviewDialogPrivate;

class Q_GUI_EXPORT QPrintPreviewDialog : public QDialog
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QPrintPreviewDialog)

public:
    explicit QPrintPreviewDialog(QWidget *parent = 0, Qt::WindowFlags flags = 0);
    explicit QPrintPreviewDialog(QPrinter *printer, QWidget *parent = 0, Qt::WindowFlags flags = 0);
    ~QPrintPreviewDialog();

#ifdef Q_NO_USING_KEYWORD
#ifndef Q_QDOC
    void open() { QDialog::open(); }
#endif
#else
    using QDialog::open;
#endif
    void open(QObject *receiver, const char *member);

    QPrinter *printer();

    void setVisible(bool visible);
    void done(int result);

Q_SIGNALS:
    void paintRequested(QPrinter *printer);

private:
    Q_PRIVATE_SLOT(d_func(), void _q_fit(QAction *action))
    Q_PRIVATE_SLOT(d_func(), void _q_zoomIn())
    Q_PRIVATE_SLOT(d_func(), void _q_zoomOut())
    Q_PRIVATE_SLOT(d_func(), void _q_navigate(QAction *action))
    Q_PRIVATE_SLOT(d_func(), void _q_setMode(QAction *action))
    Q_PRIVATE_SLOT(d_func(), void _q_pageNumEdited())
    Q_PRIVATE_SLOT(d_func(), void _q_print())
    Q_PRIVATE_SLOT(d_func(), void _q_pageSetup())
    Q_PRIVATE_SLOT(d_func(), void _q_previewChanged())
    Q_PRIVATE_SLOT(d_func(), void _q_zoomFactorChanged())

    void *dummy; // ### Qt 5 - remove me
};


QT_END_NAMESPACE

QT_END_HEADER

#endif // QT_NO_PRINTPREVIEWDIALOG

#endif // QPRINTPREVIEWDIALOG_H
