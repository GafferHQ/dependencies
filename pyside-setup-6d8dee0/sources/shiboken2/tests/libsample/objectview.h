/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of PySide2.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef OBJECTVIEW_H
#define OBJECTVIEW_H

#include "objecttype.h"
#include "libsamplemacros.h"

class Str;
class ObjectModel;

class LIBSAMPLE_API ObjectView : public ObjectType
{
public:
    ObjectView(ObjectModel* model = 0, ObjectType* parent = 0)
        : ObjectType(parent), m_model(model)
    {}

    inline void setModel(ObjectModel* model) { m_model = model; }
    inline ObjectModel* model() const { return m_model; }

    Str displayModelData();
    void modifyModelData(Str& data);

    ObjectType* getRawModelData();

private:
    ObjectModel* m_model;
};

#endif // OBJECTVIEW_H

