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

#include "qprinterinfo.h"
#include "qprinterinfo_p.h"

#include <qstringlist.h>

#include <qt_windows.h>

QT_BEGIN_NAMESPACE

#ifndef QT_NO_PRINTER

extern QPrinter::PaperSize mapDevmodePaperSize(int s);

//QList<QPrinterInfo> QPrinterInfo::availablePrinters()
//{
//    QList<QPrinterInfo> printers;

//    DWORD needed = 0;
//    DWORD returned = 0;
//    if (!EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, 0, 0, &needed, &returned)) {
//        LPBYTE buffer = new BYTE[needed];
//        if (EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4, buffer, needed, &needed, &returned)) {
//            PPRINTER_INFO_4 infoList = reinterpret_cast<PPRINTER_INFO_4>(buffer);
//            QPrinterInfo defPrn = defaultPrinter();
//            for (uint i = 0; i < returned; ++i) {
//                QString printerName(QString::fromWCharArray(infoList[i].pPrinterName));

//                QPrinterInfo printerInfo(printerName);
//                if (printerInfo.printerName() == defPrn.printerName())
//                    printerInfo.d_ptr->isDefault = true;
//                printers.append(printerInfo);
//            }
//        }
//        delete [] buffer;
//    }

//    return printers;
//}

//QPrinterInfo QPrinterInfo::defaultPrinter()
//{
//    QString noPrinters(QLatin1String("qt_no_printers"));
//    wchar_t buffer[256];
//    GetProfileString(L"windows", L"device", (wchar_t*)noPrinters.utf16(), buffer, 256);
//    QString output = QString::fromWCharArray(buffer);
//    if (output != noPrinters) {
//        // Filter out the name of the printer, which should be everything before a comma.
//        QString printerName = output.split(QLatin1Char(',')).value(0);
//        QPrinterInfo printerInfo(printerName);
//        printerInfo.d_ptr->isDefault = true;
//        return printerInfo;
//    }

//    return QPrinterInfo();
//}

//QList<QPrinter::PaperSize> QPrinterInfo::supportedPaperSizes() const
//{
//    const Q_D(QPrinterInfo);

//    QList<QPrinter::PaperSize> paperSizes;
//    if (isNull())
//        return paperSizes;

//    DWORD size = DeviceCapabilities(reinterpret_cast<const wchar_t*>(d->name.utf16()),
//                                    NULL, DC_PAPERS, NULL, NULL);
//    if ((int)size != -1) {
//        wchar_t *papers = new wchar_t[size];
//        size = DeviceCapabilities(reinterpret_cast<const wchar_t*>(d->name.utf16()),
//                                  NULL, DC_PAPERS, papers, NULL);
//        for (int c = 0; c < (int)size; ++c)
//            paperSizes.append(mapDevmodePaperSize(papers[c]));
//        delete [] papers;
//    }

//    return paperSizes;
//}

#endif // QT_NO_PRINTER

QT_END_NAMESPACE
