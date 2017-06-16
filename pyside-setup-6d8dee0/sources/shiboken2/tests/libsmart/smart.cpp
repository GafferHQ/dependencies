/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "smart.h"

bool shouldPrint() {
    return Registry::getInstance()->shouldPrint();
}

Obj::Obj() : m_integer(123), m_internalInteger(new Integer)
{
    Registry::getInstance()->add(this);
    if (shouldPrint())
        std::cout << "Object constructor " << this << '\n';
}

Obj::~Obj()
{
    Registry::getInstance()->remove(this);
    delete m_internalInteger;
    if (shouldPrint())
        std::cout << "Object destructor " << this << '\n';
}


void Obj::printObj() {
    if (shouldPrint()) {
        std::cout << "integer value: " << m_integer
                  << " internal integer value: " << m_internalInteger->m_int << '\n';
    }
}


SharedPtr<Obj> Obj::giveSharedPtrToObj()
{
    SharedPtr<Obj> o(new Obj);
    return o;
}

SharedPtr<Integer> Obj::giveSharedPtrToInteger()
{
    SharedPtr<Integer> o(new Integer);
    return o;
}

int Obj::takeSharedPtrToObj(SharedPtr<Obj> pObj)
{
    pObj->printObj();
    return pObj->m_integer;
}

int Obj::takeSharedPtrToInteger(SharedPtr<Integer> pInt)
{
    pInt->printInteger();
    return pInt->m_int;
}

Integer Obj::takeInteger(Integer val)
{
    return val;
}

Integer::Integer() : m_int(456)
{
    Registry::getInstance()->add(this);
    if (shouldPrint())
        std::cout << "Integer constructor " << this << '\n';
}

Integer::Integer(const Integer &other)
{
    Registry::getInstance()->add(this);
    if (shouldPrint())
        std::cout << "Integer copy constructor " << this << '\n';
    m_int = other.m_int;
}

Integer &Integer::operator=(const Integer &other)
{
    Registry::getInstance()->add(this);
    if (shouldPrint())
        std::cout << "Integer operator= " << this << '\n';
    m_int = other.m_int;
    return *this;
}

Integer::~Integer()
{
    Registry::getInstance()->remove(this);
    if (shouldPrint())
        std::cout << "Integer destructor " << this << '\n';
}

void Integer::printInteger()
{
    if (shouldPrint())
        std::cout << "Integer value for object " << this << " is " << m_int << '\n';
}

Registry *Registry::getInstance()
{
    static Registry registry;
    return &registry;
}

Registry::Registry() : m_printStuff(false)
{

}

void Registry::add(Obj *p)
{
    m_objects.push_back(p);
}

void Registry::add(Integer *p)
{
    m_integers.push_back(p);
}

void Registry::remove(Obj *p)
{
    m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), p), m_objects.end());
}

void Registry::remove(Integer *p)
{
    m_integers.erase(std::remove(m_integers.begin(), m_integers.end(), p), m_integers.end());
}

int Registry::countObjects() const
{
    return static_cast<int>(m_objects.size());
}

int Registry::countIntegers() const
{
    return static_cast<int>(m_integers.size());
}

bool Registry::shouldPrint() const
{
    return m_printStuff;
}

void Registry::setShouldPrint(bool flag)
{
    m_printStuff = flag;
}
