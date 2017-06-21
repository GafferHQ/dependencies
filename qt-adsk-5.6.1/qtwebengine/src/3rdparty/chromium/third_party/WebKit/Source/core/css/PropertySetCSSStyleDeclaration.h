/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PropertySetCSSStyleDeclaration_h
#define PropertySetCSSStyleDeclaration_h

#include "core/css/CSSStyleDeclaration.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"

namespace blink {

class CSSRule;
class CSSValue;
class Element;
class ExceptionState;
class MutableStylePropertySet;
class StyleSheetContents;

class AbstractPropertySetCSSStyleDeclaration : public CSSStyleDeclaration {
public:
    virtual Element* parentElement() const { return 0; }
    StyleSheetContents* contextStyleSheet() const;

    DECLARE_VIRTUAL_TRACE();

private:
    virtual CSSRule* parentRule() const override { return 0; }
    virtual unsigned length() const override final;
    virtual String item(unsigned index) const override final;
    virtual String getPropertyValue(const String& propertyName) override final;
    virtual String getPropertyPriority(const String& propertyName) override final;
    virtual String getPropertyShorthand(const String& propertyName) override final;
    virtual bool isPropertyImplicit(const String& propertyName) override final;
    virtual void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionState&) override final;
    virtual String removeProperty(const String& propertyName, ExceptionState&) override final;
    virtual String cssText() const override final;
    virtual void setCSSText(const String&, ExceptionState&) override final;
    virtual PassRefPtrWillBeRawPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) override final;
    virtual String getPropertyValueInternal(CSSPropertyID) override final;
    virtual void setPropertyInternal(CSSPropertyID, const String& value, bool important, ExceptionState&) override final;

    virtual bool cssPropertyMatches(CSSPropertyID, const CSSValue*) const override final;

protected:
    enum MutationType { NoChanges, PropertyChanged };
    virtual void willMutate() { }
    virtual void didMutate(MutationType) { }
    virtual MutableStylePropertySet& propertySet() const = 0;
};

class PropertySetCSSStyleDeclaration : public AbstractPropertySetCSSStyleDeclaration {
public:
    PropertySetCSSStyleDeclaration(MutableStylePropertySet& propertySet) : m_propertySet(&propertySet) { }

#if !ENABLE(OILPAN)
    virtual void ref() override;
    virtual void deref() override;
#endif

    DECLARE_VIRTUAL_TRACE();

protected:
    virtual MutableStylePropertySet& propertySet() const override final { ASSERT(m_propertySet); return *m_propertySet; }

    RawPtrWillBeMember<MutableStylePropertySet> m_propertySet; // Cannot be null
};

class StyleRuleCSSStyleDeclaration : public PropertySetCSSStyleDeclaration
{
public:
    static PassRefPtrWillBeRawPtr<StyleRuleCSSStyleDeclaration> create(MutableStylePropertySet& propertySet, CSSRule* parentRule)
    {
        return adoptRefWillBeNoop(new StyleRuleCSSStyleDeclaration(propertySet, parentRule));
    }

#if !ENABLE(OILPAN)
    void clearParentRule() { m_parentRule = nullptr; }

    virtual void ref() override;
    virtual void deref() override;
#endif

    void reattach(MutableStylePropertySet&);

    DECLARE_VIRTUAL_TRACE();

protected:
    StyleRuleCSSStyleDeclaration(MutableStylePropertySet&, CSSRule*);
    virtual ~StyleRuleCSSStyleDeclaration();

    virtual CSSStyleSheet* parentStyleSheet() const override;

    virtual CSSRule* parentRule() const override { return m_parentRule;  }

    virtual void willMutate() override;
    virtual void didMutate(MutationType) override;

#if !ENABLE(OILPAN)
    unsigned m_refCount;
#endif
    RawPtrWillBeMember<CSSRule> m_parentRule;
};

class InlineCSSStyleDeclaration final : public AbstractPropertySetCSSStyleDeclaration
{
public:
    explicit InlineCSSStyleDeclaration(Element* parentElement)
        : m_parentElement(parentElement)
    {
    }

    DECLARE_VIRTUAL_TRACE();

private:
    virtual MutableStylePropertySet& propertySet() const override;
#if !ENABLE(OILPAN)
    virtual void ref() override;
    virtual void deref() override;
#endif
    virtual CSSStyleSheet* parentStyleSheet() const override;
    virtual Element* parentElement() const override { return m_parentElement; }

    virtual void didMutate(MutationType) override;

    RawPtrWillBeMember<Element> m_parentElement;
};

} // namespace blink

#endif
