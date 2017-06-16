/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/fonts/GlyphPageTreeNode.h"

#include "platform/fonts/SegmentedFontData.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/opentype/OpenTypeVerticalData.h"
#include "wtf/text/CString.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/WTFString.h"
#include <stdio.h>

namespace blink {

using std::max;
using std::min;

HashMap<int, GlyphPageTreeNode*>* GlyphPageTreeNode::roots = 0;
GlyphPageTreeNode* GlyphPageTreeNode::pageZeroRoot = 0;

GlyphPageTreeNode* GlyphPageTreeNode::getRoot(unsigned pageNumber)
{
    static bool initialized;
    if (!initialized) {
        initialized = true;
        roots = new HashMap<int, GlyphPageTreeNode*>;
        pageZeroRoot = new GlyphPageTreeNode();
    }

    if (!pageNumber)
        return pageZeroRoot;

    if (GlyphPageTreeNode* foundNode = roots->get(pageNumber))
        return foundNode;

    GlyphPageTreeNode* node = new GlyphPageTreeNode();
#if ENABLE(ASSERT)
    node->m_pageNumber = pageNumber;
#endif
    roots->set(pageNumber, node);
    return node;
}

size_t GlyphPageTreeNode::treeGlyphPageCount()
{
    size_t count = 0;
    if (roots) {
        HashMap<int, GlyphPageTreeNode*>::iterator end = roots->end();
        for (HashMap<int, GlyphPageTreeNode*>::iterator it = roots->begin(); it != end; ++it)
            count += it->value->pageCount();
    }

    if (pageZeroRoot)
        count += pageZeroRoot->pageCount();

    return count;
}

size_t GlyphPageTreeNode::pageCount() const
{
    size_t count = m_page && m_page->owner() == this ? 1 : 0;

    GlyphPageTreeNodeMap::const_iterator end = m_children.end();
    for (GlyphPageTreeNodeMap::const_iterator it = m_children.begin(); it != end; ++it)
        count += it->value->pageCount();

    if (m_systemFallbackChild)
        ++count;

    return count;
}

void GlyphPageTreeNode::pruneTreeCustomFontData(const FontData* fontData)
{
    // Enumerate all the roots and prune any tree that contains our custom font data.
    if (roots) {
        HashMap<int, GlyphPageTreeNode*>::iterator end = roots->end();
        for (HashMap<int, GlyphPageTreeNode*>::iterator it = roots->begin(); it != end; ++it)
            it->value->pruneCustomFontData(fontData);
    }

    if (pageZeroRoot)
        pageZeroRoot->pruneCustomFontData(fontData);
}

void GlyphPageTreeNode::pruneTreeFontData(const SimpleFontData* fontData)
{
    if (roots) {
        HashMap<int, GlyphPageTreeNode*>::iterator end = roots->end();
        for (HashMap<int, GlyphPageTreeNode*>::iterator it = roots->begin(); it != end; ++it)
            it->value->pruneFontData(fontData);
    }

    if (pageZeroRoot)
        pageZeroRoot->pruneFontData(fontData);
}

static bool fill(GlyphPage* pageToFill, unsigned offset, unsigned length, UChar* buffer, unsigned bufferLength, const SimpleFontData* fontData)
{
    bool hasGlyphs = fontData->fillGlyphPage(pageToFill, offset, length, buffer, bufferLength);
    return hasGlyphs;
}

void GlyphPageTreeNode::initializePage(const FontData* fontData, unsigned pageNumber)
{
    ASSERT(!m_page);
    ASSERT(fontData);

    // This function must not be called for the root of the tree, because that
    // level does not contain any glyphs.
    ASSERT(m_level > 0 && m_parent);

    if (m_level == 1) {
        // Children of the root hold pure pages. These will cover only one
        // font data's glyphs, and will have glyph index 0 if the font data does not
        // contain the glyph.
        initializePurePage(fontData, pageNumber);
        return;
    }

    // The parent's page will be 0 if we are level one or the parent's font data
    // did not contain any glyphs for that page.
    GlyphPage* parentPage = m_parent->page();

    if (parentPage && parentPage->owner() != m_parent) {
        // The page we're overriding may not be owned by our parent node.
        // This happens when our parent node provides no useful overrides
        // and just copies the pointer to an already-existing page (see
        // below).
        //
        // We want our override to be shared by all nodes that reference
        // that page to avoid duplication, and so standardize on having the
        // page's owner collect all the overrides.  Call getChild on the
        // page owner with the desired font data (this will populate
        // the page) and then reference it.
        m_page = static_cast<GlyphPageTreeNode*>(parentPage->owner())->getNormalChild(fontData, pageNumber)->page();
        return;
    }

    initializeOverridePage(fontData, pageNumber);
}

void GlyphPageTreeNode::initializePurePage(const FontData* fontData, unsigned pageNumber)
{
    ASSERT(m_level == 1);

    unsigned start = pageNumber * GlyphPage::size;
    UChar buffer[GlyphPage::size * 2 + 2];
    unsigned bufferLength;
    unsigned i;

    // Fill in a buffer with the entire "page" of characters that we want to look up glyphs for.
    if (start < 0x10000) {
        bufferLength = GlyphPage::size;
        for (i = 0; i < GlyphPage::size; i++)
            buffer[i] = start + i;

        if (start == 0) {
            // Control characters must not render at all.
            for (i = 0; i < 0x20; ++i)
                buffer[i] = zeroWidthSpaceCharacter;
            for (i = 0x7F; i < 0xA0; i++)
                buffer[i] = zeroWidthSpaceCharacter;
            buffer[softHyphenCharacter] = zeroWidthSpaceCharacter;

            // \n and \t must render as a spaceCharacter.
            buffer[newlineCharacter] = spaceCharacter;
            buffer[tabulationCharacter] = spaceCharacter;
        } else if (start == (arabicLetterMarkCharacter & ~(GlyphPage::size - 1))) {
            buffer[arabicLetterMarkCharacter - start] = zeroWidthSpaceCharacter;
        } else if (start == (leftToRightMarkCharacter & ~(GlyphPage::size - 1))) {
            // LRM, RLM, LRE, RLE, ZWNJ, ZWJ, and PDF must not render at all.
            buffer[leftToRightMarkCharacter - start] = zeroWidthSpaceCharacter;
            buffer[rightToLeftMarkCharacter - start] = zeroWidthSpaceCharacter;
            buffer[leftToRightEmbedCharacter - start] = zeroWidthSpaceCharacter;
            buffer[rightToLeftEmbedCharacter - start] = zeroWidthSpaceCharacter;
            buffer[leftToRightOverrideCharacter - start] = zeroWidthSpaceCharacter;
            buffer[rightToLeftOverrideCharacter - start] = zeroWidthSpaceCharacter;
            buffer[zeroWidthNonJoinerCharacter - start] = zeroWidthSpaceCharacter;
            buffer[zeroWidthJoinerCharacter - start] = zeroWidthSpaceCharacter;
            buffer[popDirectionalFormattingCharacter - start] = zeroWidthSpaceCharacter;
            buffer[activateArabicFormShapingCharacter - start] = zeroWidthSpaceCharacter;
            buffer[activateSymmetricSwappingCharacter - start] = zeroWidthSpaceCharacter;
            buffer[firstStrongIsolateCharacter - start] = zeroWidthSpaceCharacter;
            buffer[inhibitArabicFormShapingCharacter - start] = zeroWidthSpaceCharacter;
            buffer[inhibitSymmetricSwappingCharacter - start] = zeroWidthSpaceCharacter;
            buffer[leftToRightIsolateCharacter - start] = zeroWidthSpaceCharacter;
            buffer[nationalDigitShapesCharacter - start] = zeroWidthSpaceCharacter;
            buffer[nominalDigitShapesCharacter - start] = zeroWidthSpaceCharacter;
            buffer[popDirectionalIsolateCharacter - start] = zeroWidthSpaceCharacter;
            buffer[rightToLeftIsolateCharacter - start] = zeroWidthSpaceCharacter;
        } else if (start == (objectReplacementCharacter & ~(GlyphPage::size - 1))) {
            // Object replacement character must not render at all.
            buffer[objectReplacementCharacter - start] = zeroWidthSpaceCharacter;
        } else if (start == (zeroWidthNoBreakSpaceCharacter & ~(GlyphPage::size - 1))) {
            // ZWNBS/BOM must not render at all.
            buffer[zeroWidthNoBreakSpaceCharacter - start] = zeroWidthSpaceCharacter;
        }
    } else {
        bufferLength = GlyphPage::size * 2;
        for (i = 0; i < GlyphPage::size; i++) {
            int c = i + start;
            buffer[i * 2] = U16_LEAD(c);
            buffer[i * 2 + 1] = U16_TRAIL(c);
        }
    }

    // Now that we have a buffer full of characters, we want to get back an array
    // of glyph indices.  This part involves calling into the platform-specific
    // routine of our glyph map for actually filling in the page with the glyphs.
    // Success is not guaranteed. For example, Times fails to fill page 260, giving glyph data
    // for only 128 out of 256 characters.
    bool haveGlyphs;
    if (!fontData->isSegmented()) {
        m_page = GlyphPage::createForSingleFontData(this, toSimpleFontData(fontData));
        haveGlyphs = fill(m_page.get(), 0, GlyphPage::size, buffer, bufferLength, toSimpleFontData(fontData));
    } else {
        m_page = GlyphPage::createForMixedFontData(this);
        haveGlyphs = false;

        const SegmentedFontData* segmentedFontData = toSegmentedFontData(fontData);
        for (int i = segmentedFontData->numRanges() - 1; i >= 0; i--) {
            const FontDataRange& range = segmentedFontData->rangeAt(i);
            // all this casting is to ensure all the parameters to min and max have the same type,
            // to avoid ambiguous template parameter errors on Windows
            int from = max(0, static_cast<int>(range.from()) - static_cast<int>(start));
            int to = 1 + min(static_cast<int>(range.to()) - static_cast<int>(start), static_cast<int>(GlyphPage::size) - 1);
            if (from >= static_cast<int>(GlyphPage::size) || to <= 0)
                continue;

            // If this is a custom font needs to be loaded, do not fill
            // the page so that font fallback is used while loading.
            RefPtr<CustomFontData> customData = range.fontData()->customFontData();
            if (customData && customData->isLoadingFallback()) {
                for (int j = from; j < to; j++) {
                    m_page->setCustomFontToLoad(j, customData.get());
                    haveGlyphs = true;
                }
                continue;
            }

            haveGlyphs |= fill(m_page.get(), from, to - from, buffer + from * (start < 0x10000 ? 1 : 2), (to - from) * (start < 0x10000 ? 1 : 2), range.fontData().get());
        }
    }

    if (!haveGlyphs)
        m_page = nullptr;
}

void GlyphPageTreeNode::initializeOverridePage(const FontData* fontData, unsigned pageNumber)
{
    GlyphPage* parentPage = m_parent->page();

    // Get the pure page for the fallback font (at level 1 with no
    // overrides). getRootChild will always create a page if one
    // doesn't exist, but the page doesn't necessarily have glyphs
    // (this pointer may be 0).
    GlyphPage* fallbackPage = getNormalRootChild(fontData, pageNumber)->page();
    if (!parentPage) {
        // When the parent has no glyphs for this page, we can easily
        // override it just by supplying the glyphs from our font.
        m_page = fallbackPage;
        return;
    }

    if (!fallbackPage) {
        // When our font has no glyphs for this page, we can just reference the
        // parent page.
        m_page = parentPage;
        return;
    }

    // Combine the parent's glyphs and ours to form a new more complete page.
    m_page = GlyphPage::createForMixedFontData(this);

    // Overlay the parent page on the fallback page. Check if the fallback font
    // has added anything.
    bool newGlyphs = false;
    for (unsigned i = 0; i < GlyphPage::size; i++) {
        if (parentPage->glyphAt(i)) {
            m_page->setGlyphDataForIndex(i, parentPage->glyphDataForIndex(i));
        } else if (fallbackPage->glyphAt(i)) {
            m_page->setGlyphDataForIndex(i, fallbackPage->glyphDataForIndex(i));
            newGlyphs = true;
        }

        if (parentPage->customFontToLoadAt(i)) {
            m_page->setCustomFontToLoad(i, parentPage->customFontToLoadAt(i));
        } else if (fallbackPage->customFontToLoadAt(i) && !parentPage->glyphAt(i)) {
            m_page->setCustomFontToLoad(i, fallbackPage->customFontToLoadAt(i));
            newGlyphs = true;
        }
    }

    if (!newGlyphs) {
        // We didn't override anything, so our override is just the parent page.
        m_page = parentPage;
    }
}

GlyphPageTreeNode* GlyphPageTreeNode::getNormalChild(const FontData* fontData, unsigned pageNumber)
{
    ASSERT(fontData);
    ASSERT(pageNumber == m_pageNumber);

    if (GlyphPageTreeNode* foundChild = m_children.get(fontData))
        return foundChild;

    GlyphPageTreeNode* child = new GlyphPageTreeNode(this);
    if (fontData->isCustomFont()) {
        for (GlyphPageTreeNode* curr = this; curr; curr = curr->m_parent)
            curr->m_customFontCount++;
    }

#if ENABLE(ASSERT)
    child->m_pageNumber = m_pageNumber;
#endif
    m_children.set(fontData, adoptPtr(child));
    fontData->setMaxGlyphPageTreeLevel(max(fontData->maxGlyphPageTreeLevel(), child->m_level));
    child->initializePage(fontData, pageNumber);
    return child;
}

SystemFallbackGlyphPageTreeNode* GlyphPageTreeNode::getSystemFallbackChild(unsigned pageNumber)
{
    ASSERT(pageNumber == m_pageNumber);

    if (m_systemFallbackChild)
        return m_systemFallbackChild.get();

    SystemFallbackGlyphPageTreeNode* child = new SystemFallbackGlyphPageTreeNode(this);
    m_systemFallbackChild = adoptPtr(child);
#if ENABLE(ASSERT)
    child->m_pageNumber = m_pageNumber;
#endif
    return child;
}

void GlyphPageTreeNode::pruneCustomFontData(const FontData* fontData)
{
    if (!fontData || !m_customFontCount)
        return;

    // Prune any branch that contains this FontData.
    if (OwnPtr<GlyphPageTreeNode> node = m_children.take(fontData)) {
        if (unsigned customFontCount = node->m_customFontCount + 1) {
            for (GlyphPageTreeNode* curr = this; curr; curr = curr->m_parent)
                curr->m_customFontCount -= customFontCount;
        }
    }

    // Check any branches that remain that still have custom fonts underneath them.
    if (!m_customFontCount)
        return;

    GlyphPageTreeNodeMap::iterator end = m_children.end();
    for (GlyphPageTreeNodeMap::iterator it = m_children.begin(); it != end; ++it)
        it->value->pruneCustomFontData(fontData);
}

void GlyphPageTreeNode::pruneFontData(const SimpleFontData* fontData, unsigned level)
{
    ASSERT(fontData);

    // Prune fall back child (if any) of this font.
    if (m_systemFallbackChild)
        m_systemFallbackChild->pruneFontData(fontData);

    // Prune from m_page if it's a mixed page.
    if (m_page && m_page->hasPerGlyphFontData())
        m_page->removePerGlyphFontData(fontData);

    // Prune any branch that contains this FontData.
    if (OwnPtr<GlyphPageTreeNode> node = m_children.take(fontData)) {
        if (unsigned customFontCount = node->m_customFontCount) {
            for (GlyphPageTreeNode* curr = this; curr; curr = curr->m_parent)
                curr->m_customFontCount -= customFontCount;
        }
    }

    level++;
    if (level > fontData->maxGlyphPageTreeLevel())
        return;

    GlyphPageTreeNodeMap::iterator end = m_children.end();
    for (GlyphPageTreeNodeMap::iterator it = m_children.begin(); it != end; ++it)
        it->value->pruneFontData(fontData, level);
}

GlyphPage* SystemFallbackGlyphPageTreeNode::page(UScriptCode script)
{
    PageByScriptMap::iterator it = m_pagesByScript.find(script);
    if (it != m_pagesByScript.end())
        return it->value.get();

    RefPtr<GlyphPage> newPage = initializePage();
    m_pagesByScript.set(script, newPage);
    return newPage.get();
}

void SystemFallbackGlyphPageTreeNode::pruneFontData(const SimpleFontData* fontData)
{
    PageByScriptMap::iterator end = m_pagesByScript.end();
    for (PageByScriptMap::iterator it = m_pagesByScript.begin(); it != end; ++it)
        it->value->removePerGlyphFontData(fontData);
}

PassRefPtr<GlyphPage> SystemFallbackGlyphPageTreeNode::initializePage()
{
    // System fallback page is initialized with the parent's page, as individual
    // entries may use different fonts depending on character. If the Font
    // ever finds it needs a glyph out of the system fallback page, it will
    // ask the system for the best font to use and fill that glyph in for us.
    if (GlyphPage* parentPage = m_parent->page())
        return parentPage->createCopiedSystemFallbackPage(this);
    return GlyphPage::createForMixedFontData(this);
}

} // namespace blink

