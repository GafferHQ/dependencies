/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#ifndef LUPDATE_H
#define LUPDATE_H

#include "qglobal.h"

#include <QList>

QT_BEGIN_NAMESPACE

class ConversionData;
class QString;
class QStringList;
class Translator;
class TranslatorMessage;

enum UpdateOption {
    Verbose = 1,
    NoObsolete = 2,
    PluralOnly = 4,
    NoSort = 8,
    HeuristicSameText = 16,
    HeuristicSimilarText = 32,
    HeuristicNumber = 64,
    AbsoluteLocations = 256,
    RelativeLocations = 512,
    NoLocations = 1024,
    NoUiLines = 2048
};

Q_DECLARE_FLAGS(UpdateOptions, UpdateOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(UpdateOptions)

Translator merge(const Translator &tor, const Translator &virginTor,
    UpdateOptions options, QString &err);

void fetchtrInlinedCpp(const QString &in, Translator &translator, const QString &context);
void loadCPP(Translator &translator, const QStringList &filenames, ConversionData &cd);
bool loadJava(Translator &translator, const QString &filename, ConversionData &cd);
bool loadQScript(Translator &translator, const QString &filename, ConversionData &cd);
bool loadUI(Translator &translator, const QString &filename, ConversionData &cd);
bool loadQml(Translator &translator, const QString &filename, ConversionData &cd);

QT_END_NAMESPACE

#endif
