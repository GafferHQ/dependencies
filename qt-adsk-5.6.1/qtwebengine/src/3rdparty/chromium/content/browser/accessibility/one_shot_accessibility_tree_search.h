// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_SEARCH_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_SEARCH_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

class BrowserAccessibility;
class BrowserAccessibilityManager;

// A function that returns whether or not a given node matches, given the
// start element of the search as an optional comparator.
typedef bool (*AccessibilityMatchPredicate)(
    BrowserAccessibility* start_element,
    BrowserAccessibility* this_element);

// This class provides an interface for searching the accessibility tree from
// a given starting node, with a few built-in options and allowing an arbitrary
// number of predicates that can be used to restrict the search.
//
// This class is meant to perform one search. Initialize it, then iterate
// over the matches, and then delete it.
//
// This class stores raw pointers to the matches in the tree! Don't keep this
// object around if the tree is mutating.
class CONTENT_EXPORT OneShotAccessibilityTreeSearch {
 public:
  enum Direction { FORWARDS, BACKWARDS };

  const int UNLIMITED_RESULTS = -1;

  OneShotAccessibilityTreeSearch(BrowserAccessibilityManager* tree);
  virtual ~OneShotAccessibilityTreeSearch();

  //
  // Search parameters.  All of these are optional.
  //

  // Sets the node where the search starts. The first potential match will
  // be the one immediately following this one. This node will be used as
  // the first arguement to any predicates.
  void SetStartNode(BrowserAccessibility* start_node);

  // Search forwards or backwards in an in-order traversal of the tree.
  void SetDirection(Direction direction);

  // Set the maximum number of results, or UNLIMITED_RESULTS
  // for no limit (default).
  void SetResultLimit(int result_limit);

  // If true, only searches children of |start_node| and doesn't
  // recurse.
  void SetImmediateDescendantsOnly(bool immediate_descendants_only);

  // If true, only considers nodes that aren't invisible or offscreen.
  void SetVisibleOnly(bool visible_only);

  // Restricts the matches to only nodes where |text| is found as a
  // substring of any of that node's accessible text, including its
  // name, description, or value. Case-insensitive.
  void SetSearchText(const std::string& text);

  // Restricts the matches to only those that satisfy all predicates.
  void AddPredicate(AccessibilityMatchPredicate predicate);

  //
  // Calling either of these executes the search.
  //

  size_t CountMatches();
  BrowserAccessibility* GetMatchAtIndex(size_t index);

 private:
  void Search();
  void SearchByWalkingTree();
  void SearchByIteratingOverChildren();
  bool Matches(BrowserAccessibility* node);

  BrowserAccessibilityManager* tree_;
  BrowserAccessibility* start_node_;
  Direction direction_;
  int result_limit_;
  bool immediate_descendants_only_;
  bool visible_only_;
  std::string search_text_;

  std::vector<AccessibilityMatchPredicate> predicates_;
  std::vector<BrowserAccessibility*> matches_;

  bool did_search_;

  DISALLOW_COPY_AND_ASSIGN(OneShotAccessibilityTreeSearch);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_SEARCH_H_
