// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "content/browser/accessibility/browser_accessibility.h"

namespace content {

class CONTENT_EXPORT BrowserAccessibilityAndroid : public BrowserAccessibility {
 public:
  // Overrides from BrowserAccessibility.
  void OnDataChanged() override;
  bool IsNative() const override;
  void OnLocationChanged() override;

  bool PlatformIsLeaf() const override;

  bool IsCheckable() const;
  bool IsChecked() const;
  bool IsClickable() const;
  bool IsCollection() const;
  bool IsCollectionItem() const;
  bool IsContentInvalid() const;
  bool IsDismissable() const;
  bool IsEditableText() const;
  bool IsEnabled() const;
  bool IsFocusable() const;
  bool IsFocused() const;
  bool IsHeading() const;
  bool IsHierarchical() const;
  bool IsLink() const;
  bool IsMultiLine() const;
  bool IsPassword() const;
  bool IsRangeType() const;
  bool IsScrollable() const;
  bool IsSelected() const;
  bool IsSlider() const;
  bool IsVisibleToUser() const;

  bool CanOpenPopup() const;

  bool HasFocusableChild() const;

  const char* GetClassName() const;
  base::string16 GetText() const;

  int GetItemIndex() const;
  int GetItemCount() const;

  bool CanScrollForward() const;
  bool CanScrollBackward() const;
  bool CanScrollUp() const;
  bool CanScrollDown() const;
  bool CanScrollLeft() const;
  bool CanScrollRight() const;
  int GetScrollX() const;
  int GetScrollY() const;
  int GetMinScrollX() const;
  int GetMinScrollY() const;
  int GetMaxScrollX() const;
  int GetMaxScrollY() const;
  bool Scroll(int direction) const;

  int GetTextChangeFromIndex() const;
  int GetTextChangeAddedCount() const;
  int GetTextChangeRemovedCount() const;
  base::string16 GetTextChangeBeforeText() const;

  int GetSelectionStart() const;
  int GetSelectionEnd() const;
  int GetEditableTextLength() const;

  int AndroidInputType() const;
  int AndroidLiveRegionType() const;
  int AndroidRangeType() const;

  int RowCount() const;
  int ColumnCount() const;

  int RowIndex() const;
  int RowSpan() const;
  int ColumnIndex() const;
  int ColumnSpan() const;

  float RangeMin() const;
  float RangeMax() const;
  float RangeCurrentValue() const;

  // Calls GetLineBoundaries or GetWordBoundaries depending on the value
  // of |granularity|, or fails if anything else is passed in |granularity|.
  void GetGranularityBoundaries(int granularity,
                                std::vector<int32>* starts,
                                std::vector<int32>* ends,
                                int offset);

  // Append line start and end indices for the text of this node
  // (as returned by GetText()), adding |offset| to each one.
  void GetLineBoundaries(std::vector<int32>* line_starts,
                         std::vector<int32>* line_ends,
                         int offset);

  // Append word start and end indices for the text of this node
  // (as returned by GetText()) to |word_starts| and |word_ends|,
  // adding |offset| to each one.
  void GetWordBoundaries(std::vector<int32>* word_starts,
                         std::vector<int32>* word_ends,
                         int offset);

 private:
  // This gives BrowserAccessibility::Create access to the class constructor.
  friend class BrowserAccessibility;

  BrowserAccessibilityAndroid();

  bool HasOnlyStaticTextChildren() const;
  bool HasOnlyTextAndImageChildren() const;
  bool IsIframe() const;

  void NotifyLiveRegionUpdate(base::string16& aria_live);

  int CountChildrenWithRole(ui::AXRole role) const;

  static size_t CommonPrefixLength(const base::string16 a,
                                   const base::string16 b);
  static size_t CommonSuffixLength(const base::string16 a,
                                   const base::string16 b);
  static size_t CommonEndLengths(const base::string16 a,
                                 const base::string16 b);

  base::string16 cached_text_;
  bool first_time_;
  base::string16 old_value_;
  base::string16 new_value_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_ANDROID_H_
