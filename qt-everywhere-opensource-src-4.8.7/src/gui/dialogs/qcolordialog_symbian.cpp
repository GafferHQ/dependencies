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

#include "qcolordialog_p.h"

#ifndef QT_NO_COLORDIALOG


#include "qcolor.h"
#include "private/qguiplatformplugin_p.h"

#ifdef Q_WS_S60
#include <AknColourSelectionGrid.h>
#endif

#include "private/qt_s60_p.h"

QT_BEGIN_NAMESPACE

QColor launchSymbianColorDialog(QColor initial)
{
    QColor currentColor = QColor::Invalid;
#ifdef Q_WS_S60
    QT_TRAP_THROWING(
        CArrayFixFlat<TRgb>* array = new( ELeave ) CArrayFixFlat<TRgb>(17);
        CleanupStack::PushL(array);
        array->AppendL(KRgbBlack);
        array->AppendL(KRgbDarkGray);
        array->AppendL(KRgbDarkRed);
        array->AppendL(KRgbDarkGreen);
        array->AppendL(KRgbDarkYellow);
        array->AppendL(KRgbDarkBlue);
        array->AppendL(KRgbDarkMagenta);
        array->AppendL(KRgbDarkCyan);
        array->AppendL(KRgbRed);
        array->AppendL(KRgbGreen);
        array->AppendL(KRgbYellow);
        array->AppendL(KRgbBlue);
        array->AppendL(KRgbMagenta);
        array->AppendL(KRgbCyan);
        array->AppendL(KRgbGray);
        array->AppendL(KRgbWhite);

        TRgb initialColour(initial.red(), initial.green(), initial.blue(), initial.alpha());

        TBool noneChosen = EFalse; // If true shows the default colour button
        CAknColourSelectionGrid* colourSelectionGrid =
            CAknColourSelectionGrid::NewL(array, EFalse, noneChosen, initialColour);
        CleanupStack::PushL(colourSelectionGrid);

        if (colourSelectionGrid->ExecuteLD()) {
            currentColor.setRgb(initialColour.Red(), initialColour.Green(),
                                initialColour.Blue(), initialColour.Alpha());
        }
        CleanupStack::Pop(colourSelectionGrid);
        CleanupStack::PopAndDestroy(array);
    );
#endif
    return currentColor;
}

QColor qtSymbianGetColor(const QColor &initial)
{
    return launchSymbianColorDialog(initial);
}

QT_END_NAMESPACE

#endif // QT_NO_COLORDIALOG
