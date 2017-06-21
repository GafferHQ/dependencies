/****************************************************************************
**
** Copyright (C) 2014 Intel Corporation
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
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

// ### Qt6: verify if we can remove this, somehow.
// First, try to see if the extern template from qvector.h is necessary.
// If it still is, check if removing the copy constructors in qarraydata.h
// make the calling convention of both sets of begin() and end() functions
// match, as it does for the IA-64 C++ ABI.

#ifdef QVECTOR_H
#  error "This file must be compiled with no precompiled headers"
#endif

// invert the setting of QT_STRICT_ITERATORS, whichever it was
#ifdef QT_STRICT_ITERATORS
#  undef QT_STRICT_ITERATORS
#else
#  define QT_STRICT_ITERATORS
#endif

// the Q_TEMPLATE_EXTERN at the bottom of qvector.h will do the trick
#include <QtCore/qvector.h>
