/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "core/css/FontFaceSet.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFaceCache.h"
#include "core/css/FontFaceSetLoadEvent.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/style/StyleInheritedData.h"
#include "public/platform/Platform.h"

namespace blink {

static const int defaultFontSize = 10;
static const char defaultFontFamily[] = "sans-serif";

class LoadFontPromiseResolver final : public FontFace::LoadFontCallback {
public:
    static PassRefPtrWillBeRawPtr<LoadFontPromiseResolver> create(FontFaceArray faces, ScriptState* scriptState)
    {
        return adoptRefWillBeNoop(new LoadFontPromiseResolver(faces, scriptState));
    }

    void loadFonts(ExecutionContext*);
    ScriptPromise promise() { return m_resolver->promise(); }

    virtual void notifyLoaded(FontFace*) override;
    virtual void notifyError(FontFace*) override;

    DECLARE_VIRTUAL_TRACE();

private:
    LoadFontPromiseResolver(FontFaceArray faces, ScriptState* scriptState)
        : m_numLoading(faces.size())
        , m_errorOccured(false)
        , m_resolver(ScriptPromiseResolver::create(scriptState))
    {
        m_fontFaces.swap(faces);
    }

    WillBeHeapVector<RefPtrWillBeMember<FontFace>> m_fontFaces;
    int m_numLoading;
    bool m_errorOccured;
    RefPtrWillBeMember<ScriptPromiseResolver> m_resolver;
};

void LoadFontPromiseResolver::loadFonts(ExecutionContext* context)
{
    if (!m_numLoading) {
        m_resolver->resolve(m_fontFaces);
        return;
    }

    for (size_t i = 0; i < m_fontFaces.size(); i++)
        m_fontFaces[i]->loadWithCallback(this, context);
}

void LoadFontPromiseResolver::notifyLoaded(FontFace* fontFace)
{
    m_numLoading--;
    if (m_numLoading || m_errorOccured)
        return;

    m_resolver->resolve(m_fontFaces);
}

void LoadFontPromiseResolver::notifyError(FontFace* fontFace)
{
    m_numLoading--;
    if (!m_errorOccured) {
        m_errorOccured = true;
        m_resolver->reject(fontFace->error());
    }
}

DEFINE_TRACE(LoadFontPromiseResolver)
{
    visitor->trace(m_fontFaces);
    visitor->trace(m_resolver);
    LoadFontCallback::trace(visitor);
}

class FontsReadyPromiseResolver final : public NoBaseWillBeGarbageCollected<FontsReadyPromiseResolver> {
public:
    static PassOwnPtrWillBeRawPtr<FontsReadyPromiseResolver> create(ScriptState* scriptState)
    {
        return adoptPtrWillBeNoop(new FontsReadyPromiseResolver(scriptState));
    }

    void resolve(PassRefPtrWillBeRawPtr<FontFaceSet> fontFaceSet)
    {
        m_resolver->resolve(fontFaceSet);
    }

    ScriptPromise promise() { return m_resolver->promise(); }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_resolver);
    }

private:
    explicit FontsReadyPromiseResolver(ScriptState* scriptState)
        : m_resolver(ScriptPromiseResolver::create(scriptState))
    {
    }

    RefPtrWillBeMember<ScriptPromiseResolver> m_resolver;
};

FontFaceSet::FontFaceSet(Document& document)
    : ActiveDOMObject(&document)
    , m_shouldFireLoadingEvent(false)
    , m_asyncRunner(this, &FontFaceSet::handlePendingEventsAndPromises)
{
    suspendIfNeeded();
}

FontFaceSet::~FontFaceSet()
{
}

Document* FontFaceSet::document() const
{
    return toDocument(executionContext());
}

bool FontFaceSet::inActiveDocumentContext() const
{
    ExecutionContext* context = executionContext();
    return context && toDocument(context)->isActive();
}

void FontFaceSet::addFontFacesToFontFaceCache(FontFaceCache* fontFaceCache, CSSFontSelector* fontSelector)
{
    for (const auto& fontFace : m_nonCSSConnectedFaces)
        fontFaceCache->addFontFace(fontSelector, fontFace, false);
}

const AtomicString& FontFaceSet::interfaceName() const
{
    return EventTargetNames::FontFaceSet;
}

ExecutionContext* FontFaceSet::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

AtomicString FontFaceSet::status() const
{
    DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded", AtomicString::ConstructFromLiteral));
    return (!m_loadingFonts.isEmpty() || hasLoadedFonts()) ? loading : loaded;
}

void FontFaceSet::handlePendingEventsAndPromisesSoon()
{
    // m_asyncRunner will be automatically stopped on destruction.
    m_asyncRunner.runAsync();
}

void FontFaceSet::didLayout()
{
    if (document()->frame()->isMainFrame() && m_loadingFonts.isEmpty())
        m_histogram.record();
    if (!m_loadingFonts.isEmpty() || (!hasLoadedFonts() && m_readyResolvers.isEmpty()))
        return;
    handlePendingEventsAndPromisesSoon();
}

void FontFaceSet::handlePendingEventsAndPromises()
{
    fireLoadingEvent();
    fireDoneEventIfPossible();
}

void FontFaceSet::fireLoadingEvent()
{
    if (m_shouldFireLoadingEvent) {
        m_shouldFireLoadingEvent = false;
        dispatchEvent(FontFaceSetLoadEvent::createForFontFaces(EventTypeNames::loading));
    }
}

void FontFaceSet::suspend()
{
    m_asyncRunner.suspend();
}

void FontFaceSet::resume()
{
    m_asyncRunner.resume();
}

void FontFaceSet::stop()
{
    m_asyncRunner.stop();
}

void FontFaceSet::beginFontLoading(FontFace* fontFace)
{
    m_histogram.incrementCount();
    addToLoadingFonts(fontFace);
}

void FontFaceSet::fontLoaded(FontFace* fontFace)
{
    m_histogram.updateStatus(fontFace);
    m_loadedFonts.append(fontFace);
    removeFromLoadingFonts(fontFace);
}

void FontFaceSet::loadError(FontFace* fontFace)
{
    m_histogram.updateStatus(fontFace);
    m_failedFonts.append(fontFace);
    removeFromLoadingFonts(fontFace);
}

void FontFaceSet::addToLoadingFonts(PassRefPtrWillBeRawPtr<FontFace> fontFace)
{
    if (m_loadingFonts.isEmpty() && !hasLoadedFonts()) {
        m_shouldFireLoadingEvent = true;
        handlePendingEventsAndPromisesSoon();
    }
    m_loadingFonts.add(fontFace);
}

void FontFaceSet::removeFromLoadingFonts(PassRefPtrWillBeRawPtr<FontFace> fontFace)
{
    m_loadingFonts.remove(fontFace);
    if (m_loadingFonts.isEmpty())
        handlePendingEventsAndPromisesSoon();
}

ScriptPromise FontFaceSet::ready(ScriptState* scriptState)
{
    if (!inActiveDocumentContext())
        return ScriptPromise();
    OwnPtrWillBeRawPtr<FontsReadyPromiseResolver> resolver = FontsReadyPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    m_readyResolvers.append(resolver.release());
    handlePendingEventsAndPromisesSoon();
    return promise;
}

void FontFaceSet::add(FontFace* fontFace, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return;
    if (!fontFace) {
        exceptionState.throwTypeError("The argument is not a FontFace.");
        return;
    }
    if (m_nonCSSConnectedFaces.contains(fontFace))
        return;
    if (isCSSConnectedFontFace(fontFace)) {
        exceptionState.throwDOMException(InvalidModificationError, "Cannot add a CSS-connected FontFace.");
        return;
    }
    CSSFontSelector* fontSelector = document()->styleEngine().fontSelector();
    m_nonCSSConnectedFaces.add(fontFace);
    fontSelector->fontFaceCache()->addFontFace(fontSelector, fontFace, false);
    if (fontFace->loadStatus() == FontFace::Loading)
        addToLoadingFonts(fontFace);
    fontSelector->fontFaceInvalidated();
}

void FontFaceSet::clear()
{
    if (!inActiveDocumentContext() || m_nonCSSConnectedFaces.isEmpty())
        return;
    CSSFontSelector* fontSelector = document()->styleEngine().fontSelector();
    FontFaceCache* fontFaceCache = fontSelector->fontFaceCache();
    for (const auto& fontFace : m_nonCSSConnectedFaces) {
        fontFaceCache->removeFontFace(fontFace.get(), false);
        if (fontFace->loadStatus() == FontFace::Loading)
            removeFromLoadingFonts(fontFace);
    }
    m_nonCSSConnectedFaces.clear();
    fontSelector->fontFaceInvalidated();
}

bool FontFaceSet::remove(FontFace* fontFace, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return false;
    if (!fontFace) {
        exceptionState.throwTypeError("The argument is not a FontFace.");
        return false;
    }
    WillBeHeapListHashSet<RefPtrWillBeMember<FontFace>>::iterator it = m_nonCSSConnectedFaces.find(fontFace);
    if (it != m_nonCSSConnectedFaces.end()) {
        m_nonCSSConnectedFaces.remove(it);
        CSSFontSelector* fontSelector = document()->styleEngine().fontSelector();
        fontSelector->fontFaceCache()->removeFontFace(fontFace, false);
        if (fontFace->loadStatus() == FontFace::Loading)
            removeFromLoadingFonts(fontFace);
        fontSelector->fontFaceInvalidated();
        return true;
    }
    if (isCSSConnectedFontFace(fontFace))
        exceptionState.throwDOMException(InvalidModificationError, "Cannot delete a CSS-connected FontFace.");
    return false;
}

bool FontFaceSet::has(FontFace* fontFace, ExceptionState& exceptionState) const
{
    if (!inActiveDocumentContext())
        return false;
    if (!fontFace) {
        exceptionState.throwTypeError("The argument is not a FontFace.");
        return false;
    }
    return m_nonCSSConnectedFaces.contains(fontFace) || isCSSConnectedFontFace(fontFace);
}

const WillBeHeapListHashSet<RefPtrWillBeMember<FontFace>>& FontFaceSet::cssConnectedFontFaceList() const
{
    Document* d = document();
    d->ensureStyleResolver(); // Flush pending style changes.
    return d->styleEngine().fontSelector()->fontFaceCache()->cssConnectedFontFaces();
}

bool FontFaceSet::isCSSConnectedFontFace(FontFace* fontFace) const
{
    return cssConnectedFontFaceList().contains(fontFace);
}

void FontFaceSet::forEach(FontFaceSetForEachCallback* callback, const ScriptValue& thisArg) const
{
    forEachInternal(callback, &thisArg);
}

void FontFaceSet::forEach(FontFaceSetForEachCallback* callback) const
{
    forEachInternal(callback, 0);
}

void FontFaceSet::forEachInternal(FontFaceSetForEachCallback* callback, const ScriptValue* thisArg) const
{
    if (!inActiveDocumentContext())
        return;
    const WillBeHeapListHashSet<RefPtrWillBeMember<FontFace>>& cssConnectedFaces = cssConnectedFontFaceList();
    WillBeHeapVector<RefPtrWillBeMember<FontFace>> fontFaces;
    fontFaces.reserveInitialCapacity(cssConnectedFaces.size() + m_nonCSSConnectedFaces.size());
    for (const auto& fontFace : cssConnectedFaces)
        fontFaces.append(fontFace);
    for (const auto& fontFace : m_nonCSSConnectedFaces)
        fontFaces.append(fontFace);

    for (size_t i = 0; i < fontFaces.size(); ++i) {
        FontFace* face = fontFaces[i].get();
        if (thisArg)
            callback->handleItem(*thisArg, face, face, const_cast<FontFaceSet*>(this));
        else
            callback->handleItem(face, face, const_cast<FontFaceSet*>(this));
    }
}

unsigned long FontFaceSet::size() const
{
    if (!inActiveDocumentContext())
        return m_nonCSSConnectedFaces.size();
    return cssConnectedFontFaceList().size() + m_nonCSSConnectedFaces.size();
}

void FontFaceSet::fireDoneEventIfPossible()
{
    if (m_shouldFireLoadingEvent)
        return;
    if (!m_loadingFonts.isEmpty() || (!hasLoadedFonts() && m_readyResolvers.isEmpty()))
        return;

    // If the layout was invalidated in between when we thought layout
    // was updated and when we're ready to fire the event, just wait
    // until after the next layout before firing events.
    Document* d = document();
    if (!d->view() || d->view()->needsLayout())
        return;

    if (hasLoadedFonts()) {
        RefPtrWillBeRawPtr<FontFaceSetLoadEvent> doneEvent = nullptr;
        RefPtrWillBeRawPtr<FontFaceSetLoadEvent> errorEvent = nullptr;
        doneEvent = FontFaceSetLoadEvent::createForFontFaces(EventTypeNames::loadingdone, m_loadedFonts);
        m_loadedFonts.clear();
        if (!m_failedFonts.isEmpty()) {
            errorEvent = FontFaceSetLoadEvent::createForFontFaces(EventTypeNames::loadingerror, m_failedFonts);
            m_failedFonts.clear();
        }
        dispatchEvent(doneEvent);
        if (errorEvent)
            dispatchEvent(errorEvent);
    }

    if (!m_readyResolvers.isEmpty()) {
        WillBeHeapVector<OwnPtrWillBeMember<FontsReadyPromiseResolver>> resolvers;
        m_readyResolvers.swap(resolvers);
        for (size_t index = 0; index < resolvers.size(); ++index)
            resolvers[index]->resolve(this);
    }
}

ScriptPromise FontFaceSet::load(ScriptState* scriptState, const String& fontString, const String& text)
{
    if (!inActiveDocumentContext())
        return ScriptPromise();

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
        ScriptPromise promise = resolver->promise();
        resolver->reject(DOMException::create(SyntaxError, "Could not resolve '" + fontString + "' as a font."));
        return promise;
    }

    FontFaceCache* fontFaceCache = document()->styleEngine().fontSelector()->fontFaceCache();
    FontFaceArray faces;
    for (const FontFamily* f = &font.fontDescription().family(); f; f = f->next()) {
        CSSSegmentedFontFace* segmentedFontFace = fontFaceCache->get(font.fontDescription(), f->family());
        if (segmentedFontFace)
            segmentedFontFace->match(text, faces);
    }

    RefPtrWillBeRawPtr<LoadFontPromiseResolver> resolver = LoadFontPromiseResolver::create(faces, scriptState);
    ScriptPromise promise = resolver->promise();
    resolver->loadFonts(executionContext()); // After this, resolver->promise() may return null.
    return promise;
}

bool FontFaceSet::check(const String& fontString, const String& text, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return false;

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        exceptionState.throwDOMException(SyntaxError, "Could not resolve '" + fontString + "' as a font.");
        return false;
    }

    CSSFontSelector* fontSelector = document()->styleEngine().fontSelector();
    FontFaceCache* fontFaceCache = fontSelector->fontFaceCache();

    bool hasLoadedFaces = false;
    for (const FontFamily* f = &font.fontDescription().family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = fontFaceCache->get(font.fontDescription(), f->family());
        if (face) {
            if (!face->checkFont(text))
                return false;
            hasLoadedFaces = true;
        }
    }
    if (hasLoadedFaces)
        return true;
    for (const FontFamily* f = &font.fontDescription().family(); f; f = f->next()) {
        if (fontSelector->isPlatformFontAvailable(font.fontDescription(), f->family()))
            return true;
    }
    return false;
}

bool FontFaceSet::resolveFontStyle(const String& fontString, Font& font)
{
    if (fontString.isEmpty())
        return false;

    // Interpret fontString in the same way as the 'font' attribute of CanvasRenderingContext2D.
    RefPtrWillBeRawPtr<MutableStylePropertySet> parsedStyle = MutableStylePropertySet::create();
    CSSParser::parseValue(parsedStyle.get(), CSSPropertyFont, fontString, true, HTMLStandardMode, 0);
    if (parsedStyle->isEmpty())
        return false;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);
    if (fontValue == "inherit" || fontValue == "initial")
        return false;

    RefPtr<ComputedStyle> style = ComputedStyle::create();

    FontFamily fontFamily;
    fontFamily.setFamily(defaultFontFamily);

    FontDescription defaultFontDescription;
    defaultFontDescription.setFamily(fontFamily);
    defaultFontDescription.setSpecifiedSize(defaultFontSize);
    defaultFontDescription.setComputedSize(defaultFontSize);

    style->setFontDescription(defaultFontDescription);

    style->font().update(style->font().fontSelector());

    document()->updateLayoutTreeIfNeeded();
    document()->ensureStyleResolver().computeFont(style.get(), *parsedStyle);

    font = style->font();
    font.update(document()->styleEngine().fontSelector());
    return true;
}

void FontFaceSet::FontLoadHistogram::updateStatus(FontFace* fontFace)
{
    if (m_status == Reported)
        return;
    if (fontFace->hadBlankText())
        m_status = HadBlankText;
    else if (m_status == NoWebFonts)
        m_status = DidNotHaveBlankText;
}

void FontFaceSet::FontLoadHistogram::record()
{
    if (!m_recorded) {
        m_recorded = true;
        Platform::current()->histogramCustomCounts("WebFont.WebFontsInPage", m_count, 1, 100, 50);
    }
    if (m_status == HadBlankText || m_status == DidNotHaveBlankText) {
        Platform::current()->histogramEnumeration("WebFont.HadBlankText", m_status == HadBlankText ? 1 : 0, 2);
        m_status = Reported;
    }
}

static const char* supplementName()
{
    return "FontFaceSet";
}

PassRefPtrWillBeRawPtr<FontFaceSet> FontFaceSet::from(Document& document)
{
    RefPtrWillBeRawPtr<FontFaceSet> fonts = static_cast<FontFaceSet*>(SupplementType::from(document, supplementName()));
    if (!fonts) {
        fonts = FontFaceSet::create(document);
        SupplementType::provideTo(document, supplementName(), fonts);
    }

    return fonts.release();
}

void FontFaceSet::didLayout(Document& document)
{
    if (FontFaceSet* fonts = static_cast<FontFaceSet*>(SupplementType::from(document, supplementName())))
        fonts->didLayout();
}

DEFINE_TRACE(FontFaceSet)
{
#if ENABLE(OILPAN)
    visitor->trace(m_loadingFonts);
    visitor->trace(m_readyResolvers);
    visitor->trace(m_loadedFonts);
    visitor->trace(m_failedFonts);
    visitor->trace(m_nonCSSConnectedFaces);
    WillBeHeapSupplement<Document>::trace(visitor);
#endif
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
