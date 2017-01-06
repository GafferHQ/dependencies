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

#include "qpagesetupdialog.h"

#ifndef QT_NO_PRINTDIALOG
#include <qapplication.h>

#include <private/qprintengine_win_p.h>
#include <private/qabstractpagesetupdialog_p.h>

QT_BEGIN_NAMESPACE

class QPageSetupDialogPrivate : public QAbstractPageSetupDialogPrivate
{
};

QPageSetupDialog::QPageSetupDialog(QPrinter *printer, QWidget *parent)
    : QAbstractPageSetupDialog(*(new QPageSetupDialogPrivate), printer, parent)
{
}

QPageSetupDialog::QPageSetupDialog(QWidget *parent)
    : QAbstractPageSetupDialog(*(new QPageSetupDialogPrivate), 0, parent)
{
}

int QPageSetupDialog::exec()
{
    Q_D(QPageSetupDialog);

    if (d->printer->outputFormat() != QPrinter::NativeFormat)
        return Rejected;

    QWin32PrintEngine *engine = static_cast<QWin32PrintEngine*>(d->printer->paintEngine());
    QWin32PrintEnginePrivate *ep = static_cast<QWin32PrintEnginePrivate *>(engine->d_ptr.data());

    PAGESETUPDLG psd;
    memset(&psd, 0, sizeof(PAGESETUPDLG));
    psd.lStructSize = sizeof(PAGESETUPDLG);

    // we need a temp DEVMODE struct if we don't have a global DEVMODE
    HGLOBAL hDevMode = 0;
    int devModeSize = 0;
    if (!ep->globalDevMode) {
        devModeSize = sizeof(DEVMODE) + ep->devMode->dmDriverExtra;
        hDevMode = GlobalAlloc(GHND, devModeSize);
        if (hDevMode) {
            void *dest = GlobalLock(hDevMode);
            memcpy(dest, ep->devMode, devModeSize);
            GlobalUnlock(hDevMode);
        }
        psd.hDevMode = hDevMode;
    } else {
        psd.hDevMode = ep->devMode;
    }

    HGLOBAL *tempDevNames = ep->createDevNames();
    psd.hDevNames = tempDevNames;

    QWidget *parent = parentWidget();
    parent = parent ? parent->window() : QApplication::activeWindow();
    Q_ASSERT(!parent ||parent->testAttribute(Qt::WA_WState_Created));
    psd.hwndOwner = parent ? parent->winId() : 0;

    psd.Flags = PSD_MARGINS;
    double multiplier = 1;
    switch (QLocale::system().measurementSystem()) {
    case QLocale::MetricSystem:
        psd.Flags |= PSD_INHUNDREDTHSOFMILLIMETERS;
        multiplier = 1;
        break;
    case QLocale::ImperialSystem:
        psd.Flags |= PSD_INTHOUSANDTHSOFINCHES;
        multiplier = 25.4/10;
        break;
    }

    QRect marginRect = ep->getPageMargins();
    psd.rtMargin.left   = marginRect.left()   / multiplier;
    psd.rtMargin.top    = marginRect.top()    / multiplier;
    psd.rtMargin.right  = marginRect.width()  / multiplier;;
    psd.rtMargin.bottom = marginRect.height() / multiplier;;

    bool result = PageSetupDlg(&psd);
    if (result) {
        ep->readDevnames(psd.hDevNames);
        ep->readDevmode(psd.hDevMode);

        QRect theseMargins = QRect(psd.rtMargin.left   * multiplier,
                                   psd.rtMargin.top    * multiplier,
                                   psd.rtMargin.right  * multiplier,
                                   psd.rtMargin.bottom * multiplier);

        if (theseMargins != marginRect) {
            ep->setPageMargins(psd.rtMargin.left   * multiplier,
                               psd.rtMargin.top    * multiplier,
                               psd.rtMargin.right  * multiplier,
                               psd.rtMargin.bottom * multiplier);
        }

        ep->updateCustomPaperSize();

        // copy from our temp DEVMODE struct
        if (!ep->globalDevMode && hDevMode) {
            void *src = GlobalLock(hDevMode);
            memcpy(ep->devMode, src, devModeSize);
            GlobalUnlock(hDevMode);
        }
    }

    if (!ep->globalDevMode && hDevMode)
        GlobalFree(hDevMode);
    GlobalFree(tempDevNames);
    done(result);
    return result;
}

void QPageSetupDialog::setVisible(bool visible)
{
    if (!visible)
        return;
    exec();
}

QT_END_NAMESPACE
#endif
