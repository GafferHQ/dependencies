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

#ifndef Q3PTRSTACK_H
#define Q3PTRSTACK_H

#include <Qt3Support/q3glist.h>

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Qt3SupportLight)

template<class type>
class Q3PtrStack : protected Q3GList
{
public:
    Q3PtrStack()				{ }
    Q3PtrStack( const Q3PtrStack<type> &s ) : Q3GList( s ) { }
    ~Q3PtrStack()			{ clear(); }
    Q3PtrStack<type> &operator=(const Q3PtrStack<type> &s)
			{ return (Q3PtrStack<type>&)Q3GList::operator=(s); }
    bool  autoDelete() const		{ return Q3PtrCollection::autoDelete(); }
    void  setAutoDelete( bool del )	{ Q3PtrCollection::setAutoDelete(del); }
    uint  count()   const		{ return Q3GList::count(); }
    bool  isEmpty() const		{ return Q3GList::count() == 0; }
    void  push( const type *d )		{ Q3GList::insertAt(0,Item(d)); }
    type *pop()				{ return (type *)Q3GList::takeFirst(); }
    bool  remove()			{ return Q3GList::removeFirst(); }
    void  clear()			{ Q3GList::clear(); }
    type *top()	    const		{ return (type *)Q3GList::cfirst(); }
	  operator type *() const	{ return (type *)Q3GList::cfirst(); }
    type *current() const		{ return (type *)Q3GList::cfirst(); }

#ifdef qdoc
protected:
    virtual QDataStream& read( QDataStream&, Q3PtrCollection::Item& );
    virtual QDataStream& write( QDataStream&, Q3PtrCollection::Item ) const;
#endif

private:
    void  deleteItem( Item d );
};

#if !defined(Q_BROKEN_TEMPLATE_SPECIALIZATION)
template<> inline void Q3PtrStack<void>::deleteItem( Q3PtrCollection::Item )
{
}
#endif

template<class type> inline void Q3PtrStack<type>::deleteItem( Q3PtrCollection::Item d )
{
    if ( del_item ) delete (type *)d;
}

QT_END_NAMESPACE

QT_END_HEADER

#endif // Q3PTRSTACK_H
