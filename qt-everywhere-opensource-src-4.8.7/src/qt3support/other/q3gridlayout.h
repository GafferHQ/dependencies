/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt3Support module of the Qt Toolkit.
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

#ifndef Q3GRIDLAYOUT_H
#define Q3GRIDLAYOUT_H

#include <QtGui/qboxlayout.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Qt3SupportLight)

class Q3GridLayout : public QGridLayout
{
public:
    inline explicit Q3GridLayout(QWidget *parent)
        : QGridLayout(parent) { setMargin(0); setSpacing(0); }

    inline Q3GridLayout(QWidget *parent, int nRows, int nCols = 1, int margin = 0,
                        int spacing = -1, const char *name = 0)
        : QGridLayout(parent, nRows, nCols, margin, spacing, name) {}

    inline Q3GridLayout(int nRows, int nCols = 1, int spacing = -1, const char *name = 0)
        : QGridLayout(nRows, nCols, spacing, name) {}

    inline Q3GridLayout(QLayout *parentLayout, int nRows =1, int nCols = 1, int spacing = -1,
                        const char *name = 0)
        : QGridLayout(parentLayout, nRows, nCols, spacing, name) {}

private:
    Q_DISABLE_COPY(Q3GridLayout)
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // Q3GRIDLAYOUT_H
