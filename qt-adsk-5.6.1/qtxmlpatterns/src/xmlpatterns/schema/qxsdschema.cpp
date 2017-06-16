/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
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

#include "qxsdschema_p.h"

#include <QtCore/QReadLocker>
#include <QtCore/QWriteLocker>

QT_BEGIN_NAMESPACE

using namespace QPatternist;

XsdSchema::XsdSchema(const NamePool::Ptr &namePool)
    : m_namePool(namePool)
{
}

XsdSchema::~XsdSchema()
{
}

NamePool::Ptr XsdSchema::namePool() const
{
    return m_namePool;
}

void XsdSchema::setTargetNamespace(const QString &targetNamespace)
{
    m_targetNamespace = targetNamespace;
}

QString XsdSchema::targetNamespace() const
{
    return m_targetNamespace;
}

void XsdSchema::addElement(const XsdElement::Ptr &element)
{
    const QWriteLocker locker(&m_lock);

    m_elements.insert(element->name(m_namePool), element);
}

XsdElement::Ptr XsdSchema::element(const QXmlName &name) const
{
    const QReadLocker locker(&m_lock);

    return m_elements.value(name);
}

XsdElement::List XsdSchema::elements() const
{
    const QReadLocker locker(&m_lock);

    return m_elements.values();
}

void XsdSchema::addAttribute(const XsdAttribute::Ptr &attribute)
{
    const QWriteLocker locker(&m_lock);

    m_attributes.insert(attribute->name(m_namePool), attribute);
}

XsdAttribute::Ptr XsdSchema::attribute(const QXmlName &name) const
{
    const QReadLocker locker(&m_lock);

    return m_attributes.value(name);
}

XsdAttribute::List XsdSchema::attributes() const
{
    const QReadLocker locker(&m_lock);

    return m_attributes.values();
}

void XsdSchema::addType(const SchemaType::Ptr &type)
{
    const QWriteLocker locker(&m_lock);

    m_types.insert(type->name(m_namePool), type);
}

SchemaType::Ptr XsdSchema::type(const QXmlName &name) const
{
    const QReadLocker locker(&m_lock);

    return m_types.value(name);
}

SchemaType::List XsdSchema::types() const
{
    const QReadLocker locker(&m_lock);

    return m_types.values();
}

XsdSimpleType::List XsdSchema::simpleTypes() const
{
    QReadLocker locker(&m_lock);

    XsdSimpleType::List retval;

    const SchemaType::List types = m_types.values();
    for (int i = 0; i < types.count(); ++i) {
        if (types.at(i)->isSimpleType() && types.at(i)->isDefinedBySchema())
            retval.append(types.at(i));
    }

    return retval;
}

XsdComplexType::List XsdSchema::complexTypes() const
{
    QReadLocker locker(&m_lock);

    XsdComplexType::List retval;

    const SchemaType::List types = m_types.values();
    for (int i = 0; i < types.count(); ++i) {
        if (types.at(i)->isComplexType() && types.at(i)->isDefinedBySchema())
            retval.append(types.at(i));
    }

    return retval;
}

void XsdSchema::addAnonymousType(const SchemaType::Ptr &type)
{
    const QWriteLocker locker(&m_lock);

    // search for not used anonymous type name
    QXmlName typeName = type->name(m_namePool);
    while (m_anonymousTypes.contains(typeName)) {
        typeName = m_namePool->allocateQName(QString(), QLatin1String("merged_") + m_namePool->stringForLocalName(typeName.localName()), QString());
    }

    m_anonymousTypes.insert(typeName, type);
}

SchemaType::List XsdSchema::anonymousTypes() const
{
    const QReadLocker locker(&m_lock);

    return m_anonymousTypes.values();
}

void XsdSchema::addAttributeGroup(const XsdAttributeGroup::Ptr &group)
{
    const QWriteLocker locker(&m_lock);

    m_attributeGroups.insert(group->name(m_namePool), group);
}

XsdAttributeGroup::Ptr XsdSchema::attributeGroup(const QXmlName name) const
{
    const QReadLocker locker(&m_lock);

    return m_attributeGroups.value(name);
}

XsdAttributeGroup::List XsdSchema::attributeGroups() const
{
    const QReadLocker locker(&m_lock);

    return m_attributeGroups.values();
}

void XsdSchema::addElementGroup(const XsdModelGroup::Ptr &group)
{
    const QWriteLocker locker(&m_lock);

    m_elementGroups.insert(group->name(m_namePool), group);
}

XsdModelGroup::Ptr XsdSchema::elementGroup(const QXmlName &name) const
{
    const QReadLocker locker(&m_lock);

    return m_elementGroups.value(name);
}

XsdModelGroup::List XsdSchema::elementGroups() const
{
    const QReadLocker locker(&m_lock);

    return m_elementGroups.values();
}

void XsdSchema::addNotation(const XsdNotation::Ptr &notation)
{
    const QWriteLocker locker(&m_lock);

    m_notations.insert(notation->name(m_namePool), notation);
}

XsdNotation::Ptr XsdSchema::notation(const QXmlName &name) const
{
    const QReadLocker locker(&m_lock);

    return m_notations.value(name);
}

XsdNotation::List XsdSchema::notations() const
{
    const QReadLocker locker(&m_lock);

    return m_notations.values();
}

void XsdSchema::addIdentityConstraint(const XsdIdentityConstraint::Ptr &constraint)
{
    const QWriteLocker locker(&m_lock);

    m_identityConstraints.insert(constraint->name(m_namePool), constraint);
}

XsdIdentityConstraint::Ptr XsdSchema::identityConstraint(const QXmlName &name) const
{
    const QReadLocker locker(&m_lock);

    return m_identityConstraints.value(name);
}

XsdIdentityConstraint::List XsdSchema::identityConstraints() const
{
    const QReadLocker locker(&m_lock);

    return m_identityConstraints.values();
}

QT_END_NAMESPACE
