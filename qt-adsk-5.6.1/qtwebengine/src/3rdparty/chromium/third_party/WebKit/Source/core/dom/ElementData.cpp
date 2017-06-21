/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/ElementData.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/QualifiedName.h"
#include "wtf/Vector.h"

namespace blink {

struct SameSizeAsElementData : public RefCountedWillBeGarbageCollectedFinalized<SameSizeAsElementData> {
    unsigned bitfield;
    void* pointers[3];
};

static_assert(sizeof(ElementData) == sizeof(SameSizeAsElementData), "ElementData should stay small");

static size_t sizeForShareableElementDataWithAttributeCount(unsigned count)
{
    return sizeof(ShareableElementData) + sizeof(Attribute) * count;
}

ElementData::ElementData()
    : m_isUnique(true)
    , m_arraySize(0)
    , m_presentationAttributeStyleIsDirty(false)
    , m_styleAttributeIsDirty(false)
    , m_animatedSVGAttributesAreDirty(false)
{
}

ElementData::ElementData(unsigned arraySize)
    : m_isUnique(false)
    , m_arraySize(arraySize)
    , m_presentationAttributeStyleIsDirty(false)
    , m_styleAttributeIsDirty(false)
    , m_animatedSVGAttributesAreDirty(false)
{
}

ElementData::ElementData(const ElementData& other, bool isUnique)
    : m_isUnique(isUnique)
    , m_arraySize(isUnique ? 0 : other.attributes().size())
    , m_presentationAttributeStyleIsDirty(other.m_presentationAttributeStyleIsDirty)
    , m_styleAttributeIsDirty(other.m_styleAttributeIsDirty)
    , m_animatedSVGAttributesAreDirty(other.m_animatedSVGAttributesAreDirty)
    , m_classNames(other.m_classNames)
    , m_idForStyleResolution(other.m_idForStyleResolution)
{
    // NOTE: The inline style is copied by the subclass copy constructor since we don't know what to do with it here.
}

#if ENABLE(OILPAN)
void ElementData::finalizeGarbageCollectedObject()
{
    if (m_isUnique)
        toUniqueElementData(this)->~UniqueElementData();
    else
        toShareableElementData(this)->~ShareableElementData();
}
#else
void ElementData::destroy()
{
    if (m_isUnique)
        delete toUniqueElementData(this);
    else
        delete toShareableElementData(this);
}
#endif

PassRefPtrWillBeRawPtr<UniqueElementData> ElementData::makeUniqueCopy() const
{
    if (isUnique())
        return adoptRefWillBeNoop(new UniqueElementData(toUniqueElementData(*this)));
    return adoptRefWillBeNoop(new UniqueElementData(toShareableElementData(*this)));
}

bool ElementData::isEquivalent(const ElementData* other) const
{
    AttributeCollection attributes = this->attributes();
    if (!other)
        return attributes.isEmpty();

    AttributeCollection otherAttributes = other->attributes();
    if (attributes.size() != otherAttributes.size())
        return false;

    for (const Attribute& attribute : attributes) {
        const Attribute* otherAttr = otherAttributes.find(attribute.name());
        if (!otherAttr || attribute.value() != otherAttr->value())
            return false;
    }
    return true;
}

DEFINE_TRACE(ElementData)
{
    if (m_isUnique)
        toUniqueElementData(this)->traceAfterDispatch(visitor);
    else
        toShareableElementData(this)->traceAfterDispatch(visitor);
}

DEFINE_TRACE_AFTER_DISPATCH(ElementData)
{
    visitor->trace(m_inlineStyle);
}

ShareableElementData::ShareableElementData(const Vector<Attribute>& attributes)
    : ElementData(attributes.size())
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        new (&m_attributeArray[i]) Attribute(attributes[i]);
}

ShareableElementData::~ShareableElementData()
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        m_attributeArray[i].~Attribute();
}

ShareableElementData::ShareableElementData(const UniqueElementData& other)
    : ElementData(other, false)
{
    ASSERT(!other.m_presentationAttributeStyle);

    if (other.m_inlineStyle) {
        m_inlineStyle = other.m_inlineStyle->immutableCopyIfNeeded();
    }

    for (unsigned i = 0; i < m_arraySize; ++i)
        new (&m_attributeArray[i]) Attribute(other.m_attributeVector.at(i));
}

PassRefPtrWillBeRawPtr<ShareableElementData> ShareableElementData::createWithAttributes(const Vector<Attribute>& attributes)
{
#if ENABLE(OILPAN)
    void* slot = Heap::allocate<ElementData>(sizeForShareableElementDataWithAttributeCount(attributes.size()));
#else
    void* slot = WTF::fastMalloc(sizeForShareableElementDataWithAttributeCount(attributes.size()));
#endif
    return adoptRefWillBeNoop(new (slot) ShareableElementData(attributes));
}

UniqueElementData::UniqueElementData()
{
}

UniqueElementData::UniqueElementData(const UniqueElementData& other)
    : ElementData(other, true)
    , m_presentationAttributeStyle(other.m_presentationAttributeStyle)
    , m_attributeVector(other.m_attributeVector)
{
    m_inlineStyle = other.m_inlineStyle ? other.m_inlineStyle->mutableCopy() : nullptr;
}

UniqueElementData::UniqueElementData(const ShareableElementData& other)
    : ElementData(other, true)
{
    // An ShareableElementData should never have a mutable inline StylePropertySet attached.
    ASSERT(!other.m_inlineStyle || !other.m_inlineStyle->isMutable());
    m_inlineStyle = other.m_inlineStyle;

    unsigned length = other.attributes().size();
    m_attributeVector.reserveCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        m_attributeVector.uncheckedAppend(other.m_attributeArray[i]);
}

PassRefPtrWillBeRawPtr<UniqueElementData> UniqueElementData::create()
{
    return adoptRefWillBeNoop(new UniqueElementData);
}

PassRefPtrWillBeRawPtr<ShareableElementData> UniqueElementData::makeShareableCopy() const
{
#if ENABLE(OILPAN)
    void* slot = Heap::allocate<ElementData>(sizeForShareableElementDataWithAttributeCount(m_attributeVector.size()));
#else
    void* slot = WTF::fastMalloc(sizeForShareableElementDataWithAttributeCount(m_attributeVector.size()));
#endif
    return adoptRefWillBeNoop(new (slot) ShareableElementData(*this));
}

DEFINE_TRACE_AFTER_DISPATCH(UniqueElementData)
{
    visitor->trace(m_presentationAttributeStyle);
    ElementData::traceAfterDispatch(visitor);
}

} // namespace blink
