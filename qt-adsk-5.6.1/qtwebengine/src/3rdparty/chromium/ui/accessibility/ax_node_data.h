// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_NODE_DATA_H_
#define UI_ACCESSIBILITY_AX_NODE_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

// A compact representation of the accessibility information for a
// single web object, in a form that can be serialized and sent from
// one process to another.
struct AX_EXPORT AXNodeData {
  AXNodeData();
  virtual ~AXNodeData();

  // Accessing accessibility attributes:
  //
  // There are dozens of possible attributes for an accessibility node,
  // but only a few tend to apply to any one object, so we store them
  // in sparse arrays of <attribute id, attribute value> pairs, organized
  // by type (bool, int, float, string, int list).
  //
  // There are three accessors for each type of attribute: one that returns
  // true if the attribute is present and false if not, one that takes a
  // pointer argument and returns true if the attribute is present (if you
  // need to distinguish between the default value and a missing attribute),
  // and another that returns the default value for that type if the
  // attribute is not present. In addition, strings can be returned as
  // either std::string or base::string16, for convenience.

  bool HasBoolAttribute(AXBoolAttribute attr) const;
  bool GetBoolAttribute(AXBoolAttribute attr) const;
  bool GetBoolAttribute(AXBoolAttribute attr, bool* value) const;

  bool HasFloatAttribute(AXFloatAttribute attr) const;
  float GetFloatAttribute(AXFloatAttribute attr) const;
  bool GetFloatAttribute(AXFloatAttribute attr, float* value) const;

  bool HasIntAttribute(AXIntAttribute attribute) const;
  int GetIntAttribute(AXIntAttribute attribute) const;
  bool GetIntAttribute(AXIntAttribute attribute, int* value) const;

  bool HasStringAttribute(
      AXStringAttribute attribute) const;
  const std::string& GetStringAttribute(AXStringAttribute attribute) const;
  bool GetStringAttribute(AXStringAttribute attribute,
                          std::string* value) const;

  bool GetString16Attribute(AXStringAttribute attribute,
                            base::string16* value) const;
  base::string16 GetString16Attribute(
      AXStringAttribute attribute) const;

  bool HasIntListAttribute(AXIntListAttribute attribute) const;
  const std::vector<int32>& GetIntListAttribute(
      AXIntListAttribute attribute) const;
  bool GetIntListAttribute(AXIntListAttribute attribute,
                           std::vector<int32>* value) const;

  bool GetHtmlAttribute(const char* attr, base::string16* value) const;
  bool GetHtmlAttribute(const char* attr, std::string* value) const;

  // Setting accessibility attributes.
  void AddStringAttribute(AXStringAttribute attribute,
                          const std::string& value);
  void AddIntAttribute(AXIntAttribute attribute, int value);
  void AddFloatAttribute(AXFloatAttribute attribute, float value);
  void AddBoolAttribute(AXBoolAttribute attribute, bool value);
  void AddIntListAttribute(AXIntListAttribute attribute,
                           const std::vector<int32>& value);

  // Convenience functions, mainly for writing unit tests.
  // Equivalent to AddStringAttribute(ATTR_NAME, name).
  void SetName(std::string name);
  // Equivalent to AddStringAttribute(ATTR_VALUE, value).
  void SetValue(std::string value);

  // Return a string representation of this data, for debugging.
  std::string ToString() const;

  // This is a simple serializable struct. All member variables should be
  // public and copyable.
  int32 id;
  AXRole role;
  uint32 state;
  gfx::Rect location;
  std::vector<std::pair<AXStringAttribute, std::string> > string_attributes;
  std::vector<std::pair<AXIntAttribute, int32> > int_attributes;
  std::vector<std::pair<AXFloatAttribute, float> > float_attributes;
  std::vector<std::pair<AXBoolAttribute, bool> > bool_attributes;
  std::vector<std::pair<AXIntListAttribute, std::vector<int32> > >
      intlist_attributes;
  base::StringPairs html_attributes;
  std::vector<int32> child_ids;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_NODE_DATA_H_
