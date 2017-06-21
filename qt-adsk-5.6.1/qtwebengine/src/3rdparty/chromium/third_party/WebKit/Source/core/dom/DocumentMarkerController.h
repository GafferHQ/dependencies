/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef DocumentMarkerController_h
#define DocumentMarkerController_h

#include "core/CoreExport.h"
#include "core/dom/DocumentMarker.h"
#include "core/editing/iterators/TextIterator.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"

namespace blink {

class LayoutPoint;
class LayoutRect;
class Node;
class Range;
class RenderedDocumentMarker;
class Text;

class MarkerRemoverPredicate final {
public:
    explicit MarkerRemoverPredicate(const Vector<String>& words);
    bool operator()(const DocumentMarker&, const Text&) const;

private:
    Vector<String> m_words;
};

class CORE_EXPORT DocumentMarkerController final : public NoBaseWillBeGarbageCollected<DocumentMarkerController> {
    WTF_MAKE_NONCOPYABLE(DocumentMarkerController); WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(DocumentMarkerController);
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(DocumentMarkerController);
public:

    DocumentMarkerController();

    void clear();
    void addMarker(const Position& start, const Position& end, DocumentMarker::MarkerType, const String& description = emptyString(), uint32_t hash = 0);
    void addTextMatchMarker(const Range*, bool activeMatch);

    void copyMarkers(Node* srcNode, unsigned startOffset, int length, Node* dstNode, int delta);

    void prepareForDestruction();
    // When a marker partially overlaps with range, if removePartiallyOverlappingMarkers is true, we completely
    // remove the marker. If the argument is false, we will adjust the span of the marker so that it retains
    // the portion that is outside of the range.
    enum RemovePartiallyOverlappingMarkerOrNot { DoNotRemovePartiallyOverlappingMarker, RemovePartiallyOverlappingMarker };
    void removeMarkers(const Position& start, const Position& end, DocumentMarker::MarkerTypes = DocumentMarker::AllMarkers(), RemovePartiallyOverlappingMarkerOrNot = DoNotRemovePartiallyOverlappingMarker);
    void removeMarkers(Node*, unsigned startOffset, int length, DocumentMarker::MarkerTypes = DocumentMarker::AllMarkers(),  RemovePartiallyOverlappingMarkerOrNot = DoNotRemovePartiallyOverlappingMarker);

    void removeMarkers(DocumentMarker::MarkerTypes = DocumentMarker::AllMarkers());
    void removeMarkers(Node*, DocumentMarker::MarkerTypes = DocumentMarker::AllMarkers());
    void removeMarkers(const MarkerRemoverPredicate& shouldRemoveMarker);
    void repaintMarkers(DocumentMarker::MarkerTypes = DocumentMarker::AllMarkers());
    void shiftMarkers(Node*, unsigned startOffset, int delta);
    void setMarkersActive(Range*, bool);
    void setMarkersActive(Node*, unsigned startOffset, unsigned endOffset, bool);
    bool hasMarkers(Node* node) const { return m_markers.contains(node); }

    DocumentMarker* markerContainingPoint(const LayoutPoint&, DocumentMarker::MarkerType);
    DocumentMarkerVector markersFor(Node*, DocumentMarker::MarkerTypes = DocumentMarker::AllMarkers());
    DocumentMarkerVector markersInRange(Range*, DocumentMarker::MarkerTypes);
    DocumentMarkerVector markers();
    Vector<IntRect> renderedRectsForMarkers(DocumentMarker::MarkerType);
    void updateRenderedRectsForMarkers();

    DECLARE_TRACE();

#ifndef NDEBUG
    void showMarkers() const;
#endif

private:
    void addMarker(Node*, const DocumentMarker&);

    using MarkerList = WillBeHeapVector<OwnPtrWillBeMember<RenderedDocumentMarker>>;
    using MarkerLists = WillBeHeapVector<OwnPtrWillBeMember<MarkerList>, DocumentMarker::MarkerTypeIndexesCount>;
    using MarkerMap = WillBeHeapHashMap<RawPtrWillBeWeakMember<const Node>, OwnPtrWillBeMember<MarkerLists>>;
    void mergeOverlapping(MarkerList*, PassOwnPtrWillBeRawPtr<RenderedDocumentMarker>);
    bool possiblyHasMarkers(DocumentMarker::MarkerTypes);
    void removeMarkersFromList(MarkerMap::iterator, DocumentMarker::MarkerTypes);
    void removeMarkers(TextIterator&, DocumentMarker::MarkerTypes, RemovePartiallyOverlappingMarkerOrNot);

    MarkerMap m_markers;
    // Provide a quick way to determine whether a particular marker type is absent without going through the map.
    DocumentMarker::MarkerTypes m_possiblyExistingMarkerTypes;
};

} // namespace blink

#ifndef NDEBUG
void showDocumentMarkers(const blink::DocumentMarkerController*);
#endif

#endif // DocumentMarkerController_h
