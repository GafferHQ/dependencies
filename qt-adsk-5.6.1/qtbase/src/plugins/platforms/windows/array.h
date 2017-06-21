/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#ifndef ARRAY_H
#define ARRAY_H

#include <QtCore/QtGlobal>
#include <algorithm>

QT_BEGIN_NAMESPACE

/* A simple, non-shared array. */

template <class T>
class Array
{
    Q_DISABLE_COPY(Array)
public:
    enum { initialSize = 5 };

    typedef T* const_iterator;

    explicit Array(size_t size= 0) : data(0), m_capacity(0), m_size(0)
        { if (size) resize(size); }
    ~Array() { delete [] data; }

    T *data;
    inline size_t size() const          { return m_size; }
    inline const_iterator begin() const { return data; }
    inline const_iterator end() const   { return data + m_size; }

    inline void append(const T &value)
    {
        const size_t oldSize = m_size;
        resize(m_size + 1);
        data[oldSize] = value;
    }

    inline void resize(size_t size)
    {
        if (size > m_size)
            reserve(size > 1 ? size + size / 2 : size_t(initialSize));
        m_size = size;
    }

    void reserve(size_t capacity)
    {
        if (capacity > m_capacity) {
            const T *oldData = data;
            data = new T[capacity];
            if (oldData) {
                std::copy(oldData, oldData + m_size,
                          QT_MAKE_CHECKED_ARRAY_ITERATOR(data, capacity));
                delete [] oldData;
            }
            m_capacity = capacity;
        }
    }

private:
    size_t m_capacity;
    size_t m_size;
};

QT_END_NAMESPACE

#endif // ARRAY_H
