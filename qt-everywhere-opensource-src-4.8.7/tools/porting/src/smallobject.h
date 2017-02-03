/****************************************************************************
**
** Copyright (C) 2001-2004 Roberto Raggi
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the qt3to4 porting application of the Qt Toolkit.
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

#ifndef SMALLOBJECT_H
#define SMALLOBJECT_H

#include <QList>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

QT_BEGIN_NAMESPACE

struct block_t
{
    static int N;

    block_t *chain;
    char *data;
    char *ptr;
    char *end;

    inline void init(int block_size = 256)
    {
        ++N;
        chain = 0;
        data = (char*) malloc(block_size);
        ptr = data;
        end = data + block_size;
    }

    inline void init0(int block_size)
    {
        init();
        memset(data, '0', block_size);
    }

    inline void destroy()
    {
        --N;
        if (chain) {
            chain->destroy();
            free(chain);
        }

        free(data);
    }

    inline void *allocate(size_t size, block_t **right_most)
    {
        const unsigned int block_size = 1 << 16;
        if (end < ptr + size) {
            assert( size < block_size );

            if (!chain) {
                chain = (block_t*) malloc(sizeof(block_t));
                Q_ASSERT(chain);
                chain->init(block_size);
            }

            return chain->allocate(size, right_most);
        }

        char *r = ptr;
        ptr += size;

        if (right_most)
            *right_most = this;

        return r;
    }

};

// a stupid memory pool
struct pool
{
    block_t blk;
    block_t *right_most;

    inline pool() { blk.init(); right_most = &blk; }
    inline ~pool() { blk.destroy(); }

    inline void *allocate(size_t size)
    {
        void *ptr = right_most->allocate(size, &right_most);
        Q_ASSERT(ptr);
        return ptr;

    }
    inline void *reallocate(void *old, size_t old_size, size_t size)
    {
        void *alloc = right_most->allocate(size, &right_most);
        memcpy(alloc, old, old_size);
        return alloc;
    }
private:
   Q_DISABLE_COPY(pool)
};

/*
    A memory pool that calls the destructor for each stored item when memory is
    freed, at the expence storing one BaseType* per item.

    Note that allocations for types that does not inherit BaseType
    is not allowed and will result in a crash when the memory is freed.
*/
template <typename BaseType>
class TypedPool
{
public:
    inline TypedPool() { blk.init(); right_most = &blk; }
    inline ~TypedPool()
    {
        foreach(BaseType *item, allocated)
            item->~BaseType();
        blk.destroy();
    }

    inline void *allocate(size_t size)
    {
        void *memory = right_most->allocate(size, &right_most);
        allocated.append(reinterpret_cast<BaseType *>(memory));
        Q_ASSERT(memory);
        return memory;
    }
private:
    Q_DISABLE_COPY(TypedPool)
    block_t blk;
    block_t *right_most;
    QList<BaseType *> allocated;
};

struct SmallObject
{
    static void *operator new(size_t size);
    static void operator delete(void *p);
};

QT_END_NAMESPACE

#endif
