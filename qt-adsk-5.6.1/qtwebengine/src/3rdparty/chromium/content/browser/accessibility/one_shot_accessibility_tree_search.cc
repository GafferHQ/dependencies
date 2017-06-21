// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

// Given a node, populate a vector with all of the strings from that node's
// attributes that might be relevant for a text search.
void GetNodeStrings(BrowserAccessibility* node,
                    std::vector<base::string16>* strings) {
  if (node->HasStringAttribute(ui::AX_ATTR_NAME))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_NAME));
  if (node->HasStringAttribute(ui::AX_ATTR_DESCRIPTION))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_DESCRIPTION));
  if (node->HasStringAttribute(ui::AX_ATTR_HELP))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_HELP));
  if (node->HasStringAttribute(ui::AX_ATTR_VALUE))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_VALUE));
  if (node->HasStringAttribute(ui::AX_ATTR_PLACEHOLDER))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_PLACEHOLDER));
}

OneShotAccessibilityTreeSearch::OneShotAccessibilityTreeSearch(
    BrowserAccessibilityManager* tree)
    : tree_(tree),
      start_node_(nullptr),
      direction_(OneShotAccessibilityTreeSearch::FORWARDS),
      result_limit_(UNLIMITED_RESULTS),
      immediate_descendants_only_(false),
      visible_only_(false),
      did_search_(false) {
}

OneShotAccessibilityTreeSearch::~OneShotAccessibilityTreeSearch() {
}

void OneShotAccessibilityTreeSearch::SetStartNode(
    BrowserAccessibility* start_node) {
  DCHECK(!did_search_);
  start_node_ = start_node;
}

void OneShotAccessibilityTreeSearch::SetDirection(Direction direction) {
  DCHECK(!did_search_);
  direction_ = direction;
}

void OneShotAccessibilityTreeSearch::SetResultLimit(int result_limit) {
  DCHECK(!did_search_);
  result_limit_ = result_limit;
}

void OneShotAccessibilityTreeSearch::SetImmediateDescendantsOnly(
    bool immediate_descendants_only) {
  DCHECK(!did_search_);
  immediate_descendants_only_ = immediate_descendants_only;
}

void OneShotAccessibilityTreeSearch::SetVisibleOnly(bool visible_only) {
  DCHECK(!did_search_);
  visible_only_ = visible_only;
}

void OneShotAccessibilityTreeSearch::SetSearchText(const std::string& text) {
  DCHECK(!did_search_);
  search_text_ = text;
}

void OneShotAccessibilityTreeSearch::AddPredicate(
    AccessibilityMatchPredicate predicate) {
  DCHECK(!did_search_);
  predicates_.push_back(predicate);
}

size_t OneShotAccessibilityTreeSearch::CountMatches() {
  if (!did_search_)
    Search();

  return matches_.size();
}

BrowserAccessibility* OneShotAccessibilityTreeSearch::GetMatchAtIndex(
    size_t index) {
  if (!did_search_)
    Search();

  CHECK(index < matches_.size());
  return matches_[index];
}

void OneShotAccessibilityTreeSearch::Search()
{
  if (immediate_descendants_only_) {
    SearchByIteratingOverChildren();
  } else {
    SearchByWalkingTree();
  }
}

void OneShotAccessibilityTreeSearch::SearchByIteratingOverChildren() {
  if (!start_node_)
    return;

  for (unsigned i = 0;
       i < start_node_->PlatformChildCount() &&
           (result_limit_ == UNLIMITED_RESULTS ||
            static_cast<int>(matches_.size()) < result_limit_);
       ++i) {
    BrowserAccessibility* child = start_node_->PlatformGetChild(i);
    if (Matches(child))
      matches_.push_back(child);
  }
}

void OneShotAccessibilityTreeSearch::SearchByWalkingTree() {
  BrowserAccessibility* node = nullptr;
  if (start_node_) {
    if (direction_ == FORWARDS)
      node = tree_->NextInTreeOrder(start_node_);
    else
      node = tree_->PreviousInTreeOrder(start_node_);
  } else {
    start_node_ = tree_->GetRoot();
    node = start_node_;
  }

  while (node && (result_limit_ == UNLIMITED_RESULTS ||
                  static_cast<int>(matches_.size()) < result_limit_)) {
    if (Matches(node))
      matches_.push_back(node);

    if (direction_ == FORWARDS)
      node = tree_->NextInTreeOrder(node);
    else
      node = tree_->PreviousInTreeOrder(node);
  }
}

bool OneShotAccessibilityTreeSearch::Matches(BrowserAccessibility* node) {
  for (size_t i = 0; i < predicates_.size(); ++i) {
    if (!predicates_[i](start_node_, node))
      return false;
  }

  if (visible_only_) {
    if (node->HasState(ui::AX_STATE_INVISIBLE) ||
        node->HasState(ui::AX_STATE_OFFSCREEN)) {
      return false;
    }
  }

  if (!search_text_.empty()) {
    base::string16 search_text_lower =
      base::i18n::ToLower(base::UTF8ToUTF16(search_text_));
    std::vector<base::string16> node_strings;
    GetNodeStrings(node, &node_strings);
    bool found_text_match = false;
    for (size_t i = 0; i < node_strings.size(); ++i) {
      base::string16 node_string_lower =
        base::i18n::ToLower(node_strings[i]);
      if (node_string_lower.find(search_text_lower) !=
          base::string16::npos) {
        found_text_match = true;
        break;
      }
    }
    if (!found_text_match)
      return false;
  }

  return true;
}

}  // namespace content
