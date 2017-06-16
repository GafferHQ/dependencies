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

#include "qpagesetupdialog.h"
#include <private/qpagesetupdialog_p.h>

#include <QtPrintSupport/qprinter.h>

#ifndef QT_NO_PRINTDIALOG

QT_BEGIN_NAMESPACE

/*!
    \class QPageSetupDialog

    \brief The QPageSetupDialog class provides a configuration dialog
    for the page-related options on a printer.

    \ingroup standard-dialogs
    \ingroup printing
    \inmodule QtPrintSupport

    On Windows and OS X the page setup dialog is implemented using
    the native page setup dialogs.

    Note that on Windows and OS X custom paper sizes won't be
    reflected in the native page setup dialogs. Additionally, custom
    page margins set on a QPrinter won't show in the native OS X
    page setup dialog.

    \sa QPrinter, QPrintDialog
*/


/*!
    \fn QPageSetupDialog::QPageSetupDialog(QPrinter *printer, QWidget *parent)

    Constructs a page setup dialog that configures \a printer with \a
    parent as the parent widget.
*/

/*!
    \fn QPageSetupDialog::~QPageSetupDialog()

    Destroys the page setup dialog.
*/

/*!
    \since 4.5

    \fn QPageSetupDialog::QPageSetupDialog(QWidget *parent)

    Constructs a page setup dialog that configures a default-constructed
    QPrinter with \a parent as the parent widget.

    \sa printer()
*/

/*!
    \fn QPrinter *QPageSetupDialog::printer()

    Returns the printer that was passed to the QPageSetupDialog
    constructor.
*/

QPageSetupDialogPrivate::QPageSetupDialogPrivate(QPrinter *prntr) : printer(0), ownsPrinter(false)
{
    setPrinter(prntr);
}

void QPageSetupDialogPrivate::setPrinter(QPrinter *newPrinter)
{
    if (printer && ownsPrinter)
        delete printer;

    if (newPrinter) {
        printer = newPrinter;
        ownsPrinter = false;
    } else {
        printer = new QPrinter;
        ownsPrinter = true;
    }
#ifndef Q_DEAD_CODE_FROM_QT4_X11
    if (printer->outputFormat() != QPrinter::NativeFormat)
        qWarning("QPageSetupDialog: Cannot be used on non-native printers");
#endif
}

/*!
    \overload
    \since 4.5

    Opens the dialog and connects its accepted() signal to the slot specified
    by \a receiver and \a member.

    The signal will be disconnected from the slot when the dialog is closed.
*/
void QPageSetupDialog::open(QObject *receiver, const char *member)
{
    Q_D(QPageSetupDialog);
    connect(this, SIGNAL(accepted()), receiver, member);
    d->receiverToDisconnectOnClose = receiver;
    d->memberToDisconnectOnClose = member;
    QDialog::open();
}

#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
/*! \fn void QPageSetupDialog::setVisible(bool visible)
    \reimp
*/
#endif

QPageSetupDialog::~QPageSetupDialog()
{
    Q_D(QPageSetupDialog);
    if (d->ownsPrinter)
        delete d->printer;
}

QPrinter *QPageSetupDialog::printer()
{
    Q_D(QPageSetupDialog);
    return d->printer;
}

/*!
    \fn int QPageSetupDialog::exec()

    This virtual function is called to pop up the dialog. It must be
    reimplemented in subclasses.
*/

/*!
    \reimp
*/
void QPageSetupDialog::done(int result)
{
    Q_D(QPageSetupDialog);
    QDialog::done(result);
    if (d->receiverToDisconnectOnClose) {
        disconnect(this, SIGNAL(accepted()),
                   d->receiverToDisconnectOnClose, d->memberToDisconnectOnClose);
        d->receiverToDisconnectOnClose = 0;
    }
    d->memberToDisconnectOnClose.clear();

}

QT_END_NAMESPACE

#endif
