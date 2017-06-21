/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef CSSComputedStyleDeclaration_h
#define CSSComputedStyleDeclaration_h

#include "core/CoreExport.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/style/ComputedStyleConstants.h"
#include "wtf/HashMap.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSPrimitiveValue;
class CSSValueList;
class ExceptionState;
class MutableStylePropertySet;
class Node;
class LayoutObject;
class ComputedStyle;
class ShadowData;
class ShadowList;
class StyleColor;
class StylePropertyShorthand;

class CORE_EXPORT CSSComputedStyleDeclaration final : public CSSStyleDeclaration {
public:
    static PassRefPtrWillBeRawPtr<CSSComputedStyleDeclaration> create(PassRefPtrWillBeRawPtr<Node> node, bool allowVisitedStyle = false, const String& pseudoElementName = String())
    {
        return adoptRefWillBeNoop(new CSSComputedStyleDeclaration(node, allowVisitedStyle, pseudoElementName));
    }
    virtual ~CSSComputedStyleDeclaration();

#if !ENABLE(OILPAN)
    virtual void ref() override;
    virtual void deref() override;
#endif

    String getPropertyValue(CSSPropertyID) const;
    bool getPropertyPriority(CSSPropertyID) const;

    PassRefPtrWillBeRawPtr<MutableStylePropertySet> copyProperties() const;

    PassRefPtrWillBeRawPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    PassRefPtrWillBeRawPtr<CSSValue> getFontSizeCSSValuePreferringKeyword() const;
    bool isMonospaceFont() const;

    PassRefPtrWillBeRawPtr<MutableStylePropertySet> copyPropertiesInSet(const Vector<CSSPropertyID>&) const;

    DECLARE_VIRTUAL_TRACE();

private:
    CSSComputedStyleDeclaration(PassRefPtrWillBeRawPtr<Node>, bool allowVisitedStyle, const String&);

    // The styled node is either the node passed into getComputedStyle, or the
    // PseudoElement for :before and :after if they exist.
    // FIXME: This should be styledElement since in JS getComputedStyle only works
    // on Elements, but right now editing creates these for text nodes. We should fix
    // that.
    Node* styledNode() const;

    // CSSOM functions. Don't make these public.
    virtual CSSRule* parentRule() const override;
    virtual unsigned length() const override;
    virtual String item(unsigned index) const override;
    const ComputedStyle* computeComputedStyle() const;
    virtual String getPropertyValue(const String& propertyName) override;
    virtual String getPropertyPriority(const String& propertyName) override;
    virtual String getPropertyShorthand(const String& propertyName) override;
    virtual bool isPropertyImplicit(const String& propertyName) override;
    virtual void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionState&) override;
    virtual String removeProperty(const String& propertyName, ExceptionState&) override;
    virtual String cssText() const override;
    virtual void setCSSText(const String&, ExceptionState&) override;
    virtual PassRefPtrWillBeRawPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) override;
    virtual String getPropertyValueInternal(CSSPropertyID) override;
    virtual void setPropertyInternal(CSSPropertyID, const String& value, bool important, ExceptionState&) override;

    virtual bool cssPropertyMatches(CSSPropertyID, const CSSValue*) const override;

    RefPtrWillBeMember<Node> m_node;
    PseudoId m_pseudoElementSpecifier;
    bool m_allowVisitedStyle;
#if !ENABLE(OILPAN)
    unsigned m_refCount;
#endif
};

} // namespace blink

#endif // CSSComputedStyleDeclaration_h
