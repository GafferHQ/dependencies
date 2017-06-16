// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_win.h"

#include <UIAutomationClient.h>
#include <UIAutomationCoreApi.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/enum_variant.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/common/accessibility_messages.h"
#include "content/public/common/content_client.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/base/win/accessibility_ids_win.h"
#include "ui/base/win/accessibility_misc_utils.h"
#include "ui/base/win/atl_module.h"

namespace content {

// These nonstandard GUIDs are taken directly from the Mozilla sources
// (accessible/src/msaa/nsAccessNodeWrap.cpp); some documentation is here:
// http://developer.mozilla.org/en/Accessibility/AT-APIs/ImplementationFeatures/MSAA
const GUID GUID_ISimpleDOM = {0x0c539790,
                              0x12e4,
                              0x11cf,
                              {0xb6, 0x61, 0x00, 0xaa, 0x00, 0x4c, 0xd6, 0xd8}};
const GUID GUID_IAccessibleContentDocument = {
    0xa5d8e1f3,
    0x3571,
    0x4d8f,
    {0x95, 0x21, 0x07, 0xed, 0x28, 0xfb, 0x07, 0x2e}};

const base::char16 BrowserAccessibilityWin::kEmbeddedCharacter = L'\xfffc';

// static
LONG BrowserAccessibilityWin::next_unique_id_win_ =
    base::win::kFirstBrowserAccessibilityManagerAccessibilityId;

//
// BrowserAccessibilityRelation
//
// A simple implementation of IAccessibleRelation, used to represent
// a relationship between two accessible nodes in the tree.
//

class BrowserAccessibilityRelation
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IAccessibleRelation {
  BEGIN_COM_MAP(BrowserAccessibilityRelation)
    COM_INTERFACE_ENTRY(IAccessibleRelation)
  END_COM_MAP()

  CONTENT_EXPORT BrowserAccessibilityRelation() {}
  CONTENT_EXPORT virtual ~BrowserAccessibilityRelation() {}

  CONTENT_EXPORT void Initialize(BrowserAccessibilityWin* owner,
                                 const base::string16& type);
  CONTENT_EXPORT void AddTarget(int target_id);

  // IAccessibleRelation methods.
  CONTENT_EXPORT STDMETHODIMP get_relationType(BSTR* relation_type) override;
  CONTENT_EXPORT STDMETHODIMP get_nTargets(long* n_targets) override;
  CONTENT_EXPORT STDMETHODIMP
  get_target(long target_index, IUnknown** target) override;
  CONTENT_EXPORT STDMETHODIMP
  get_targets(long max_targets, IUnknown** targets, long* n_targets) override;

  // IAccessibleRelation methods not implemented.
  CONTENT_EXPORT STDMETHODIMP
  get_localizedRelationType(BSTR* relation_type) override {
    return E_NOTIMPL;
  }

 private:
  base::string16 type_;
  base::win::ScopedComPtr<BrowserAccessibilityWin> owner_;
  std::vector<int> target_ids_;
};

void BrowserAccessibilityRelation::Initialize(BrowserAccessibilityWin* owner,
                                              const base::string16& type) {
  owner_ = owner;
  type_ = type;
}

void BrowserAccessibilityRelation::AddTarget(int target_id) {
  target_ids_.push_back(target_id);
}

STDMETHODIMP BrowserAccessibilityRelation::get_relationType(
    BSTR* relation_type) {
  if (!relation_type)
    return E_INVALIDARG;

  if (!owner_->instance_active())
    return E_FAIL;

  *relation_type = SysAllocString(type_.c_str());
  DCHECK(*relation_type);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityRelation::get_nTargets(long* n_targets) {
  if (!n_targets)
    return E_INVALIDARG;

  if (!owner_->instance_active())
    return E_FAIL;

  *n_targets = static_cast<long>(target_ids_.size());

  BrowserAccessibilityManager* manager = owner_->manager();
  for (long i = *n_targets - 1; i >= 0; --i) {
    BrowserAccessibility* result = manager->GetFromID(target_ids_[i]);
    if (!result || !result->instance_active()) {
      *n_targets = 0;
      break;
    }
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityRelation::get_target(long target_index,
                                                      IUnknown** target) {
  if (!target)
    return E_INVALIDARG;

  if (!owner_->instance_active())
    return E_FAIL;

  if (target_index < 0 ||
      target_index >= static_cast<long>(target_ids_.size())) {
    return E_INVALIDARG;
  }

  BrowserAccessibilityManager* manager = owner_->manager();
  BrowserAccessibility* result =
      manager->GetFromID(target_ids_[target_index]);
  if (!result || !result->instance_active())
    return E_FAIL;

  *target = static_cast<IAccessible*>(
      result->ToBrowserAccessibilityWin()->NewReference());
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityRelation::get_targets(long max_targets,
                                                       IUnknown** targets,
                                                       long* n_targets) {
  if (!targets || !n_targets)
    return E_INVALIDARG;

  if (!owner_->instance_active())
    return E_FAIL;

  long count = static_cast<long>(target_ids_.size());
  if (count > max_targets)
    count = max_targets;

  *n_targets = count;
  if (count == 0)
    return S_FALSE;

  for (long i = 0; i < count; ++i) {
    HRESULT result = get_target(i, &targets[i]);
    if (result != S_OK)
      return result;
  }

  return S_OK;
}

//
// BrowserAccessibilityWin::WinAttributes
//

BrowserAccessibilityWin::WinAttributes::WinAttributes()
    : ia_role(0),
      ia_state(0),
      ia2_role(0),
      ia2_state(0) {
}

BrowserAccessibilityWin::WinAttributes::~WinAttributes() {
}

//
// BrowserAccessibilityWin
//

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  ui::win::CreateATLModuleIfNeeded();
  CComObject<BrowserAccessibilityWin>* instance;
  HRESULT hr = CComObject<BrowserAccessibilityWin>::CreateInstance(&instance);
  DCHECK(SUCCEEDED(hr));
  return instance->NewReference();
}

BrowserAccessibilityWin* BrowserAccessibility::ToBrowserAccessibilityWin() {
  return static_cast<BrowserAccessibilityWin*>(this);
}

BrowserAccessibilityWin::BrowserAccessibilityWin()
    : win_attributes_(new WinAttributes()),
      previous_scroll_x_(0),
      previous_scroll_y_(0) {
  // Start unique IDs at -1 and decrement each time, because get_accChild
  // uses positive IDs to enumerate children, so we use negative IDs to
  // clearly distinguish between indices and unique IDs.
  unique_id_win_ = next_unique_id_win_;
  if (next_unique_id_win_ ==
          base::win::kLastBrowserAccessibilityManagerAccessibilityId) {
    next_unique_id_win_ =
        base::win::kFirstBrowserAccessibilityManagerAccessibilityId;
  }
  next_unique_id_win_--;
}

BrowserAccessibilityWin::~BrowserAccessibilityWin() {
  for (size_t i = 0; i < relations_.size(); ++i)
    relations_[i]->Release();
}

//
// IAccessible methods.
//
// Conventions:
// * Always test for instance_active() first and return E_FAIL if it's false.
// * Always check for invalid arguments first, even if they're unused.
// * Return S_FALSE if the only output is a string argument and it's empty.
//

HRESULT BrowserAccessibilityWin::accDoDefaultAction(VARIANT var_id) {
  if (!instance_active())
    return E_FAIL;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  manager()->DoDefaultAction(*target);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accHitTest(LONG x_left,
                                                 LONG y_top,
                                                 VARIANT* child) {
  if (!instance_active())
    return E_FAIL;

  if (!child)
    return E_INVALIDARG;

  gfx::Point point(x_left, y_top);
  if (!GetGlobalBoundsRect().Contains(point)) {
    // Return S_FALSE and VT_EMPTY when the outside the object's boundaries.
    child->vt = VT_EMPTY;
    return S_FALSE;
  }

  BrowserAccessibility* result = BrowserAccessibilityForPoint(point);
  if (result == this) {
    // Point is within this object.
    child->vt = VT_I4;
    child->lVal = CHILDID_SELF;
  } else {
    child->vt = VT_DISPATCH;
    child->pdispVal = result->ToBrowserAccessibilityWin()->NewReference();
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accLocation(LONG* x_left,
                                                  LONG* y_top,
                                                  LONG* width,
                                                  LONG* height,
                                                  VARIANT var_id) {
  if (!instance_active())
    return E_FAIL;

  if (!x_left || !y_top || !width || !height)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  gfx::Rect bounds = target->GetGlobalBoundsRect();
  *x_left = bounds.x();
  *y_top  = bounds.y();
  *width  = bounds.width();
  *height = bounds.height();

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accNavigate(LONG nav_dir,
                                                  VARIANT start,
                                                  VARIANT* end) {
  BrowserAccessibilityWin* target = GetTargetFromChildID(start);
  if (!target)
    return E_INVALIDARG;

  if ((nav_dir == NAVDIR_LASTCHILD || nav_dir == NAVDIR_FIRSTCHILD) &&
      start.lVal != CHILDID_SELF) {
    // MSAA states that navigating to first/last child can only be from self.
    return E_INVALIDARG;
  }

  uint32 child_count = target->PlatformChildCount();

  BrowserAccessibility* result = NULL;
  switch (nav_dir) {
    case NAVDIR_DOWN:
    case NAVDIR_UP:
    case NAVDIR_LEFT:
    case NAVDIR_RIGHT:
      // These directions are not implemented, matching Mozilla and IE.
      return E_NOTIMPL;
    case NAVDIR_FIRSTCHILD:
      if (child_count > 0)
        result = target->PlatformGetChild(0);
      break;
    case NAVDIR_LASTCHILD:
      if (child_count > 0)
        result = target->PlatformGetChild(child_count - 1);
      break;
    case NAVDIR_NEXT:
      result = target->GetNextSibling();
      break;
    case NAVDIR_PREVIOUS:
      result = target->GetPreviousSibling();
      break;
  }

  if (!result) {
    end->vt = VT_EMPTY;
    return S_FALSE;
  }

  end->vt = VT_DISPATCH;
  end->pdispVal = result->ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accChild(VARIANT var_child,
                                                   IDispatch** disp_child) {
  if (!instance_active())
    return E_FAIL;

  if (!disp_child)
    return E_INVALIDARG;

  *disp_child = NULL;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_child);
  if (!target)
    return E_INVALIDARG;

  (*disp_child) = target->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accChildCount(LONG* child_count) {
  if (!instance_active())
    return E_FAIL;

  if (!child_count)
    return E_INVALIDARG;

  *child_count = PlatformChildCount();

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accDefaultAction(VARIANT var_id,
                                                           BSTR* def_action) {
  if (!instance_active())
    return E_FAIL;

  if (!def_action)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetStringAttributeAsBstr(
      ui::AX_ATTR_ACTION, def_action);
}

STDMETHODIMP BrowserAccessibilityWin::get_accDescription(VARIANT var_id,
                                                         BSTR* desc) {
  if (!instance_active())
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  base::string16 description_str = target->description();
  if (description_str.empty())
    return S_FALSE;

  *desc = SysAllocString(description_str.c_str());

  DCHECK(*desc);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accFocus(VARIANT* focus_child) {
  if (!instance_active())
    return E_FAIL;

  if (!focus_child)
    return E_INVALIDARG;

  BrowserAccessibilityWin* focus = static_cast<BrowserAccessibilityWin*>(
      manager()->GetFocus(this));
  if (focus == this) {
    focus_child->vt = VT_I4;
    focus_child->lVal = CHILDID_SELF;
  } else if (focus == NULL) {
    focus_child->vt = VT_EMPTY;
  } else {
    focus_child->vt = VT_DISPATCH;
    focus_child->pdispVal = focus->NewReference();
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accHelp(VARIANT var_id, BSTR* help) {
  if (!instance_active())
    return E_FAIL;

  if (!help)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  base::string16 help_str = target->help();
  if (help_str.empty())
    return S_FALSE;

  *help = SysAllocString(help_str.c_str());

  DCHECK(*help);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accKeyboardShortcut(VARIANT var_id,
                                                              BSTR* acc_key) {
  if (!instance_active())
    return E_FAIL;

  if (!acc_key)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  return target->GetStringAttributeAsBstr(
      ui::AX_ATTR_SHORTCUT, acc_key);
}

STDMETHODIMP BrowserAccessibilityWin::get_accName(VARIANT var_id, BSTR* name) {
  if (!instance_active())
    return E_FAIL;

  if (!name)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  base::string16 name_str = target->name();

  // If the name is empty, see if it's labeled by another element.
  if (name_str.empty()) {
    int title_elem_id;
    if (target->GetIntAttribute(ui::AX_ATTR_TITLE_UI_ELEMENT,
                                &title_elem_id)) {
      BrowserAccessibilityWin* title_elem =
          manager()->GetFromID(title_elem_id)->ToBrowserAccessibilityWin();
      if (title_elem)
        name_str = title_elem->GetNameRecursive();
    }
  }

  if (name_str.empty())
    return S_FALSE;

  *name = SysAllocString(name_str.c_str());

  DCHECK(*name);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accParent(IDispatch** disp_parent) {
  if (!instance_active())
    return E_FAIL;

  if (!disp_parent)
    return E_INVALIDARG;

  IAccessible* parent_obj = GetParent()->ToBrowserAccessibilityWin();
  if (parent_obj == NULL) {
    // This happens if we're the root of the tree;
    // return the IAccessible for the window.
    parent_obj =
        manager()->ToBrowserAccessibilityManagerWin()->GetParentIAccessible();
    // |parent| can only be NULL if the manager was created before the parent
    // IAccessible was known and it wasn't subsequently set before a client
    // requested it. This has been fixed. |parent| may also be NULL during
    // destruction. Possible cases where this could occur include tabs being
    // dragged to a new window, etc.
    if (!parent_obj) {
      DVLOG(1) <<  "In Function: "
               << __FUNCTION__
               << ". Parent IAccessible interface is NULL. Returning failure";
      return E_FAIL;
    }
  }
  parent_obj->AddRef();
  *disp_parent = parent_obj;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accRole(VARIANT var_id,
                                                  VARIANT* role) {
  if (!instance_active())
    return E_FAIL;

  if (!role)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  if (!target->role_name().empty()) {
    role->vt = VT_BSTR;
    role->bstrVal = SysAllocString(target->role_name().c_str());
  } else {
    role->vt = VT_I4;
    role->lVal = target->ia_role();
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accState(VARIANT var_id,
                                                   VARIANT* state) {
  if (!instance_active())
    return E_FAIL;

  if (!state)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  state->vt = VT_I4;
  state->lVal = target->ia_state();
  if (manager()->GetFocus(NULL) == this)
    state->lVal |= STATE_SYSTEM_FOCUSED;

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accValue(VARIANT var_id,
                                                   BSTR* value) {
  if (!instance_active())
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  BrowserAccessibilityWin* target = GetTargetFromChildID(var_id);
  if (!target)
    return E_INVALIDARG;

  if (target->ia_role() == ROLE_SYSTEM_PROGRESSBAR ||
      target->ia_role() == ROLE_SYSTEM_SCROLLBAR ||
      target->ia_role() == ROLE_SYSTEM_SLIDER) {
    base::string16 value_text = target->GetValueText();
    *value = SysAllocString(value_text.c_str());
    DCHECK(*value);
    return S_OK;
  }

  // Expose color well value.
  if (target->ia2_role() == IA2_ROLE_COLOR_CHOOSER) {
    int color = target->GetIntAttribute(ui::AX_ATTR_COLOR_VALUE);
    int red = (color >> 16) & 0xFF;
    int green = (color >> 8) & 0xFF;
    int blue = color & 0xFF;
    base::string16 value_text;
    value_text = base::IntToString16((red * 100) / 255) + L"% red " +
                 base::IntToString16((green * 100) / 255) + L"% green " +
                 base::IntToString16((blue * 100) / 255) + L"% blue";
    *value = SysAllocString(value_text.c_str());
    DCHECK(*value);
    return S_OK;
  }

  *value = SysAllocString(target->value().c_str());
  DCHECK(*value);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_accHelpTopic(BSTR* help_file,
                                                       VARIANT var_id,
                                                       LONG* topic_id) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_accSelection(VARIANT* selected) {
  if (!instance_active())
    return E_FAIL;

  if (GetRole() != ui::AX_ROLE_LIST_BOX)
    return E_NOTIMPL;

  unsigned long selected_count = 0;
  for (size_t i = 0; i < InternalChildCount(); ++i) {
    if (InternalGetChild(i)->HasState(ui::AX_STATE_SELECTED))
      ++selected_count;
  }

  if (selected_count == 0) {
    selected->vt = VT_EMPTY;
    return S_OK;
  }

  if (selected_count == 1) {
    for (size_t i = 0; i < InternalChildCount(); ++i) {
      if (InternalGetChild(i)->HasState(ui::AX_STATE_SELECTED)) {
        selected->vt = VT_DISPATCH;
        selected->pdispVal =
            InternalGetChild(i)->ToBrowserAccessibilityWin()->NewReference();
        return S_OK;
      }
    }
  }

  // Multiple items are selected.
  base::win::EnumVariant* enum_variant =
      new base::win::EnumVariant(selected_count);
  enum_variant->AddRef();
  unsigned long index = 0;
  for (size_t i = 0; i < InternalChildCount(); ++i) {
    if (InternalGetChild(i)->HasState(ui::AX_STATE_SELECTED)) {
      enum_variant->ItemAt(index)->vt = VT_DISPATCH;
      enum_variant->ItemAt(index)->pdispVal =
        InternalGetChild(i)->ToBrowserAccessibilityWin()->NewReference();
      ++index;
    }
  }
  selected->vt = VT_UNKNOWN;
  selected->punkVal = static_cast<IUnknown*>(
      static_cast<base::win::IUnknownImpl*>(enum_variant));
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::accSelect(
    LONG flags_sel, VARIANT var_id) {
  if (!instance_active())
    return E_FAIL;

  if (flags_sel & SELFLAG_TAKEFOCUS) {
    manager()->SetFocus(this, true);
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP
BrowserAccessibilityWin::put_accName(VARIANT var_id, BSTR put_name) {
  return E_NOTIMPL;
}
STDMETHODIMP
BrowserAccessibilityWin::put_accValue(VARIANT var_id, BSTR put_val) {
  return E_NOTIMPL;
}

//
// IAccessible2 methods.
//

STDMETHODIMP BrowserAccessibilityWin::role(LONG* role) {
  if (!instance_active())
    return E_FAIL;

  if (!role)
    return E_INVALIDARG;

  *role = ia2_role();

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_attributes(BSTR* attributes) {
  if (!instance_active())
    return E_FAIL;

  if (!attributes)
    return E_INVALIDARG;

  // The iaccessible2 attributes are a set of key-value pairs
  // separated by semicolons, with a colon between the key and the value.
  base::string16 str;
  const std::vector<base::string16>& attributes_list = ia2_attributes();
  for (unsigned int i = 0; i < attributes_list.size(); ++i) {
    str += attributes_list[i] + L';';
  }

  if (str.empty())
    return S_FALSE;

  *attributes = SysAllocString(str.c_str());
  DCHECK(*attributes);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_states(AccessibleStates* states) {
  if (!instance_active())
    return E_FAIL;

  if (!states)
    return E_INVALIDARG;

  *states = ia2_state();

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_uniqueID(LONG* unique_id) {
  if (!instance_active())
    return E_FAIL;

  if (!unique_id)
    return E_INVALIDARG;

  *unique_id = unique_id_win_;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_windowHandle(HWND* window_handle) {
  if (!instance_active())
    return E_FAIL;

  if (!window_handle)
    return E_INVALIDARG;

  *window_handle =
      manager()->ToBrowserAccessibilityManagerWin()->GetParentHWND();
  if (!*window_handle)
    return E_FAIL;

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_indexInParent(LONG* index_in_parent) {
  if (!instance_active())
    return E_FAIL;

  if (!index_in_parent)
    return E_INVALIDARG;

  *index_in_parent = this->GetIndexInParent();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nRelations(LONG* n_relations) {
  if (!instance_active())
    return E_FAIL;

  if (!n_relations)
    return E_INVALIDARG;

  *n_relations = relations_.size();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_relation(
    LONG relation_index,
    IAccessibleRelation** relation) {
  if (!instance_active())
    return E_FAIL;

  if (relation_index < 0 ||
      relation_index >= static_cast<long>(relations_.size())) {
    return E_INVALIDARG;
  }

  if (!relation)
    return E_INVALIDARG;

  relations_[relation_index]->AddRef();
  *relation = relations_[relation_index];
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_relations(
    LONG max_relations,
    IAccessibleRelation** relations,
    LONG* n_relations) {
  if (!instance_active())
    return E_FAIL;

  if (!relations || !n_relations)
    return E_INVALIDARG;

  long count = static_cast<long>(relations_.size());
  *n_relations = count;
  if (count == 0)
    return S_FALSE;

  for (long i = 0; i < count; ++i) {
    relations_[i]->AddRef();
    relations[i] = relations_[i];
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollTo(enum IA2ScrollType scroll_type) {
  if (!instance_active())
    return E_FAIL;

  gfx::Rect r = GetLocation();
  switch(scroll_type) {
    case IA2_SCROLL_TYPE_TOP_LEFT:
      manager()->ScrollToMakeVisible(*this, gfx::Rect(r.x(), r.y(), 0, 0));
      break;
    case IA2_SCROLL_TYPE_BOTTOM_RIGHT:
      manager()->ScrollToMakeVisible(
          *this, gfx::Rect(r.right(), r.bottom(), 0, 0));
      break;
    case IA2_SCROLL_TYPE_TOP_EDGE:
      manager()->ScrollToMakeVisible(
          *this, gfx::Rect(r.x(), r.y(), r.width(), 0));
      break;
    case IA2_SCROLL_TYPE_BOTTOM_EDGE:
      manager()->ScrollToMakeVisible(
          *this, gfx::Rect(r.x(), r.bottom(), r.width(), 0));
    break;
    case IA2_SCROLL_TYPE_LEFT_EDGE:
      manager()->ScrollToMakeVisible(
          *this, gfx::Rect(r.x(), r.y(), 0, r.height()));
      break;
    case IA2_SCROLL_TYPE_RIGHT_EDGE:
      manager()->ScrollToMakeVisible(
          *this, gfx::Rect(r.right(), r.y(), 0, r.height()));
      break;
    case IA2_SCROLL_TYPE_ANYWHERE:
    default:
      manager()->ScrollToMakeVisible(*this, r);
      break;
  }

  manager()->ToBrowserAccessibilityManagerWin()->TrackScrollingObject(this);

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollToPoint(
    enum IA2CoordinateType coordinate_type,
    LONG x,
    LONG y) {
  if (!instance_active())
    return E_FAIL;

  gfx::Point scroll_to(x, y);

  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    scroll_to -= manager()->GetViewBounds().OffsetFromOrigin();
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    if (GetParent())
      scroll_to += GetParent()->GetLocation().OffsetFromOrigin();
  } else {
    return E_INVALIDARG;
  }

  manager()->ScrollToPoint(*this, scroll_to);
  manager()->ToBrowserAccessibilityManagerWin()->TrackScrollingObject(this);

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_groupPosition(
    LONG* group_level,
    LONG* similar_items_in_group,
    LONG* position_in_group) {
  if (!instance_active())
    return E_FAIL;

  if (!group_level || !similar_items_in_group || !position_in_group)
    return E_INVALIDARG;

  *group_level = 0;
  *similar_items_in_group = GetIntAttribute(ui::AX_ATTR_SET_SIZE);
  *position_in_group = GetIntAttribute(ui::AX_ATTR_POS_IN_SET);
  return S_OK;
}

//
// IAccessibleEx methods not implemented.
//

STDMETHODIMP BrowserAccessibilityWin::get_extendedRole(BSTR* extended_role) {
  return E_NOTIMPL;
}
STDMETHODIMP
BrowserAccessibilityWin::get_localizedExtendedRole(
    BSTR* localized_extended_role) {
  return E_NOTIMPL;
}
STDMETHODIMP
BrowserAccessibilityWin::get_nExtendedStates(LONG* n_extended_states) {
  return E_NOTIMPL;
}
STDMETHODIMP
BrowserAccessibilityWin::get_extendedStates(LONG max_extended_states,
                                            BSTR** extended_states,
                                            LONG* n_extended_states) {
  return E_NOTIMPL;
}
STDMETHODIMP
BrowserAccessibilityWin::get_localizedExtendedStates(
    LONG max_localized_extended_states,
    BSTR** localized_extended_states,
    LONG* n_localized_extended_states) {
  return E_NOTIMPL;
}
STDMETHODIMP BrowserAccessibilityWin::get_locale(IA2Locale* locale) {
  return E_NOTIMPL;
}

//
// IAccessibleApplication methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_appName(BSTR* app_name) {
  // No need to check |instance_active()| because this interface is
  // global, and doesn't depend on any local state.

  if (!app_name)
    return E_INVALIDARG;

  // GetProduct() returns a string like "Chrome/aa.bb.cc.dd", split out
  // the part before the "/".
  std::vector<std::string> product_components;
  base::SplitString(GetContentClient()->GetProduct(), '/', &product_components);
  DCHECK_EQ(2U, product_components.size());
  if (product_components.size() != 2)
    return E_FAIL;
  *app_name = SysAllocString(base::UTF8ToUTF16(product_components[0]).c_str());
  DCHECK(*app_name);
  return *app_name ? S_OK : E_FAIL;
}

STDMETHODIMP BrowserAccessibilityWin::get_appVersion(BSTR* app_version) {
  // No need to check |instance_active()| because this interface is
  // global, and doesn't depend on any local state.

  if (!app_version)
    return E_INVALIDARG;

  // GetProduct() returns a string like "Chrome/aa.bb.cc.dd", split out
  // the part after the "/".
  std::vector<std::string> product_components;
  base::SplitString(GetContentClient()->GetProduct(), '/', &product_components);
  DCHECK_EQ(2U, product_components.size());
  if (product_components.size() != 2)
    return E_FAIL;
  *app_version =
      SysAllocString(base::UTF8ToUTF16(product_components[1]).c_str());
  DCHECK(*app_version);
  return *app_version ? S_OK : E_FAIL;
}

STDMETHODIMP BrowserAccessibilityWin::get_toolkitName(BSTR* toolkit_name) {
  // No need to check |instance_active()| because this interface is
  // global, and doesn't depend on any local state.

  if (!toolkit_name)
    return E_INVALIDARG;

  // This is hard-coded; all products based on the Chromium engine
  // will have the same toolkit name, so that assistive technology can
  // detect any Chrome-based product.
  *toolkit_name = SysAllocString(L"Chrome");
  DCHECK(*toolkit_name);
  return *toolkit_name ? S_OK : E_FAIL;
}

STDMETHODIMP BrowserAccessibilityWin::get_toolkitVersion(
    BSTR* toolkit_version) {
  // No need to check |instance_active()| because this interface is
  // global, and doesn't depend on any local state.

  if (!toolkit_version)
    return E_INVALIDARG;

  std::string user_agent = GetContentClient()->GetUserAgent();
  *toolkit_version = SysAllocString(base::UTF8ToUTF16(user_agent).c_str());
  DCHECK(*toolkit_version);
  return *toolkit_version ? S_OK : E_FAIL;
}

//
// IAccessibleImage methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_description(BSTR* desc) {
  if (!instance_active())
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  if (description().empty())
    return S_FALSE;

  *desc = SysAllocString(description().c_str());

  DCHECK(*desc);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_imagePosition(
    enum IA2CoordinateType coordinate_type,
    LONG* x,
    LONG* y) {
  if (!instance_active())
    return E_FAIL;

  if (!x || !y)
    return E_INVALIDARG;

  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    HWND parent_hwnd =
        manager()->ToBrowserAccessibilityManagerWin()->GetParentHWND();
    if (!parent_hwnd)
      return E_FAIL;
    POINT top_left = {0, 0};
    ::ClientToScreen(parent_hwnd, &top_left);
    *x = GetLocation().x() + top_left.x;
    *y = GetLocation().y() + top_left.y;
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    *x = GetLocation().x();
    *y = GetLocation().y();
    if (GetParent()) {
      *x -= GetParent()->GetLocation().x();
      *y -= GetParent()->GetLocation().y();
    }
  } else {
    return E_INVALIDARG;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_imageSize(LONG* height, LONG* width) {
  if (!instance_active())
    return E_FAIL;

  if (!height || !width)
    return E_INVALIDARG;

  *height = GetLocation().height();
  *width = GetLocation().width();
  return S_OK;
}

//
// IAccessibleTable methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_accessibleAt(
    long row,
    long column,
    IUnknown** accessible) {
  if (!instance_active())
    return E_FAIL;

  if (!accessible)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(
          ui::AX_ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  const std::vector<int32>& cell_ids = GetIntListAttribute(
      ui::AX_ATTR_CELL_IDS);
  DCHECK_EQ(columns * rows, static_cast<int>(cell_ids.size()));

  int cell_id = cell_ids[row * columns + column];
  BrowserAccessibilityWin* cell = GetFromID(cell_id);
  if (cell) {
    *accessible = static_cast<IAccessible*>(cell->NewReference());
    return S_OK;
  }

  *accessible = NULL;
  return E_INVALIDARG;
}

STDMETHODIMP BrowserAccessibilityWin::get_caption(IUnknown** accessible) {
  if (!instance_active())
    return E_FAIL;

  if (!accessible)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_childIndex(long row,
                                                     long column,
                                                     long* cell_index) {
  if (!instance_active())
    return E_FAIL;

  if (!cell_index)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(
          ui::AX_ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  const std::vector<int32>& cell_ids = GetIntListAttribute(
      ui::AX_ATTR_CELL_IDS);
  const std::vector<int32>& unique_cell_ids = GetIntListAttribute(
      ui::AX_ATTR_UNIQUE_CELL_IDS);
  DCHECK_EQ(columns * rows, static_cast<int>(cell_ids.size()));
  int cell_id = cell_ids[row * columns + column];
  for (size_t i = 0; i < unique_cell_ids.size(); ++i) {
    if (unique_cell_ids[i] == cell_id) {
      *cell_index = (long)i;
      return S_OK;
    }
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnDescription(long column,
                                                            BSTR* description) {
  if (!instance_active())
    return E_FAIL;

  if (!description)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(ui::AX_ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (column < 0 || column >= columns)
    return E_INVALIDARG;

  const std::vector<int32>& cell_ids = GetIntListAttribute(
      ui::AX_ATTR_CELL_IDS);
  for (int i = 0; i < rows; ++i) {
    int cell_id = cell_ids[i * columns + column];
    BrowserAccessibilityWin* cell = static_cast<BrowserAccessibilityWin*>(
        manager()->GetFromID(cell_id));
    if (cell && cell->GetRole() == ui::AX_ROLE_COLUMN_HEADER) {
      base::string16 cell_name = cell->GetString16Attribute(
          ui::AX_ATTR_NAME);
      if (cell_name.size() > 0) {
        *description = SysAllocString(cell_name.c_str());
        return S_OK;
      }

      if (cell->description().size() > 0) {
        *description = SysAllocString(cell->description().c_str());
        return S_OK;
      }
    }
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnExtentAt(
    long row,
    long column,
    long* n_columns_spanned) {
  if (!instance_active())
    return E_FAIL;

  if (!n_columns_spanned)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(ui::AX_ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  const std::vector<int32>& cell_ids = GetIntListAttribute(
      ui::AX_ATTR_CELL_IDS);
  int cell_id = cell_ids[row * columns + column];
  BrowserAccessibilityWin* cell = static_cast<BrowserAccessibilityWin*>(
      manager()->GetFromID(cell_id));
  int colspan;
  if (cell &&
      cell->GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN, &colspan) &&
      colspan >= 1) {
    *n_columns_spanned = colspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnHeader(
    IAccessibleTable** accessible_table,
    long* starting_row_index) {
  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnIndex(long cell_index,
                                                      long* column_index) {
  if (!instance_active())
    return E_FAIL;

  if (!column_index)
    return E_INVALIDARG;

  const std::vector<int32>& unique_cell_ids = GetIntListAttribute(
      ui::AX_ATTR_UNIQUE_CELL_IDS);
  int cell_id_count = static_cast<int>(unique_cell_ids.size());
  if (cell_index < 0)
    return E_INVALIDARG;
  if (cell_index >= cell_id_count)
    return S_FALSE;

  int cell_id = unique_cell_ids[cell_index];
  BrowserAccessibilityWin* cell =
      manager()->GetFromID(cell_id)->ToBrowserAccessibilityWin();
  int col_index;
  if (cell &&
      cell->GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX, &col_index)) {
    *column_index = col_index;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_nColumns(long* column_count) {
  if (!instance_active())
    return E_FAIL;

  if (!column_count)
    return E_INVALIDARG;

  int columns;
  if (GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_COUNT, &columns)) {
    *column_count = columns;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_nRows(long* row_count) {
  if (!instance_active())
    return E_FAIL;

  if (!row_count)
    return E_INVALIDARG;

  int rows;
  if (GetIntAttribute(ui::AX_ATTR_TABLE_ROW_COUNT, &rows)) {
    *row_count = rows;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedChildren(long* cell_count) {
  if (!instance_active())
    return E_FAIL;

  if (!cell_count)
    return E_INVALIDARG;

  // TODO(dmazzoni): add support for selected cells/rows/columns in tables.
  *cell_count = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedColumns(long* column_count) {
  if (!instance_active())
    return E_FAIL;

  if (!column_count)
    return E_INVALIDARG;

  *column_count = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedRows(long* row_count) {
  if (!instance_active())
    return E_FAIL;

  if (!row_count)
    return E_INVALIDARG;

  *row_count = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowDescription(long row,
                                                         BSTR* description) {
  if (!instance_active())
    return E_FAIL;

  if (!description)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(ui::AX_ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows)
    return E_INVALIDARG;

  const std::vector<int32>& cell_ids = GetIntListAttribute(
      ui::AX_ATTR_CELL_IDS);
  for (int i = 0; i < columns; ++i) {
    int cell_id = cell_ids[row * columns + i];
    BrowserAccessibilityWin* cell =
        manager()->GetFromID(cell_id)->ToBrowserAccessibilityWin();
    if (cell && cell->GetRole() == ui::AX_ROLE_ROW_HEADER) {
      base::string16 cell_name = cell->GetString16Attribute(
          ui::AX_ATTR_NAME);
      if (cell_name.size() > 0) {
        *description = SysAllocString(cell_name.c_str());
        return S_OK;
      }

      if (cell->description().size() > 0) {
        *description = SysAllocString(cell->description().c_str());
        return S_OK;
      }
    }
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowExtentAt(long row,
                                                      long column,
                                                      long* n_rows_spanned) {
  if (!instance_active())
    return E_FAIL;

  if (!n_rows_spanned)
    return E_INVALIDARG;

  int columns;
  int rows;
  if (!GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !GetIntAttribute(ui::AX_ATTR_TABLE_ROW_COUNT, &rows) ||
      columns <= 0 ||
      rows <= 0) {
    return S_FALSE;
  }

  if (row < 0 || row >= rows || column < 0 || column >= columns)
    return E_INVALIDARG;

  const std::vector<int32>& cell_ids = GetIntListAttribute(
      ui::AX_ATTR_CELL_IDS);
  int cell_id = cell_ids[row * columns + column];
  BrowserAccessibilityWin* cell =
      manager()->GetFromID(cell_id)->ToBrowserAccessibilityWin();
  int rowspan;
  if (cell &&
      cell->GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      rowspan >= 1) {
    *n_rows_spanned = rowspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowHeader(
    IAccessibleTable** accessible_table,
    long* starting_column_index) {
  // TODO(dmazzoni): implement
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowIndex(long cell_index,
                                                   long* row_index) {
  if (!instance_active())
    return E_FAIL;

  if (!row_index)
    return E_INVALIDARG;

  const std::vector<int32>& unique_cell_ids = GetIntListAttribute(
      ui::AX_ATTR_UNIQUE_CELL_IDS);
  int cell_id_count = static_cast<int>(unique_cell_ids.size());
  if (cell_index < 0)
    return E_INVALIDARG;
  if (cell_index >= cell_id_count)
    return S_FALSE;

  int cell_id = unique_cell_ids[cell_index];
  BrowserAccessibilityWin* cell =
      manager()->GetFromID(cell_id)->ToBrowserAccessibilityWin();
  int cell_row_index;
  if (cell &&
      cell->GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_ROW_INDEX, &cell_row_index)) {
    *row_index = cell_row_index;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedChildren(long max_children,
                                                           long** children,
                                                           long* n_children) {
  if (!instance_active())
    return E_FAIL;

  if (!children || !n_children)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_children = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedColumns(long max_columns,
                                                          long** columns,
                                                          long* n_columns) {
  if (!instance_active())
    return E_FAIL;

  if (!columns || !n_columns)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_columns = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedRows(long max_rows,
                                                       long** rows,
                                                       long* n_rows) {
  if (!instance_active())
    return E_FAIL;

  if (!rows || !n_rows)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_rows = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_summary(IUnknown** accessible) {
  if (!instance_active())
    return E_FAIL;

  if (!accessible)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_isColumnSelected(
    long column,
    boolean* is_selected) {
  if (!instance_active())
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_isRowSelected(long row,
                                                        boolean* is_selected) {
  if (!instance_active())
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_isSelected(long row,
                                                     long column,
                                                     boolean* is_selected) {
  if (!instance_active())
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowColumnExtentsAtIndex(
    long index,
    long* row,
    long* column,
    long* row_extents,
    long* column_extents,
    boolean* is_selected) {
  if (!instance_active())
    return E_FAIL;

  if (!row || !column || !row_extents || !column_extents || !is_selected)
    return E_INVALIDARG;

  const std::vector<int32>& unique_cell_ids = GetIntListAttribute(
      ui::AX_ATTR_UNIQUE_CELL_IDS);
  int cell_id_count = static_cast<int>(unique_cell_ids.size());
  if (index < 0)
    return E_INVALIDARG;
  if (index >= cell_id_count)
    return S_FALSE;

  int cell_id = unique_cell_ids[index];
  BrowserAccessibilityWin* cell =
      manager()->GetFromID(cell_id)->ToBrowserAccessibilityWin();
  int rowspan;
  int colspan;
  if (cell &&
      cell->GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      cell->GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN, &colspan) &&
      rowspan >= 1 &&
      colspan >= 1) {
    *row_extents = rowspan;
    *column_extents = colspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::selectRow(long row) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::selectColumn(long column) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::unselectRow(long row) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::unselectColumn(long column) {
  return E_NOTIMPL;
}

STDMETHODIMP
BrowserAccessibilityWin::get_modelChange(IA2TableModelChange* model_change) {
  return E_NOTIMPL;
}

//
// IAccessibleTable2 methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_cellAt(long row,
                                                 long column,
                                                 IUnknown** cell) {
  return get_accessibleAt(row, column, cell);
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelectedCells(long* cell_count) {
  return get_nSelectedChildren(cell_count);
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedCells(
    IUnknown*** cells,
    long* n_selected_cells) {
  if (!instance_active())
    return E_FAIL;

  if (!cells || !n_selected_cells)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_selected_cells = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedColumns(long** columns,
                                                          long* n_columns) {
  if (!instance_active())
    return E_FAIL;

  if (!columns || !n_columns)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_columns = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selectedRows(long** rows,
                                                       long* n_rows) {
  if (!instance_active())
    return E_FAIL;

  if (!rows || !n_rows)
    return E_INVALIDARG;

  // TODO(dmazzoni): Implement this.
  *n_rows = 0;
  return S_OK;
}


//
// IAccessibleTableCell methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_columnExtent(
    long* n_columns_spanned) {
  if (!instance_active())
    return E_FAIL;

  if (!n_columns_spanned)
    return E_INVALIDARG;

  int colspan;
  if (GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN, &colspan) &&
      colspan >= 1) {
    *n_columns_spanned = colspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnHeaderCells(
    IUnknown*** cell_accessibles,
    long* n_column_header_cells) {
  if (!instance_active())
    return E_FAIL;

  if (!cell_accessibles || !n_column_header_cells)
    return E_INVALIDARG;

  *n_column_header_cells = 0;

  int column;
  if (!GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX, &column)) {
    return S_FALSE;
  }

  BrowserAccessibility* table = GetParent();
  while (table && table->GetRole() != ui::AX_ROLE_TABLE)
    table = table->GetParent();
  if (!table) {
    NOTREACHED();
    return S_FALSE;
  }

  int columns;
  int rows;
  if (!table->GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !table->GetIntAttribute(
          ui::AX_ATTR_TABLE_ROW_COUNT, &rows)) {
    return S_FALSE;
  }
  if (columns <= 0 || rows <= 0 || column < 0 || column >= columns)
    return S_FALSE;

  const std::vector<int32>& cell_ids = table->GetIntListAttribute(
      ui::AX_ATTR_CELL_IDS);

  for (int i = 0; i < rows; ++i) {
    int cell_id = cell_ids[i * columns + column];
    BrowserAccessibilityWin* cell =
        manager()->GetFromID(cell_id)->ToBrowserAccessibilityWin();
    if (cell && cell->GetRole() == ui::AX_ROLE_COLUMN_HEADER)
      (*n_column_header_cells)++;
  }

  *cell_accessibles = static_cast<IUnknown**>(CoTaskMemAlloc(
      (*n_column_header_cells) * sizeof(cell_accessibles[0])));
  int index = 0;
  for (int i = 0; i < rows; ++i) {
    int cell_id = cell_ids[i * columns + column];
    BrowserAccessibility* cell = manager()->GetFromID(cell_id);
    if (cell && cell->GetRole() == ui::AX_ROLE_COLUMN_HEADER) {
      (*cell_accessibles)[index] = static_cast<IAccessible*>(
          cell->ToBrowserAccessibilityWin()->NewReference());
      ++index;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_columnIndex(long* column_index) {
  if (!instance_active())
    return E_FAIL;

  if (!column_index)
    return E_INVALIDARG;

  int column;
  if (GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX, &column)) {
    *column_index = column;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowExtent(long* n_rows_spanned) {
  if (!instance_active())
    return E_FAIL;

  if (!n_rows_spanned)
    return E_INVALIDARG;

  int rowspan;
  if (GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      rowspan >= 1) {
    *n_rows_spanned = rowspan;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowHeaderCells(
    IUnknown*** cell_accessibles,
    long* n_row_header_cells) {
  if (!instance_active())
    return E_FAIL;

  if (!cell_accessibles || !n_row_header_cells)
    return E_INVALIDARG;

  *n_row_header_cells = 0;

  int row;
  if (!GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_ROW_INDEX, &row)) {
    return S_FALSE;
  }

  BrowserAccessibility* table = GetParent();
  while (table && table->GetRole() != ui::AX_ROLE_TABLE)
    table = table->GetParent();
  if (!table) {
    NOTREACHED();
    return S_FALSE;
  }

  int columns;
  int rows;
  if (!table->GetIntAttribute(
          ui::AX_ATTR_TABLE_COLUMN_COUNT, &columns) ||
      !table->GetIntAttribute(
          ui::AX_ATTR_TABLE_ROW_COUNT, &rows)) {
    return S_FALSE;
  }
  if (columns <= 0 || rows <= 0 || row < 0 || row >= rows)
    return S_FALSE;

  const std::vector<int32>& cell_ids = table->GetIntListAttribute(
      ui::AX_ATTR_CELL_IDS);

  for (int i = 0; i < columns; ++i) {
    int cell_id = cell_ids[row * columns + i];
    BrowserAccessibility* cell = manager()->GetFromID(cell_id);
    if (cell && cell->GetRole() == ui::AX_ROLE_ROW_HEADER)
      (*n_row_header_cells)++;
  }

  *cell_accessibles = static_cast<IUnknown**>(CoTaskMemAlloc(
      (*n_row_header_cells) * sizeof(cell_accessibles[0])));
  int index = 0;
  for (int i = 0; i < columns; ++i) {
    int cell_id = cell_ids[row * columns + i];
    BrowserAccessibility* cell = manager()->GetFromID(cell_id);
    if (cell && cell->GetRole() == ui::AX_ROLE_ROW_HEADER) {
      (*cell_accessibles)[index] = static_cast<IAccessible*>(
          cell->ToBrowserAccessibilityWin()->NewReference());
      ++index;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowIndex(long* row_index) {
  if (!instance_active())
    return E_FAIL;

  if (!row_index)
    return E_INVALIDARG;

  int row;
  if (GetIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_INDEX, &row)) {
    *row_index = row;
    return S_OK;
  }
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_isSelected(boolean* is_selected) {
  if (!instance_active())
    return E_FAIL;

  if (!is_selected)
    return E_INVALIDARG;

  *is_selected = false;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_rowColumnExtents(
    long* row_index,
    long* column_index,
    long* row_extents,
    long* column_extents,
    boolean* is_selected) {
  if (!instance_active())
    return E_FAIL;

  if (!row_index ||
      !column_index ||
      !row_extents ||
      !column_extents ||
      !is_selected) {
    return E_INVALIDARG;
  }

  int row;
  int column;
  int rowspan;
  int colspan;
  if (GetIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_INDEX, &row) &&
      GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX, &column) &&
      GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_ROW_SPAN, &rowspan) &&
      GetIntAttribute(
          ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN, &colspan)) {
    *row_index = row;
    *column_index = column;
    *row_extents = rowspan;
    *column_extents = colspan;
    *is_selected = false;
    return S_OK;
  }

  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_table(IUnknown** table) {
  if (!instance_active())
    return E_FAIL;

  if (!table)
    return E_INVALIDARG;


  int row;
  int column;
  GetIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_INDEX, &row);
  GetIntAttribute(ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX, &column);

  BrowserAccessibility* find_table = GetParent();
  while (find_table && find_table->GetRole() != ui::AX_ROLE_TABLE)
    find_table = find_table->GetParent();
  if (!find_table) {
    NOTREACHED();
    return S_FALSE;
  }

  *table = static_cast<IAccessibleTable*>(
      find_table->ToBrowserAccessibilityWin()->NewReference());

  return S_OK;
}

//
// IAccessibleText methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_nCharacters(LONG* n_characters) {
  if (!instance_active())
    return E_FAIL;

  if (!n_characters)
    return E_INVALIDARG;

  *n_characters = TextForIAccessibleText().length();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_caretOffset(LONG* offset) {
  if (!instance_active())
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  // IA2 spec says that caret offset should be -1 if the object is not focused.
  if (manager()->GetFocus(this) != this) {
    *offset = -1;
    return S_FALSE;
  }

  *offset = 0;
  if (IsEditableText()) {
    int sel_start = 0;
    if (GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START,
                        &sel_start))
      *offset = sel_start;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_characterExtents(
    LONG offset,
    enum IA2CoordinateType coordinate_type,
    LONG* out_x,
    LONG* out_y,
    LONG* out_width,
    LONG* out_height) {
  if (!instance_active())
    return E_FAIL;

  if (!out_x || !out_y || !out_width || !out_height)
    return E_INVALIDARG;

  const base::string16& text_str = TextForIAccessibleText();
  HandleSpecialTextOffset(text_str, &offset);

  if (offset < 0 || offset > static_cast<LONG>(text_str.size()))
    return E_INVALIDARG;

  gfx::Rect character_bounds;
  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    character_bounds = GetGlobalBoundsForRange(offset, 1);
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    character_bounds = GetLocalBoundsForRange(offset, 1);
    character_bounds -= GetLocation().OffsetFromOrigin();
  } else {
    return E_INVALIDARG;
  }

  *out_x = character_bounds.x();
  *out_y = character_bounds.y();
  *out_width = character_bounds.width();
  *out_height = character_bounds.height();

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nSelections(LONG* n_selections) {
  if (!instance_active())
    return E_FAIL;

  if (!n_selections)
    return E_INVALIDARG;

  *n_selections = 0;
  if (IsEditableText()) {
    int sel_start = 0;
    int sel_end = 0;
    if (GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START,
                        &sel_start) &&
        GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END, &sel_end) &&
        sel_start != sel_end)
      *n_selections = 1;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_selection(LONG selection_index,
                                                    LONG* start_offset,
                                                    LONG* end_offset) {
  if (!instance_active())
    return E_FAIL;

  if (!start_offset || !end_offset || selection_index != 0)
    return E_INVALIDARG;

  LONG n_selections = 0;
  if (FAILED(get_nSelections(&n_selections)) || n_selections < 1)
    return E_INVALIDARG;

  *start_offset = 0;
  *end_offset = 0;
  if (IsEditableText()) {
    int sel_start = 0;
    int sel_end = 0;
    if (GetIntAttribute(
            ui::AX_ATTR_TEXT_SEL_START, &sel_start) &&
        GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END, &sel_end)) {
      *start_offset = sel_start;
      *end_offset = sel_end;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_text(LONG start_offset,
                                               LONG end_offset,
                                               BSTR* text) {
  if (!instance_active())
    return E_FAIL;

  if (!text)
    return E_INVALIDARG;

  const base::string16& text_str = TextForIAccessibleText();

  // Handle special text offsets.
  HandleSpecialTextOffset(text_str, &start_offset);
  HandleSpecialTextOffset(text_str, &end_offset);

  // The spec allows the arguments to be reversed.
  if (start_offset > end_offset) {
    LONG tmp = start_offset;
    start_offset = end_offset;
    end_offset = tmp;
  }

  // The spec does not allow the start or end offsets to be out or range;
  // we must return an error if so.
  LONG len = text_str.length();
  if (start_offset < 0)
    return E_INVALIDARG;
  if (end_offset > len)
    return E_INVALIDARG;

  base::string16 substr = text_str.substr(start_offset,
                                          end_offset - start_offset);
  if (substr.empty())
    return S_FALSE;

  *text = SysAllocString(substr.c_str());
  DCHECK(*text);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_textAtOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  if (!instance_active())
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  const base::string16& text_str = TextForIAccessibleText();
  HandleSpecialTextOffset(text_str, &offset);
  if (offset < 0)
    return E_INVALIDARG;

  LONG text_len = text_str.length();
  if (offset > text_len)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = NULL;
    return S_FALSE;
  }

  // According to the IA2 Spec, only line boundaries should succeed when
  // the offset is one past the end of the text.
  if (offset == text_len && boundary_type != IA2_TEXT_BOUNDARY_LINE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = nullptr;
    return S_FALSE;
  }

  *start_offset = FindBoundary(
      text_str, boundary_type, offset, ui::BACKWARDS_DIRECTION);
  *end_offset = FindBoundary(
      text_str, boundary_type, offset, ui::FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_textBeforeOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  if (!instance_active())
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = NULL;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = FindBoundary(
      text_str, boundary_type, offset, ui::BACKWARDS_DIRECTION);
  *end_offset = offset;
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_textAfterOffset(
    LONG offset,
    enum IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  if (!instance_active())
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE) {
    *start_offset = 0;
    *end_offset = 0;
    *text = NULL;
    return S_FALSE;
  }

  const base::string16& text_str = TextForIAccessibleText();

  *start_offset = offset;
  *end_offset = FindBoundary(
      text_str, boundary_type, offset, ui::FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityWin::get_newText(IA2TextSegment* new_text) {
  if (!instance_active())
    return E_FAIL;

  if (!new_text)
    return E_INVALIDARG;

  if (!old_win_attributes_)
    return E_FAIL;

  int start, old_len, new_len;
  ComputeHypertextRemovedAndInserted(&start, &old_len, &new_len);
  if (new_len == 0)
    return E_FAIL;

  base::string16 substr = hypertext().substr(start, new_len);
  new_text->text = SysAllocString(substr.c_str());
  new_text->start = static_cast<long>(start);
  new_text->end = static_cast<long>(start + new_len);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_oldText(IA2TextSegment* old_text) {
  if (!instance_active())
    return E_FAIL;

  if (!old_text)
    return E_INVALIDARG;

  if (!old_win_attributes_)
    return E_FAIL;

  int start, old_len, new_len;
  ComputeHypertextRemovedAndInserted(&start, &old_len, &new_len);
  if (old_len == 0)
    return E_FAIL;

  base::string16 old_hypertext = old_win_attributes_->hypertext;
  base::string16 substr = old_hypertext.substr(start, old_len);
  old_text->text = SysAllocString(substr.c_str());
  old_text->start = static_cast<long>(start);
  old_text->end = static_cast<long>(start + old_len);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_offsetAtPoint(
    LONG x,
    LONG y,
    enum IA2CoordinateType coord_type,
    LONG* offset) {
  if (!instance_active())
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement this. We're returning S_OK for now so that
  // screen readers still return partially accurate results rather than
  // completely failing.
  *offset = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollSubstringTo(
    LONG start_index,
    LONG end_index,
    enum IA2ScrollType scroll_type) {
  // TODO(dmazzoni): adjust this for the start and end index, too.
  return scrollTo(scroll_type);
}

STDMETHODIMP BrowserAccessibilityWin::scrollSubstringToPoint(
    LONG start_index,
    LONG end_index,
    enum IA2CoordinateType coordinate_type,
    LONG x, LONG y) {
  // TODO(dmazzoni): adjust this for the start and end index, too.
  return scrollToPoint(coordinate_type, x, y);
}

STDMETHODIMP BrowserAccessibilityWin::addSelection(LONG start_offset,
                                                   LONG end_offset) {
  if (!instance_active())
    return E_FAIL;

  const base::string16& text_str = TextForIAccessibleText();
  HandleSpecialTextOffset(text_str, &start_offset);
  HandleSpecialTextOffset(text_str, &end_offset);

  manager()->SetTextSelection(*this, start_offset, end_offset);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::removeSelection(LONG selection_index) {
  if (!instance_active())
    return E_FAIL;

  if (selection_index != 0)
    return E_INVALIDARG;

  manager()->SetTextSelection(*this, 0, 0);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::setCaretOffset(LONG offset) {
  if (!instance_active())
    return E_FAIL;

  const base::string16& text_str = TextForIAccessibleText();
  HandleSpecialTextOffset(text_str, &offset);
  manager()->SetTextSelection(*this, offset, offset);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::setSelection(LONG selection_index,
                                                   LONG start_offset,
                                                   LONG end_offset) {
  if (!instance_active())
    return E_FAIL;

  if (selection_index != 0)
    return E_INVALIDARG;

  const base::string16& text_str = TextForIAccessibleText();
  HandleSpecialTextOffset(text_str, &start_offset);
  HandleSpecialTextOffset(text_str, &end_offset);

  manager()->SetTextSelection(*this, start_offset, end_offset);
  return S_OK;
}

//
// IAccessibleText methods not implemented.
//

STDMETHODIMP BrowserAccessibilityWin::get_attributes(LONG offset,
                                                     LONG* start_offset,
                                                     LONG* end_offset,
                                                     BSTR* text_attributes) {
  return E_NOTIMPL;
}

//
// IAccessibleHypertext methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_nHyperlinks(long* hyperlink_count) {
  if (!instance_active())
    return E_FAIL;

  if (!hyperlink_count)
    return E_INVALIDARG;

  *hyperlink_count = hyperlink_offset_to_index().size();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_hyperlink(
    long index,
    IAccessibleHyperlink** hyperlink) {
  if (!instance_active())
    return E_FAIL;

  if (!hyperlink ||
      index < 0 ||
      index >= static_cast<long>(hyperlinks().size())) {
    return E_INVALIDARG;
  }

  int32 id = hyperlinks()[index];
  BrowserAccessibilityWin* child =
      manager()->GetFromID(id)->ToBrowserAccessibilityWin();
  if (child) {
    *hyperlink = static_cast<IAccessibleHyperlink*>(child->NewReference());
    return S_OK;
  }

  return E_FAIL;
}

STDMETHODIMP BrowserAccessibilityWin::get_hyperlinkIndex(
    long char_index,
    long* hyperlink_index) {
  if (!instance_active())
    return E_FAIL;

  if (!hyperlink_index)
    return E_INVALIDARG;

  *hyperlink_index = -1;

  if (char_index < 0 ||
      char_index >= static_cast<long>(hypertext().size())) {
    return E_INVALIDARG;
  }

  std::map<int32, int32>::iterator it =
      hyperlink_offset_to_index().find(char_index);
  if (it == hyperlink_offset_to_index().end())
    return E_FAIL;

  *hyperlink_index = it->second;
  return S_OK;
}

//
// IAccessibleHyperlink not implemented.
//

STDMETHODIMP BrowserAccessibilityWin::get_anchor(long index, VARIANT* anchor) {
  return E_NOTIMPL;
}
STDMETHODIMP
BrowserAccessibilityWin::get_anchorTarget(long index, VARIANT* anchor_target) {
  return E_NOTIMPL;
}
STDMETHODIMP BrowserAccessibilityWin::get_startIndex(long* index) {
  return E_NOTIMPL;
}
STDMETHODIMP BrowserAccessibilityWin::get_endIndex(long* index) {
  return E_NOTIMPL;
}
STDMETHODIMP BrowserAccessibilityWin::get_valid(boolean* valid) {
  return E_NOTIMPL;
}

//
// IAccessibleAction not implemented.
//

STDMETHODIMP BrowserAccessibilityWin::nActions(long* n_actions) {
  return E_NOTIMPL;
}
STDMETHODIMP BrowserAccessibilityWin::doAction(long action_index) {
  return E_NOTIMPL;
}
STDMETHODIMP
BrowserAccessibilityWin::get_description(long action_index, BSTR* description) {
  return E_NOTIMPL;
}
STDMETHODIMP BrowserAccessibilityWin::get_keyBinding(long action_index,
                                                     long n_max_bindings,
                                                     BSTR** key_bindings,
                                                     long* n_bindings) {
  return E_NOTIMPL;
}
STDMETHODIMP BrowserAccessibilityWin::get_name(long action_index, BSTR* name) {
  return E_NOTIMPL;
}
STDMETHODIMP
BrowserAccessibilityWin::get_localizedName(long action_index,
                                           BSTR* localized_name) {
  return E_NOTIMPL;
}

//
// IAccessibleValue methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_currentValue(VARIANT* value) {
  if (!instance_active())
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(
          ui::AX_ATTR_VALUE_FOR_RANGE, &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_minimumValue(VARIANT* value) {
  if (!instance_active())
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::get_maximumValue(VARIANT* value) {
  if (!instance_active())
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityWin::setCurrentValue(VARIANT new_value) {
  // TODO(dmazzoni): Implement this.
  return E_NOTIMPL;
}

//
// ISimpleDOMDocument methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_URL(BSTR* url) {
  if (!instance_active())
    return E_FAIL;

  if (!url)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(ui::AX_ATTR_DOC_URL, url);
}

STDMETHODIMP BrowserAccessibilityWin::get_title(BSTR* title) {
  if (!instance_active())
    return E_FAIL;

  if (!title)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(ui::AX_ATTR_DOC_TITLE, title);
}

STDMETHODIMP BrowserAccessibilityWin::get_mimeType(BSTR* mime_type) {
  if (!instance_active())
    return E_FAIL;

  if (!mime_type)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(
      ui::AX_ATTR_DOC_MIMETYPE, mime_type);
}

STDMETHODIMP BrowserAccessibilityWin::get_docType(BSTR* doc_type) {
  if (!instance_active())
    return E_FAIL;

  if (!doc_type)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(
      ui::AX_ATTR_DOC_DOCTYPE, doc_type);
}

STDMETHODIMP
BrowserAccessibilityWin::get_nameSpaceURIForID(short name_space_id,
                                               BSTR* name_space_uri) {
  return E_NOTIMPL;
}
STDMETHODIMP
BrowserAccessibilityWin::put_alternateViewMediaTypes(
    BSTR* comma_separated_media_types) {
  return E_NOTIMPL;
}

//
// ISimpleDOMNode methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_nodeInfo(
    BSTR* node_name,
    short* name_space_id,
    BSTR* node_value,
    unsigned int* num_children,
    unsigned int* unique_id,
    unsigned short* node_type) {
  if (!instance_active())
    return E_FAIL;

  if (!node_name || !name_space_id || !node_value || !num_children ||
      !unique_id || !node_type) {
    return E_INVALIDARG;
  }

  base::string16 tag;
  if (GetString16Attribute(ui::AX_ATTR_HTML_TAG, &tag))
    *node_name = SysAllocString(tag.c_str());
  else
    *node_name = NULL;

  *name_space_id = 0;
  *node_value = SysAllocString(value().c_str());
  *num_children = PlatformChildCount();
  *unique_id = unique_id_win_;

  if (ia_role() == ROLE_SYSTEM_DOCUMENT) {
    *node_type = NODETYPE_DOCUMENT;
  } else if (ia_role() == ROLE_SYSTEM_TEXT &&
             ((ia2_state() & IA2_STATE_EDITABLE) == 0)) {
    *node_type = NODETYPE_TEXT;
  } else {
    *node_type = NODETYPE_ELEMENT;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_attributes(
    unsigned short max_attribs,
    BSTR* attrib_names,
    short* name_space_id,
    BSTR* attrib_values,
    unsigned short* num_attribs) {
  if (!instance_active())
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values || !num_attribs)
    return E_INVALIDARG;

  *num_attribs = max_attribs;
  if (*num_attribs > GetHtmlAttributes().size())
    *num_attribs = GetHtmlAttributes().size();

  for (unsigned short i = 0; i < *num_attribs; ++i) {
    attrib_names[i] = SysAllocString(
        base::UTF8ToUTF16(GetHtmlAttributes()[i].first).c_str());
    name_space_id[i] = 0;
    attrib_values[i] = SysAllocString(
        base::UTF8ToUTF16(GetHtmlAttributes()[i].second).c_str());
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_attributesForNames(
    unsigned short num_attribs,
    BSTR* attrib_names,
    short* name_space_id,
    BSTR* attrib_values) {
  if (!instance_active())
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values)
    return E_INVALIDARG;

  for (unsigned short i = 0; i < num_attribs; ++i) {
    name_space_id[i] = 0;
    bool found = false;
    std::string name = base::UTF16ToUTF8((LPCWSTR)attrib_names[i]);
    for (unsigned int j = 0;  j < GetHtmlAttributes().size(); ++j) {
      if (GetHtmlAttributes()[j].first == name) {
        attrib_values[i] = SysAllocString(
            base::UTF8ToUTF16(GetHtmlAttributes()[j].second).c_str());
        found = true;
        break;
      }
    }
    if (!found) {
      attrib_values[i] = NULL;
    }
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_computedStyle(
    unsigned short max_style_properties,
    boolean use_alternate_view,
    BSTR* style_properties,
    BSTR* style_values,
    unsigned short *num_style_properties)  {
  if (!instance_active())
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  base::string16 display;
  if (max_style_properties == 0 ||
      !GetString16Attribute(ui::AX_ATTR_DISPLAY, &display)) {
    *num_style_properties = 0;
    return S_OK;
  }

  *num_style_properties = 1;
  style_properties[0] = SysAllocString(L"display");
  style_values[0] = SysAllocString(display.c_str());

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_computedStyleForProperties(
    unsigned short num_style_properties,
    boolean use_alternate_view,
    BSTR* style_properties,
    BSTR* style_values) {
  if (!instance_active())
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  for (unsigned short i = 0; i < num_style_properties; ++i) {
    base::string16 name = (LPCWSTR)style_properties[i];
    base::StringToLowerASCII(&name);
    if (name == L"display") {
      base::string16 display = GetString16Attribute(
          ui::AX_ATTR_DISPLAY);
      style_values[i] = SysAllocString(display.c_str());
    } else {
      style_values[i] = NULL;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollTo(boolean placeTopLeft) {
  return scrollTo(placeTopLeft ?
      IA2_SCROLL_TYPE_TOP_LEFT : IA2_SCROLL_TYPE_ANYWHERE);
}

STDMETHODIMP BrowserAccessibilityWin::get_parentNode(ISimpleDOMNode** node) {
  if (!instance_active())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  *node = GetParent()->ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_firstChild(ISimpleDOMNode** node)  {
  if (!instance_active())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (PlatformChildCount() == 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = PlatformGetChild(0)->ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_lastChild(ISimpleDOMNode** node) {
  if (!instance_active())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (PlatformChildCount() == 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = PlatformGetChild(PlatformChildCount() - 1)
      ->ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_previousSibling(
    ISimpleDOMNode** node) {
  if (!instance_active())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (!GetParent() || GetIndexInParent() <= 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = GetParent()->InternalGetChild(GetIndexInParent() - 1)->
      ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_nextSibling(ISimpleDOMNode** node) {
  if (!instance_active())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (!GetParent() ||
      GetIndexInParent() < 0 ||
      GetIndexInParent() >= static_cast<int>(
          GetParent()->InternalChildCount()) - 1) {
    *node = NULL;
    return S_FALSE;
  }

  *node = GetParent()->InternalGetChild(GetIndexInParent() + 1)->
      ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_childAt(
    unsigned int child_index,
    ISimpleDOMNode** node) {
  if (!instance_active())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (child_index >= PlatformChildCount())
    return E_INVALIDARG;

  BrowserAccessibility* child = PlatformGetChild(child_index);
  if (!child) {
    *node = NULL;
    return S_FALSE;
  }

  *node = child->ToBrowserAccessibilityWin()->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_innerHTML(BSTR* innerHTML) {
  return E_NOTIMPL;
}

STDMETHODIMP
BrowserAccessibilityWin::get_localInterface(void** local_interface) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_language(BSTR* language) {
  return E_NOTIMPL;
}

//
// ISimpleDOMText methods.
//

STDMETHODIMP BrowserAccessibilityWin::get_domText(BSTR* dom_text) {
  if (!instance_active())
    return E_FAIL;

  if (!dom_text)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(
      ui::AX_ATTR_NAME, dom_text);
}

STDMETHODIMP BrowserAccessibilityWin::get_clippedSubstringBounds(
    unsigned int start_index,
    unsigned int end_index,
    int* out_x,
    int* out_y,
    int* out_width,
    int* out_height) {
  // TODO(dmazzoni): fully support this API by intersecting the
  // rect with the container's rect.
  return get_unclippedSubstringBounds(
      start_index, end_index, out_x, out_y, out_width, out_height);
}

STDMETHODIMP BrowserAccessibilityWin::get_unclippedSubstringBounds(
    unsigned int start_index,
    unsigned int end_index,
    int* out_x,
    int* out_y,
    int* out_width,
    int* out_height) {
  if (!instance_active())
    return E_FAIL;

  if (!out_x || !out_y || !out_width || !out_height)
    return E_INVALIDARG;

  const base::string16& text_str = TextForIAccessibleText();
  if (start_index > text_str.size() ||
      end_index > text_str.size() ||
      start_index > end_index) {
    return E_INVALIDARG;
  }

  gfx::Rect bounds = GetGlobalBoundsForRange(
      start_index, end_index - start_index);
  *out_x = bounds.x();
  *out_y = bounds.y();
  *out_width = bounds.width();
  *out_height = bounds.height();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::scrollToSubstring(
    unsigned int start_index,
    unsigned int end_index) {
  if (!instance_active())
    return E_FAIL;

  const base::string16& text_str = TextForIAccessibleText();
  if (start_index > text_str.size() ||
      end_index > text_str.size() ||
      start_index > end_index) {
    return E_INVALIDARG;
  }

  manager()->ScrollToMakeVisible(*this, GetLocalBoundsForRange(
      start_index, end_index - start_index));
  manager()->ToBrowserAccessibilityManagerWin()->TrackScrollingObject(this);

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_fontFamily(BSTR* font_family) {
  return E_NOTIMPL;
}

//
// IServiceProvider methods.
//

STDMETHODIMP BrowserAccessibilityWin::QueryService(REFGUID guidService,
                                                   REFIID riid,
                                                   void** object) {
  if (!instance_active())
    return E_FAIL;

  // The system uses IAccessible APIs for many purposes, but only
  // assistive technology like screen readers uses IAccessible2.
  // Enable full accessibility support when IAccessible2 APIs are queried.
  if (riid == IID_IAccessible2)
    BrowserAccessibilityStateImpl::GetInstance()->EnableAccessibility();

  if (guidService == GUID_IAccessibleContentDocument) {
    // Special Mozilla extension: return the accessible for the root document.
    // Screen readers use this to distinguish between a document loaded event
    // on the root document vs on an iframe.
    return manager()->GetRoot()->ToBrowserAccessibilityWin()->QueryInterface(
        IID_IAccessible2, object);
  }

  if (guidService == IID_IAccessible ||
      guidService == IID_IAccessible2 ||
      guidService == IID_IAccessibleAction ||
      guidService == IID_IAccessibleApplication ||
      guidService == IID_IAccessibleHyperlink ||
      guidService == IID_IAccessibleHypertext ||
      guidService == IID_IAccessibleImage ||
      guidService == IID_IAccessibleTable ||
      guidService == IID_IAccessibleTable2 ||
      guidService == IID_IAccessibleTableCell ||
      guidService == IID_IAccessibleText ||
      guidService == IID_IAccessibleValue ||
      guidService == IID_ISimpleDOMDocument ||
      guidService == IID_ISimpleDOMNode ||
      guidService == IID_ISimpleDOMText ||
      guidService == GUID_ISimpleDOM) {
    return QueryInterface(riid, object);
  }

  // We only support the IAccessibleEx interface on Windows 8 and above. This
  // is needed for the on-screen Keyboard to show up in metro mode, when the
  // user taps an editable portion on the page.
  // All methods in the IAccessibleEx interface are unimplemented.
  if (riid == IID_IAccessibleEx &&
      base::win::GetVersion() >= base::win::VERSION_WIN8) {
    return QueryInterface(riid, object);
  }

  *object = NULL;
  return E_FAIL;
}

STDMETHODIMP
BrowserAccessibilityWin::GetObjectForChild(long child_id, IAccessibleEx** ret) {
  return E_NOTIMPL;
}

STDMETHODIMP
BrowserAccessibilityWin::GetIAccessiblePair(IAccessible** acc, long* child_id) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::GetRuntimeId(SAFEARRAY** runtime_id) {
  return E_NOTIMPL;
}

STDMETHODIMP
BrowserAccessibilityWin::ConvertReturnedElement(
    IRawElementProviderSimple* element,
    IAccessibleEx** acc) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::GetPatternProvider(PATTERNID id,
                                                         IUnknown** provider) {
  DVLOG(1) << "In Function: "
           << __FUNCTION__
           << " for pattern id: "
           << id;
  if (id == UIA_ValuePatternId || id == UIA_TextPatternId) {
    if (IsEditableText()) {
      DVLOG(1) << "Returning UIA text provider";
      base::win::UIATextProvider::CreateTextProvider(
          GetValueText(), true, provider);
      return S_OK;
    }
  }
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::GetPropertyValue(PROPERTYID id,
                                                       VARIANT* ret) {
  DVLOG(1) << "In Function: "
           << __FUNCTION__
           << " for property id: "
           << id;
  V_VT(ret) = VT_EMPTY;
  if (id == UIA_ControlTypePropertyId) {
    if (IsEditableText()) {
      V_VT(ret) = VT_I4;
      ret->lVal = UIA_EditControlTypeId;
      DVLOG(1) << "Returning Edit control type";
    } else {
      DVLOG(1) << "Returning empty control type";
    }
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityWin::get_ProviderOptions(
    enum ProviderOptions* ret) {
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityWin::get_HostRawElementProvider(
    IRawElementProviderSimple** provider) {
  return E_NOTIMPL;
}

//
// CComObjectRootEx methods.
//

// static
HRESULT WINAPI BrowserAccessibilityWin::InternalQueryInterface(
    void* this_ptr,
    const _ATL_INTMAP_ENTRY* entries,
    REFIID iid,
    void** object) {
  BrowserAccessibilityWin* accessibility =
      reinterpret_cast<BrowserAccessibilityWin*>(this_ptr);
  int32 ia_role = accessibility->ia_role();
  if (iid == IID_IAccessibleImage) {
    if (ia_role != ROLE_SYSTEM_GRAPHIC) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleTable || iid == IID_IAccessibleTable2) {
    if (ia_role != ROLE_SYSTEM_TABLE) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleTableCell) {
    if (!accessibility->IsCellOrTableHeaderRole()) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleValue) {
    if (ia_role != ROLE_SYSTEM_PROGRESSBAR &&
        ia_role != ROLE_SYSTEM_SCROLLBAR &&
        ia_role != ROLE_SYSTEM_SLIDER) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_ISimpleDOMDocument) {
    if (ia_role != ROLE_SYSTEM_DOCUMENT) {
      *object = NULL;
      return E_NOINTERFACE;
    }
  }

  return CComObjectRootBase::InternalQueryInterface(
      this_ptr, entries, iid, object);
}

//
// Private methods.
//

void BrowserAccessibilityWin::UpdateStep1ComputeWinAttributes() {
  // Swap win_attributes_ to old_win_attributes_, allowing us to see
  // exactly what changed and fire appropriate events. Note that
  // old_win_attributes_ is cleared at the end of UpdateStep3FireEvents.
  old_win_attributes_.swap(win_attributes_);
  win_attributes_.reset(new WinAttributes());

  InitRoleAndState();

  win_attributes_->ia2_attributes.clear();

  // Expose autocomplete attribute for combobox and textbox.
  StringAttributeToIA2(ui::AX_ATTR_AUTO_COMPLETE, "autocomplete");

  // Expose the "display" and "tag" attributes.
  StringAttributeToIA2(ui::AX_ATTR_DISPLAY, "display");
  StringAttributeToIA2(ui::AX_ATTR_HTML_TAG, "tag");
  StringAttributeToIA2(ui::AX_ATTR_ROLE, "xml-roles");

  // Expose "level" attribute for headings, trees, etc.
  IntAttributeToIA2(ui::AX_ATTR_HIERARCHICAL_LEVEL, "level");

  // Expose the set size and position in set.
  IntAttributeToIA2(ui::AX_ATTR_SET_SIZE, "setsize");
  IntAttributeToIA2(ui::AX_ATTR_POS_IN_SET, "posinset");

  if (ia_role() == ROLE_SYSTEM_CHECKBUTTON ||
      ia_role() == ROLE_SYSTEM_RADIOBUTTON ||
      ia2_role() == IA2_ROLE_CHECK_MENU_ITEM ||
      ia2_role() == IA2_ROLE_RADIO_MENU_ITEM ||
      ia2_role() == IA2_ROLE_TOGGLE_BUTTON) {
    win_attributes_->ia2_attributes.push_back(L"checkable:true");
  }

  // Expose live region attributes.
  StringAttributeToIA2(ui::AX_ATTR_LIVE_STATUS, "live");
  StringAttributeToIA2(ui::AX_ATTR_LIVE_RELEVANT, "relevant");
  BoolAttributeToIA2(ui::AX_ATTR_LIVE_ATOMIC, "atomic");
  BoolAttributeToIA2(ui::AX_ATTR_LIVE_BUSY, "busy");

  // Expose container live region attributes.
  StringAttributeToIA2(ui::AX_ATTR_CONTAINER_LIVE_STATUS,
                       "container-live");
  StringAttributeToIA2(ui::AX_ATTR_CONTAINER_LIVE_RELEVANT,
                       "container-relevant");
  BoolAttributeToIA2(ui::AX_ATTR_CONTAINER_LIVE_ATOMIC,
                     "container-atomic");
  BoolAttributeToIA2(ui::AX_ATTR_CONTAINER_LIVE_BUSY,
                     "container-busy");

  // Expose table cell index.
  if (IsCellOrTableHeaderRole()) {
    BrowserAccessibility* table = GetParent();
    while (table && table->GetRole() != ui::AX_ROLE_TABLE)
      table = table->GetParent();
    if (table) {
      const std::vector<int32>& unique_cell_ids = table->GetIntListAttribute(
          ui::AX_ATTR_UNIQUE_CELL_IDS);
      for (size_t i = 0; i < unique_cell_ids.size(); ++i) {
        if (unique_cell_ids[i] == GetId()) {
          win_attributes_->ia2_attributes.push_back(
              base::string16(L"table-cell-index:") + base::IntToString16(i));
        }
      }
    }
  }

  // Expose invalid state for form controls and elements with aria-invalid.
  int invalid_state;
  if (GetIntAttribute(ui::AX_ATTR_INVALID_STATE, &invalid_state)) {
    // TODO(nektar): Handle the possibility of having multiple aria-invalid
    // attributes defined, e.g., "invalid:spelling,grammar".
    switch (invalid_state) {
      case ui::AX_INVALID_STATE_FALSE:
        win_attributes_->ia2_attributes.push_back(L"invalid:false");
        break;
      case ui::AX_INVALID_STATE_TRUE:
        win_attributes_->ia2_attributes.push_back(L"invalid:true");
        break;
      case ui::AX_INVALID_STATE_SPELLING:
        win_attributes_->ia2_attributes.push_back(L"invalid:spelling");
        break;
      case ui::AX_INVALID_STATE_GRAMMAR:
        win_attributes_->ia2_attributes.push_back(L"invalid:grammar");
        break;
      case ui::AX_INVALID_STATE_OTHER:
        {
          base::string16 aria_invalid_value;
          if (GetString16Attribute(ui::AX_ATTR_ARIA_INVALID_VALUE,
                                   &aria_invalid_value)) {
            win_attributes_->ia2_attributes.push_back(
                L"invalid:" + aria_invalid_value);
          } else {
            // Set the attribute to L"true", since we cannot be more specific.
            win_attributes_->ia2_attributes.push_back(L"invalid:true");
          }
        }
        break;
      default:
        NOTREACHED();
    }
  }

  // Expose row or column header sort direction.
  int32 sort_direction;
  if ((ia_role() == ROLE_SYSTEM_COLUMNHEADER ||
      ia_role() == ROLE_SYSTEM_ROWHEADER) &&
      GetIntAttribute(ui::AX_ATTR_SORT_DIRECTION, &sort_direction)) {
    switch (sort_direction) {
      case ui::AX_SORT_DIRECTION_UNSORTED:
        win_attributes_->ia2_attributes.push_back(L"sort:none");
        break;
      case ui::AX_SORT_DIRECTION_ASCENDING:
        win_attributes_->ia2_attributes.push_back(L"sort:ascending");
        break;
      case ui::AX_SORT_DIRECTION_DESCENDING:
        win_attributes_->ia2_attributes.push_back(L"sort:descending");
        break;
      case ui::AX_SORT_DIRECTION_OTHER:
        win_attributes_->ia2_attributes.push_back(L"sort:other");
        break;
      default:
        NOTREACHED();
    }
  }

  // The calculation of the accessible name of an element has been
  // standardized in the HTML to Platform Accessibility APIs Implementation
  // Guide (http://www.w3.org/TR/html-aapi/). In order to return the
  // appropriate accessible name on Windows, we need to apply some logic
  // to the fields we get from WebKit.
  //
  // TODO(dmazzoni): move most of this logic into WebKit.
  //
  // WebKit gives us:
  //
  //   name: the default name, e.g. inner text
  //   title ui element: a reference to a <label> element on the same
  //       page that labels this node.
  //   description: accessible labels that override the default name:
  //       aria-label or aria-labelledby or aria-describedby
  //   help: the value of the "title" attribute
  //
  // On Windows, the logic we apply lets some fields take precedence and
  // always returns the primary name in "name" and the secondary name,
  // if any, in "description".

  int title_elem_id = GetIntAttribute(ui::AX_ATTR_TITLE_UI_ELEMENT);
  base::string16 name = GetString16Attribute(ui::AX_ATTR_NAME);
  base::string16 description = GetString16Attribute(ui::AX_ATTR_DESCRIPTION);
  base::string16 help = GetString16Attribute(ui::AX_ATTR_HELP);
  base::string16 value = GetString16Attribute(ui::AX_ATTR_VALUE);

  // WebKit annoyingly puts the title in the description if there's no other
  // description, which just confuses the rest of the logic. Put it back.
  // Now "help" is always the value of the "title" attribute, if present.
  base::string16 title_attr;
  if (GetHtmlAttribute("title", &title_attr) &&
      description == title_attr &&
      help.empty()) {
    help = description;
    description.clear();
  }

  // Now implement the main logic: the descripion should become the name if
  // it's nonempty, and the help should become the description if
  // there's no description - or the name if there's no name or description.
  if (!description.empty()) {
    name = description;
    description.clear();
  }
  if (!help.empty() && description.empty()) {
    description = help;
    help.clear();
  }
  if (!description.empty() && name.empty() && !title_elem_id) {
    name = description;
    description.clear();
  }

  // If it's a text field, also consider the placeholder.
  base::string16 placeholder;
  if (GetRole() == ui::AX_ROLE_TEXT_FIELD &&
      HasState(ui::AX_STATE_FOCUSABLE) &&
      GetHtmlAttribute("placeholder", &placeholder)) {
    if (name.empty() && !title_elem_id) {
      name = placeholder;
    } else if (description.empty()) {
      description = placeholder;
    }
  }

  // On Windows, the value of a document should be its url.
  if (GetRole() == ui::AX_ROLE_ROOT_WEB_AREA ||
      GetRole() == ui::AX_ROLE_WEB_AREA) {
    value = GetString16Attribute(ui::AX_ATTR_DOC_URL);
  }

  // For certain roles (listbox option, static text, and list marker)
  // WebKit stores the main accessible text in the "value" - swap it so
  // that it's the "name".
  if (name.empty() &&
      (GetRole() == ui::AX_ROLE_STATIC_TEXT ||
       GetRole() == ui::AX_ROLE_LIST_MARKER ||
       IsListBoxOptionOrMenuListOption())) {
    base::string16 tmp = value;
    value = name;
    name = tmp;
  }

  // If this doesn't have a value and is linked then set its value to the url
  // attribute. This allows screen readers to read an empty link's destination.
  if (value.empty() && (ia_state() & STATE_SYSTEM_LINKED))
    value = GetString16Attribute(ui::AX_ATTR_URL);

  win_attributes_->name = name;
  win_attributes_->description = description;
  win_attributes_->help = help;
  win_attributes_->value = value;

  // Clear any old relationships between this node and other nodes.
  for (size_t i = 0; i < relations_.size(); ++i)
    relations_[i]->Release();
  relations_.clear();

  // Handle title UI element.
  if (title_elem_id) {
    // Add a labelled by relationship.
    CComObject<BrowserAccessibilityRelation>* relation;
    HRESULT hr = CComObject<BrowserAccessibilityRelation>::CreateInstance(
        &relation);
    DCHECK(SUCCEEDED(hr));
    relation->AddRef();
    relation->Initialize(this, IA2_RELATION_LABELLED_BY);
    relation->AddTarget(title_elem_id);
    relations_.push_back(relation);
  }

  // Expose slider value.
  if (ia_role() == ROLE_SYSTEM_PROGRESSBAR ||
      ia_role() == ROLE_SYSTEM_SCROLLBAR ||
      ia_role() == ROLE_SYSTEM_SLIDER) {
    win_attributes_->ia2_attributes.push_back(L"valuetext:" + GetValueText());
  }

  // Expose dropeffect attribute.
  base::string16 dropEffect;
  if (GetHtmlAttribute("aria-dropeffect", &dropEffect))
    win_attributes_->ia2_attributes.push_back(L"dropeffect:" + dropEffect);

  // Expose grabbed attribute.
  base::string16 grabbed;
  if (GetHtmlAttribute("aria-grabbed", &grabbed))
    win_attributes_->ia2_attributes.push_back(L"grabbed:" + grabbed);

  // Expose datetime attribute.
  base::string16 datetime;
  if (GetRole() == ui::AX_ROLE_TIME &&
      GetHtmlAttribute("datetime", &datetime))
    win_attributes_->ia2_attributes.push_back(L"datetime:" + datetime);

  // Expose input-text type attribute.
  base::string16 type;
  if (GetRole() == ui::AX_ROLE_TEXT_FIELD &&
      GetHtmlAttribute("type", &type))
    win_attributes_->ia2_attributes.push_back(L"text-input-type:" + type);

  // If this is a web area for a presentational iframe, give it a role of
  // something other than DOCUMENT so that the fact that it's a separate doc
  // is not exposed to AT.
  if (IsWebAreaForPresentationalIframe()) {
    win_attributes_->ia_role = ROLE_SYSTEM_GROUPING;
    win_attributes_->ia2_role = ROLE_SYSTEM_GROUPING;
  }
}

void BrowserAccessibilityWin::UpdateStep2ComputeHypertext() {
  // Construct the hypertext for this node, which contains the concatenation
  // of all of the static text of this node's children and an embedded object
  // character for all non-static-text children. Build up a map from the
  // character index of each embedded object character to the id of the
  // child object it points to.
  for (unsigned int i = 0; i < PlatformChildCount(); ++i) {
    BrowserAccessibilityWin* child =
        PlatformGetChild(i)->ToBrowserAccessibilityWin();
    if (child->GetRole() == ui::AX_ROLE_STATIC_TEXT) {
      win_attributes_->hypertext += child->name();
    } else {
      int32 char_offset = hypertext().size();
      int32 child_id = child->GetId();
      int32 index = hyperlinks().size();
      win_attributes_->hyperlink_offset_to_index[char_offset] = index;
      win_attributes_->hyperlinks.push_back(child_id);
      win_attributes_->hypertext += kEmbeddedCharacter;
    }
  }
}

void BrowserAccessibilityWin::UpdateStep3FireEvents(bool is_subtree_creation) {
  BrowserAccessibilityManagerWin* manager =
      this->manager()->ToBrowserAccessibilityManagerWin();

  // Fire an event when an alert first appears.
  if (ia_role() == ROLE_SYSTEM_ALERT &&
      old_win_attributes_->ia_role != ROLE_SYSTEM_ALERT) {
    manager->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, this);
  }

  // Fire an event when a new subtree is created.
  if (is_subtree_creation)
    manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_SHOW, this);

  // The rest of the events only fire on changes, not on new objects.
  if (old_win_attributes_->ia_role != 0 ||
      !old_win_attributes_->role_name.empty()) {
    // Fire an event if the name, description, help, or value changes.
    if (name() != old_win_attributes_->name)
      manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_NAMECHANGE, this);
    if (description() != old_win_attributes_->description)
      manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_DESCRIPTIONCHANGE, this);
    if (help() != old_win_attributes_->help)
      manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_HELPCHANGE, this);
    if (value() != old_win_attributes_->value)
      manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_VALUECHANGE, this);
    if (ia_state() != old_win_attributes_->ia_state)
      manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_STATECHANGE, this);

    // Normally focus events are handled elsewhere, however
    // focus for managed descendants is platform-specific.
    // Fire a focus event if the focused descendant in a multi-select
    // list box changes.
    if (GetRole() == ui::AX_ROLE_LIST_BOX_OPTION &&
        (ia_state() & STATE_SYSTEM_FOCUSABLE) &&
        (ia_state() & STATE_SYSTEM_SELECTABLE) &&
        (ia_state() & STATE_SYSTEM_FOCUSED) &&
        !(old_win_attributes_->ia_state & STATE_SYSTEM_FOCUSED)) {
      manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_FOCUS, this);
    }

    // Handle selection being added or removed.
    bool is_selected_now = (ia_state() & STATE_SYSTEM_SELECTED) != 0;
    bool was_selected_before =
        (old_win_attributes_->ia_state & STATE_SYSTEM_SELECTED) != 0;
    if (is_selected_now || was_selected_before) {
      bool multiselect = false;
      if (GetParent() && GetParent()->HasState(ui::AX_STATE_MULTISELECTABLE))
        multiselect = true;

      if (multiselect) {
        // In a multi-select box, fire SELECTIONADD and SELECTIONREMOVE events.
        if (is_selected_now && !was_selected_before) {
          manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_SELECTIONADD, this);
        } else if (!is_selected_now && was_selected_before) {
          manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_SELECTIONREMOVE, this);
        }
      } else if (is_selected_now && !was_selected_before) {
        // In a single-select box, only fire SELECTION events.
        manager->MaybeCallNotifyWinEvent(EVENT_OBJECT_SELECTION, this);
      }
    }

    // Fire an event if this container object has scrolled.
    int sx = 0;
    int sy = 0;
    if (GetIntAttribute(ui::AX_ATTR_SCROLL_X, &sx) &&
        GetIntAttribute(ui::AX_ATTR_SCROLL_Y, &sy)) {
      if (sx != previous_scroll_x_ || sy != previous_scroll_y_)
        manager->MaybeCallNotifyWinEvent(EVENT_SYSTEM_SCROLLINGEND, this);
      previous_scroll_x_ = sx;
      previous_scroll_y_ = sy;
    }

    // Changing a static text node can affect the IAccessibleText hypertext
    // of the parent node, so force an update on the parent.
    BrowserAccessibilityWin* parent = GetParent()->ToBrowserAccessibilityWin();
    if (parent &&
        GetRole() == ui::AX_ROLE_STATIC_TEXT &&
        name() != old_win_attributes_->name) {
      parent->UpdateStep1ComputeWinAttributes();
      parent->UpdateStep2ComputeHypertext();
      parent->UpdateStep3FireEvents(false);
    }

    // Fire hypertext-related events.
    int start, old_len, new_len;
    ComputeHypertextRemovedAndInserted(&start, &old_len, &new_len);
    if (old_len > 0) {
      // In-process screen readers may call IAccessibleText::get_oldText
      // in reaction to this event to retrieve the text that was removed.
      manager->MaybeCallNotifyWinEvent(IA2_EVENT_TEXT_REMOVED, this);
    }
    if (new_len > 0) {
      // In-process screen readers may call IAccessibleText::get_newText
      // in reaction to this event to retrieve the text that was inserted.
      manager->MaybeCallNotifyWinEvent(IA2_EVENT_TEXT_INSERTED, this);
    }
  }

  old_win_attributes_.reset(nullptr);
}

void BrowserAccessibilityWin::OnSubtreeWillBeDeleted() {
  manager()->ToBrowserAccessibilityManagerWin()->MaybeCallNotifyWinEvent(
      EVENT_OBJECT_HIDE, this);
}

void BrowserAccessibilityWin::NativeAddReference() {
  AddRef();
}

void BrowserAccessibilityWin::NativeReleaseReference() {
  Release();
}

bool BrowserAccessibilityWin::IsNative() const {
  return true;
}

void BrowserAccessibilityWin::OnLocationChanged() {
  manager()->ToBrowserAccessibilityManagerWin()->MaybeCallNotifyWinEvent(
      EVENT_OBJECT_LOCATIONCHANGE, this);
}

BrowserAccessibilityWin* BrowserAccessibilityWin::NewReference() {
  AddRef();
  return this;
}

BrowserAccessibilityWin* BrowserAccessibilityWin::GetTargetFromChildID(
    const VARIANT& var_id) {
  if (var_id.vt != VT_I4)
    return NULL;

  LONG child_id = var_id.lVal;
  if (child_id == CHILDID_SELF)
    return this;

  if (child_id >= 1 && child_id <= static_cast<LONG>(PlatformChildCount()))
    return PlatformGetChild(child_id - 1)->ToBrowserAccessibilityWin();

  return manager()->ToBrowserAccessibilityManagerWin()->
      GetFromUniqueIdWin(child_id);
}

HRESULT BrowserAccessibilityWin::GetStringAttributeAsBstr(
    ui::AXStringAttribute attribute,
    BSTR* value_bstr) {
  base::string16 str;

  if (!GetString16Attribute(attribute, &str))
    return S_FALSE;

  if (str.empty())
    return S_FALSE;

  *value_bstr = SysAllocString(str.c_str());
  DCHECK(*value_bstr);

  return S_OK;
}

void BrowserAccessibilityWin::StringAttributeToIA2(
    ui::AXStringAttribute attribute,
    const char* ia2_attr) {
  base::string16 value;
  if (GetString16Attribute(attribute, &value)) {
    win_attributes_->ia2_attributes.push_back(
        base::ASCIIToUTF16(ia2_attr) + L":" + value);
  }
}

void BrowserAccessibilityWin::BoolAttributeToIA2(
    ui::AXBoolAttribute attribute,
    const char* ia2_attr) {
  bool value;
  if (GetBoolAttribute(attribute, &value)) {
    win_attributes_->ia2_attributes.push_back(
        (base::ASCIIToUTF16(ia2_attr) + L":") +
        (value ? L"true" : L"false"));
  }
}

void BrowserAccessibilityWin::IntAttributeToIA2(
    ui::AXIntAttribute attribute,
    const char* ia2_attr) {
  int value;
  if (GetIntAttribute(attribute, &value)) {
    win_attributes_->ia2_attributes.push_back(
        base::ASCIIToUTF16(ia2_attr) + L":" +
        base::IntToString16(value));
  }
}

base::string16 BrowserAccessibilityWin::GetNameRecursive() const {
  if (!name().empty()) {
    return name();
  }

  base::string16 result;
  for (uint32 i = 0; i < PlatformChildCount(); ++i) {
    result += PlatformGetChild(i)->ToBrowserAccessibilityWin()->
        GetNameRecursive();
  }
  return result;
}

base::string16 BrowserAccessibilityWin::GetValueText() {
  float fval;
  base::string16 value = this->value();

  if (value.empty() &&
      GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE, &fval)) {
    value = base::UTF8ToUTF16(base::DoubleToString(fval));
  }
  return value;
}

base::string16 BrowserAccessibilityWin::TextForIAccessibleText() {
  if (IsEditableText() || GetRole() == ui::AX_ROLE_MENU_LIST_OPTION)
    return value();
  return (GetRole() == ui::AX_ROLE_STATIC_TEXT) ? name() : hypertext();
}

bool BrowserAccessibilityWin::IsSameHypertextCharacter(size_t old_char_index,
                                                       size_t new_char_index) {
  CHECK(old_win_attributes_);

  // For anything other than the "embedded character", we just compare the
  // characters directly.
  base::char16 old_ch = old_win_attributes_->hypertext[old_char_index];
  base::char16 new_ch = win_attributes_->hypertext[new_char_index];
  if (old_ch != new_ch)
    return false;
  if (old_ch == new_ch && new_ch != kEmbeddedCharacter)
    return true;

  // If it's an embedded character, they're only identical if the child id
  // the hyperlink points to is the same.
  std::map<int32, int32>& old_offset_to_index =
      old_win_attributes_->hyperlink_offset_to_index;
  std::vector<int32>& old_hyperlinks = old_win_attributes_->hyperlinks;
  int32 old_hyperlinks_count = static_cast<int32>(old_hyperlinks.size());
  std::map<int32, int32>::iterator iter;
  iter = old_offset_to_index.find(old_char_index);
  int old_index = (iter != old_offset_to_index.end()) ? iter->second : -1;
  int old_child_id = (old_index >= 0 && old_index < old_hyperlinks_count) ?
      old_hyperlinks[old_index] : -1;

  std::map<int32, int32>& new_offset_to_index =
      win_attributes_->hyperlink_offset_to_index;
  std::vector<int32>& new_hyperlinks = win_attributes_->hyperlinks;
  int32 new_hyperlinks_count = static_cast<int32>(new_hyperlinks.size());
  iter = new_offset_to_index.find(new_char_index);
  int new_index = (iter != new_offset_to_index.end()) ? iter->second : -1;
  int new_child_id = (new_index >= 0 && new_index < new_hyperlinks_count) ?
      new_hyperlinks[new_index] : -1;

  return old_child_id == new_child_id;
}

void BrowserAccessibilityWin::ComputeHypertextRemovedAndInserted(
    int* start, int* old_len, int* new_len) {
  CHECK(old_win_attributes_);

  *start = 0;
  *old_len = 0;
  *new_len = 0;

  const base::string16& old_text = old_win_attributes_->hypertext;
  const base::string16& new_text = hypertext();

  size_t common_prefix = 0;
  while (common_prefix < old_text.size() &&
         common_prefix < new_text.size() &&
         IsSameHypertextCharacter(common_prefix, common_prefix)) {
    ++common_prefix;
  }

  size_t common_suffix = 0;
  while (common_prefix + common_suffix < old_text.size() &&
         common_prefix + common_suffix < new_text.size() &&
         IsSameHypertextCharacter(
             old_text.size() - common_suffix - 1,
             new_text.size() - common_suffix - 1)) {
    ++common_suffix;
  }

  *start = common_prefix;
  *old_len = old_text.size() - common_prefix - common_suffix;
  *new_len = new_text.size() - common_prefix - common_suffix;
}

void BrowserAccessibilityWin::HandleSpecialTextOffset(
    const base::string16& text,
    LONG* offset) {
  if (*offset == IA2_TEXT_OFFSET_LENGTH)
    *offset = static_cast<LONG>(text.size());
  else if (*offset == IA2_TEXT_OFFSET_CARET)
    get_caretOffset(offset);
}

ui::TextBoundaryType BrowserAccessibilityWin::IA2TextBoundaryToTextBoundary(
    IA2TextBoundaryType ia2_boundary) {
  switch(ia2_boundary) {
    case IA2_TEXT_BOUNDARY_CHAR:
      return ui::CHAR_BOUNDARY;
    case IA2_TEXT_BOUNDARY_WORD:
      return ui::WORD_BOUNDARY;
    case IA2_TEXT_BOUNDARY_LINE:
      return ui::LINE_BOUNDARY;
    case IA2_TEXT_BOUNDARY_SENTENCE:
      return ui::SENTENCE_BOUNDARY;
    case IA2_TEXT_BOUNDARY_PARAGRAPH:
      return ui::PARAGRAPH_BOUNDARY;
    case IA2_TEXT_BOUNDARY_ALL:
      return ui::ALL_BOUNDARY;
    default:
      NOTREACHED();
  }
  return ui::CHAR_BOUNDARY;
}

LONG BrowserAccessibilityWin::FindBoundary(
    const base::string16& text,
    IA2TextBoundaryType ia2_boundary,
    LONG start_offset,
    ui::TextBoundaryDirection direction) {
  HandleSpecialTextOffset(text, &start_offset);
  if (ia2_boundary == IA2_TEXT_BOUNDARY_WORD &&
      GetRole() == ui::AX_ROLE_TEXT_FIELD) {
    return GetWordStartBoundary(static_cast<int>(start_offset), direction);
  }

  ui::TextBoundaryType boundary = IA2TextBoundaryToTextBoundary(ia2_boundary);
  const std::vector<int32>& line_breaks = GetIntListAttribute(
      ui::AX_ATTR_LINE_BREAKS);
  return ui::FindAccessibleTextBoundary(
      text, line_breaks, boundary, start_offset, direction);
}

BrowserAccessibilityWin* BrowserAccessibilityWin::GetFromID(int32 id) {
  return manager()->GetFromID(id)->ToBrowserAccessibilityWin();
}

bool BrowserAccessibilityWin::IsListBoxOptionOrMenuListOption() {
  if (!GetParent())
    return false;

  int32 role = GetRole();
  int32 parent_role = GetParent()->GetRole();

  if (role == ui::AX_ROLE_LIST_BOX_OPTION &&
      parent_role == ui::AX_ROLE_LIST_BOX) {
    return true;
  }

  if (role == ui::AX_ROLE_MENU_LIST_OPTION &&
      parent_role == ui::AX_ROLE_MENU_LIST_POPUP) {
    return true;
  }

  return false;
}

void BrowserAccessibilityWin::InitRoleAndState() {
  int32 ia_role = 0;
  int32 ia_state = 0;
  base::string16 role_name;
  int32 ia2_role = 0;
  int32 ia2_state = IA2_STATE_OPAQUE;

  if (HasState(ui::AX_STATE_BUSY))
    ia_state |= STATE_SYSTEM_BUSY;
  if (HasState(ui::AX_STATE_CHECKED))
    ia_state |= STATE_SYSTEM_CHECKED;
  if (HasState(ui::AX_STATE_COLLAPSED))
    ia_state |= STATE_SYSTEM_COLLAPSED;
  if (HasState(ui::AX_STATE_EXPANDED))
    ia_state |= STATE_SYSTEM_EXPANDED;
  if (HasState(ui::AX_STATE_FOCUSABLE))
    ia_state |= STATE_SYSTEM_FOCUSABLE;
  if (HasState(ui::AX_STATE_HASPOPUP))
    ia_state |= STATE_SYSTEM_HASPOPUP;
  if (HasState(ui::AX_STATE_INDETERMINATE))
    ia_state |= STATE_SYSTEM_INDETERMINATE;
  if (HasIntAttribute(ui::AX_ATTR_INVALID_STATE) &&
      GetIntAttribute(ui::AX_ATTR_INVALID_STATE) != ui::AX_INVALID_STATE_FALSE)
    ia2_state |= IA2_STATE_INVALID_ENTRY;
  if (HasState(ui::AX_STATE_INVISIBLE))
    ia_state |= STATE_SYSTEM_INVISIBLE;
  if (HasState(ui::AX_STATE_LINKED))
    ia_state |= STATE_SYSTEM_LINKED;
  if (HasState(ui::AX_STATE_MULTISELECTABLE)) {
    ia_state |= STATE_SYSTEM_EXTSELECTABLE;
    ia_state |= STATE_SYSTEM_MULTISELECTABLE;
  }
  // TODO(ctguil): Support STATE_SYSTEM_EXTSELECTABLE/accSelect.
  if (HasState(ui::AX_STATE_OFFSCREEN))
    ia_state |= STATE_SYSTEM_OFFSCREEN;
  if (HasState(ui::AX_STATE_PRESSED))
    ia_state |= STATE_SYSTEM_PRESSED;
  if (HasState(ui::AX_STATE_PROTECTED))
    ia_state |= STATE_SYSTEM_PROTECTED;
  if (HasState(ui::AX_STATE_REQUIRED))
    ia2_state |= IA2_STATE_REQUIRED;
  if (HasState(ui::AX_STATE_SELECTABLE))
    ia_state |= STATE_SYSTEM_SELECTABLE;
  if (HasState(ui::AX_STATE_SELECTED))
    ia_state |= STATE_SYSTEM_SELECTED;
  if (HasState(ui::AX_STATE_VISITED))
    ia_state |= STATE_SYSTEM_TRAVERSED;
  if (!HasState(ui::AX_STATE_ENABLED))
    ia_state |= STATE_SYSTEM_UNAVAILABLE;
  if (HasState(ui::AX_STATE_VERTICAL))
    ia2_state |= IA2_STATE_VERTICAL;
  if (HasState(ui::AX_STATE_HORIZONTAL))
    ia2_state |= IA2_STATE_HORIZONTAL;
  if (HasState(ui::AX_STATE_VISITED))
    ia_state |= STATE_SYSTEM_TRAVERSED;

  // Expose whether or not the mouse is over an element, but suppress
  // this for tests because it can make the test results flaky depending
  // on the position of the mouse.
  BrowserAccessibilityStateImpl* accessibility_state =
      BrowserAccessibilityStateImpl::GetInstance();
  if (!accessibility_state->disable_hot_tracking_for_testing()) {
    if (HasState(ui::AX_STATE_HOVERED))
      ia_state |= STATE_SYSTEM_HOTTRACKED;
  }

  // WebKit marks everything as readonly unless it's editable text, so if it's
  // not readonly, mark it as editable now. The final computation of the
  // READONLY state for MSAA is below, after the switch.
  if (!HasState(ui::AX_STATE_READ_ONLY))
    ia2_state |= IA2_STATE_EDITABLE;

  if (GetBoolAttribute(ui::AX_ATTR_BUTTON_MIXED))
    ia_state |= STATE_SYSTEM_MIXED;

  if (GetBoolAttribute(ui::AX_ATTR_CAN_SET_VALUE))
    ia2_state |= IA2_STATE_EDITABLE;

  if (!GetStringAttribute(ui::AX_ATTR_AUTO_COMPLETE).empty())
    ia2_state |= IA2_STATE_SUPPORTS_AUTOCOMPLETION;

  base::string16 html_tag = GetString16Attribute(
      ui::AX_ATTR_HTML_TAG);
  switch (GetRole()) {
    case ui::AX_ROLE_ALERT:
      ia_role = ROLE_SYSTEM_ALERT;
      break;
    case ui::AX_ROLE_ALERT_DIALOG:
      ia_role = ROLE_SYSTEM_DIALOG;
      break;
    case ui::AX_ROLE_APPLICATION:
      ia_role = ROLE_SYSTEM_APPLICATION;
      break;
    case ui::AX_ROLE_ARTICLE:
      ia_role = ROLE_SYSTEM_DOCUMENT;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_BANNER:
      ia_role = ROLE_SYSTEM_GROUPING;
      ia2_role = IA2_ROLE_HEADER;
      break;
    case ui::AX_ROLE_BLOCKQUOTE:
      role_name = html_tag;
      ia2_role = IA2_ROLE_SECTION;
      break;
    case ui::AX_ROLE_BUSY_INDICATOR:
      ia_role = ROLE_SYSTEM_ANIMATION;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_BUTTON:
      ia_role = ROLE_SYSTEM_PUSHBUTTON;
      break;
    case ui::AX_ROLE_CANVAS:
      if (GetBoolAttribute(ui::AX_ATTR_CANVAS_HAS_FALLBACK)) {
        role_name = L"canvas";
        ia2_role = IA2_ROLE_CANVAS;
      } else {
        ia_role = ROLE_SYSTEM_GRAPHIC;
      }
      break;
    case ui::AX_ROLE_CAPTION:
      ia_role = ROLE_SYSTEM_TEXT;
      ia2_role = IA2_ROLE_CAPTION;
      break;
    case ui::AX_ROLE_CELL:
      ia_role = ROLE_SYSTEM_CELL;
      break;
    case ui::AX_ROLE_CHECK_BOX:
      ia_role = ROLE_SYSTEM_CHECKBUTTON;
      ia2_state |= IA2_STATE_CHECKABLE;
      break;
    case ui::AX_ROLE_COLOR_WELL:
      ia_role = ROLE_SYSTEM_TEXT;
      ia2_role = IA2_ROLE_COLOR_CHOOSER;
      break;
    case ui::AX_ROLE_COLUMN:
      ia_role = ROLE_SYSTEM_COLUMN;
      break;
    case ui::AX_ROLE_COLUMN_HEADER:
      ia_role = ROLE_SYSTEM_COLUMNHEADER;
      break;
    case ui::AX_ROLE_COMBO_BOX:
      ia_role = ROLE_SYSTEM_COMBOBOX;
      break;
    case ui::AX_ROLE_COMPLEMENTARY:
      ia_role = ROLE_SYSTEM_GROUPING;
      ia2_role = IA2_ROLE_NOTE;
      break;
    case ui::AX_ROLE_CONTENT_INFO:
      ia_role = ROLE_SYSTEM_TEXT;
      ia2_role = IA2_ROLE_PARAGRAPH;
      break;
    case ui::AX_ROLE_DATE:
    case ui::AX_ROLE_DATE_TIME:
      ia_role = ROLE_SYSTEM_DROPLIST;
      ia2_role = IA2_ROLE_DATE_EDITOR;
      break;
    case ui::AX_ROLE_DIV:
      role_name = L"div";
      ia_role = ROLE_SYSTEM_GROUPING;
      ia2_role = IA2_ROLE_SECTION;
      break;
    case ui::AX_ROLE_DEFINITION:
      role_name = html_tag;
      ia2_role = IA2_ROLE_PARAGRAPH;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_DESCRIPTION_LIST_DETAIL:
      role_name = html_tag;
      ia_role = ROLE_SYSTEM_TEXT;
      ia2_role = IA2_ROLE_PARAGRAPH;
      break;
    case ui::AX_ROLE_DESCRIPTION_LIST:
      role_name = html_tag;
      ia_role = ROLE_SYSTEM_LIST;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_DESCRIPTION_LIST_TERM:
      ia_role = ROLE_SYSTEM_LISTITEM;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_DETAILS:
      role_name = html_tag;
      ia_role = ROLE_SYSTEM_GROUPING;
      break;
    case ui::AX_ROLE_DIALOG:
      ia_role = ROLE_SYSTEM_DIALOG;
      break;
    case ui::AX_ROLE_DISCLOSURE_TRIANGLE:
      ia_role = ROLE_SYSTEM_PUSHBUTTON;
      break;
    case ui::AX_ROLE_DOCUMENT:
    case ui::AX_ROLE_ROOT_WEB_AREA:
    case ui::AX_ROLE_WEB_AREA:
      ia_role = ROLE_SYSTEM_DOCUMENT;
      ia_state |= STATE_SYSTEM_READONLY;
      ia_state |= STATE_SYSTEM_FOCUSABLE;
      break;
    case ui::AX_ROLE_EMBEDDED_OBJECT:
      ia_role = ROLE_SYSTEM_CLIENT;
      ia2_role = IA2_ROLE_EMBEDDED_OBJECT;
      break;
    case ui::AX_ROLE_FIGCAPTION:
      role_name = html_tag;
      ia2_role = IA2_ROLE_CAPTION;
      break;
    case ui::AX_ROLE_FIGURE:
      ia_role = ROLE_SYSTEM_GROUPING;
      break;
    case ui::AX_ROLE_FORM:
      role_name = L"form";
      ia2_role = IA2_ROLE_FORM;
      break;
    case ui::AX_ROLE_FOOTER:
      ia_role = ROLE_SYSTEM_GROUPING;
      ia2_role = IA2_ROLE_FOOTER;
      break;
    case ui::AX_ROLE_GRID:
      ia_role = ROLE_SYSTEM_TABLE;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_GROUP: {
      base::string16 aria_role = GetString16Attribute(
          ui::AX_ATTR_ROLE);
      if (aria_role == L"group" || html_tag == L"fieldset") {
        ia_role = ROLE_SYSTEM_GROUPING;
      } else if (html_tag == L"li") {
        ia_role = ROLE_SYSTEM_LISTITEM;
        ia_state |= STATE_SYSTEM_READONLY;
      } else {
        if (html_tag.empty())
          role_name = L"div";
        else
          role_name = html_tag;
        ia2_role = IA2_ROLE_SECTION;
      }
      break;
    }
    case ui::AX_ROLE_HEADING:
      role_name = html_tag;
      ia2_role = IA2_ROLE_HEADING;
      break;
    case ui::AX_ROLE_IFRAME:
      ia_role = ROLE_SYSTEM_DOCUMENT;
      ia2_role = IA2_ROLE_INTERNAL_FRAME;
      ia_state = STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_IFRAME_PRESENTATIONAL:
      ia_role = ROLE_SYSTEM_GROUPING;
      break;
    case ui::AX_ROLE_IMAGE:
      ia_role = ROLE_SYSTEM_GRAPHIC;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_IMAGE_MAP:
      role_name = html_tag;
      ia2_role = IA2_ROLE_IMAGE_MAP;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_IMAGE_MAP_LINK:
      ia_role = ROLE_SYSTEM_LINK;
      ia_state |= STATE_SYSTEM_LINKED;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_INPUT_TIME:
      ia_role = ROLE_SYSTEM_GROUPING;
      break;
    case ui::AX_ROLE_LABEL_TEXT:
    case ui::AX_ROLE_LEGEND:
      ia_role = ROLE_SYSTEM_TEXT;
      ia2_role = IA2_ROLE_LABEL;
      break;
    case ui::AX_ROLE_LINK:
      ia_role = ROLE_SYSTEM_LINK;
      ia_state |= STATE_SYSTEM_LINKED;
      break;
    case ui::AX_ROLE_LIST:
      ia_role = ROLE_SYSTEM_LIST;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_LIST_BOX:
      ia_role = ROLE_SYSTEM_LIST;
      break;
    case ui::AX_ROLE_LIST_BOX_OPTION:
      ia_role = ROLE_SYSTEM_LISTITEM;
      if (ia_state & STATE_SYSTEM_SELECTABLE) {
        ia_state |= STATE_SYSTEM_FOCUSABLE;
        if (HasState(ui::AX_STATE_FOCUSED))
          ia_state |= STATE_SYSTEM_FOCUSED;
      }
      break;
    case ui::AX_ROLE_LIST_ITEM:
      ia_role = ROLE_SYSTEM_LISTITEM;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_MAIN:
      ia_role = ROLE_SYSTEM_GROUPING;
      ia2_role = IA2_ROLE_PARAGRAPH;
      break;
    case ui::AX_ROLE_MARK:
      ia_role = ROLE_SYSTEM_TEXT;
      ia2_role = IA2_ROLE_TEXT_FRAME;
      break;
    case ui::AX_ROLE_MARQUEE:
      ia_role = ROLE_SYSTEM_ANIMATION;
      break;
    case ui::AX_ROLE_MATH:
      ia_role = ROLE_SYSTEM_EQUATION;
      break;
    case ui::AX_ROLE_MENU:
    case ui::AX_ROLE_MENU_BUTTON:
      ia_role = ROLE_SYSTEM_MENUPOPUP;
      break;
    case ui::AX_ROLE_MENU_BAR:
      ia_role = ROLE_SYSTEM_MENUBAR;
      break;
    case ui::AX_ROLE_MENU_ITEM:
      ia_role = ROLE_SYSTEM_MENUITEM;
      break;
    case ui::AX_ROLE_MENU_ITEM_CHECK_BOX:
      ia_role = ROLE_SYSTEM_MENUITEM;
      ia2_role = IA2_ROLE_CHECK_MENU_ITEM;
      ia2_state |= IA2_STATE_CHECKABLE;
      break;
    case ui::AX_ROLE_MENU_ITEM_RADIO:
      ia_role = ROLE_SYSTEM_MENUITEM;
      ia2_role = IA2_ROLE_RADIO_MENU_ITEM;
      break;
    case ui::AX_ROLE_MENU_LIST_POPUP:
      ia_role = ROLE_SYSTEM_LIST;
      ia2_state &= ~(IA2_STATE_EDITABLE);
      break;
    case ui::AX_ROLE_MENU_LIST_OPTION:
      ia_role = ROLE_SYSTEM_LISTITEM;
      ia2_state &= ~(IA2_STATE_EDITABLE);
      if (ia_state & STATE_SYSTEM_SELECTABLE) {
        ia_state |= STATE_SYSTEM_FOCUSABLE;
        if (HasState(ui::AX_STATE_FOCUSED))
          ia_state |= STATE_SYSTEM_FOCUSED;
      }
      break;
    case ui::AX_ROLE_METER:
      role_name = html_tag;
      ia_role = ROLE_SYSTEM_PROGRESSBAR;
      break;
    case ui::AX_ROLE_NAVIGATION:
      ia_role = ROLE_SYSTEM_GROUPING;
      ia2_role = IA2_ROLE_SECTION;
      break;
    case ui::AX_ROLE_NOTE:
      ia_role = ROLE_SYSTEM_GROUPING;
      ia2_role = IA2_ROLE_NOTE;
      break;
    case ui::AX_ROLE_OUTLINE:
      ia_role = ROLE_SYSTEM_OUTLINE;
      break;
    case ui::AX_ROLE_PARAGRAPH:
      role_name = L"P";
      ia2_role = IA2_ROLE_PARAGRAPH;
      break;
    case ui::AX_ROLE_POP_UP_BUTTON:
      if (html_tag == L"select") {
        ia_role = ROLE_SYSTEM_COMBOBOX;
      } else {
        ia_role = ROLE_SYSTEM_BUTTONMENU;
      }
      break;
    case ui::AX_ROLE_PRE:
      role_name = html_tag;
      ia_role = ROLE_SYSTEM_TEXT;
      ia2_role = IA2_ROLE_PARAGRAPH;
      break;
    case ui::AX_ROLE_PROGRESS_INDICATOR:
      ia_role = ROLE_SYSTEM_PROGRESSBAR;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_RADIO_BUTTON:
      ia_role = ROLE_SYSTEM_RADIOBUTTON;
      ia2_state = IA2_STATE_CHECKABLE;
      break;
    case ui::AX_ROLE_RADIO_GROUP:
      ia_role = ROLE_SYSTEM_GROUPING;
      break;
    case ui::AX_ROLE_REGION:
      if (html_tag == L"section") {
        ia_role = ROLE_SYSTEM_GROUPING;
        ia2_role = IA2_ROLE_SECTION;
      } else {
        ia_role = ROLE_SYSTEM_PANE;
      }
      break;
    case ui::AX_ROLE_ROW:
      ia_role = ROLE_SYSTEM_ROW;
      break;
    case ui::AX_ROLE_ROW_HEADER:
      ia_role = ROLE_SYSTEM_ROWHEADER;
      break;
    case ui::AX_ROLE_RUBY:
      ia_role = ROLE_SYSTEM_TEXT;
      ia2_role = IA2_ROLE_TEXT_FRAME;
      break;
    case ui::AX_ROLE_RULER:
      ia_role = ROLE_SYSTEM_CLIENT;
      ia2_role = IA2_ROLE_RULER;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_SCROLL_AREA:
      ia_role = ROLE_SYSTEM_CLIENT;
      ia2_role = IA2_ROLE_SCROLL_PANE;
      ia_state |= STATE_SYSTEM_READONLY;
      ia2_state &= ~(IA2_STATE_EDITABLE);
      break;
    case ui::AX_ROLE_SCROLL_BAR:
      ia_role = ROLE_SYSTEM_SCROLLBAR;
      break;
    case ui::AX_ROLE_SEARCH:
      ia_role = ROLE_SYSTEM_GROUPING;
      ia2_role = IA2_ROLE_SECTION;
      break;
    case ui::AX_ROLE_SLIDER:
      ia_role = ROLE_SYSTEM_SLIDER;
      break;
    case ui::AX_ROLE_SPIN_BUTTON:
      ia_role = ROLE_SYSTEM_SPINBUTTON;
      break;
    case ui::AX_ROLE_SPIN_BUTTON_PART:
      ia_role = ROLE_SYSTEM_PUSHBUTTON;
      break;
    case ui::AX_ROLE_ANNOTATION:
    case ui::AX_ROLE_LIST_MARKER:
    case ui::AX_ROLE_STATIC_TEXT:
      ia_role = ROLE_SYSTEM_STATICTEXT;
      break;
    case ui::AX_ROLE_STATUS:
      ia_role = ROLE_SYSTEM_STATUSBAR;
      break;
    case ui::AX_ROLE_SPLITTER:
      ia_role = ROLE_SYSTEM_SEPARATOR;
      break;
    case ui::AX_ROLE_SVG_ROOT:
      ia_role = ROLE_SYSTEM_GRAPHIC;
      break;
    case ui::AX_ROLE_SWITCH:
      role_name = L"switch";
      ia2_role = IA2_ROLE_TOGGLE_BUTTON;
      break;
    case ui::AX_ROLE_TAB:
      ia_role = ROLE_SYSTEM_PAGETAB;
      break;
    case ui::AX_ROLE_TABLE: {
      base::string16 aria_role = GetString16Attribute(
          ui::AX_ATTR_ROLE);
      if (aria_role == L"treegrid") {
        ia_role = ROLE_SYSTEM_OUTLINE;
      } else {
        ia_role = ROLE_SYSTEM_TABLE;
      }
      break;
    }
    case ui::AX_ROLE_TABLE_HEADER_CONTAINER:
      ia_role = ROLE_SYSTEM_GROUPING;
      ia2_role = IA2_ROLE_SECTION;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_TAB_LIST:
      ia_role = ROLE_SYSTEM_PAGETABLIST;
      break;
    case ui::AX_ROLE_TAB_PANEL:
      ia_role = ROLE_SYSTEM_PROPERTYPAGE;
      break;
    case ui::AX_ROLE_TOGGLE_BUTTON:
      ia_role = ROLE_SYSTEM_PUSHBUTTON;
      ia2_role = IA2_ROLE_TOGGLE_BUTTON;
      break;
    case ui::AX_ROLE_TEXT_FIELD:
    case ui::AX_ROLE_SEARCH_BOX:
      ia_role = ROLE_SYSTEM_TEXT;
      if (HasState(ui::AX_STATE_MULTILINE))
        ia2_state |= IA2_STATE_MULTI_LINE;
      else
        ia2_state |= IA2_STATE_SINGLE_LINE;
      ia2_state |= IA2_STATE_EDITABLE;
      ia2_state |= IA2_STATE_SELECTABLE_TEXT;
      break;
    case ui::AX_ROLE_TIME:
      role_name = html_tag;
      ia_role = ROLE_SYSTEM_TEXT;
      ia2_role = IA2_ROLE_TEXT_FRAME;
      break;
    case ui::AX_ROLE_TIMER:
      ia_role = ROLE_SYSTEM_CLOCK;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_TOOLBAR:
      ia_role = ROLE_SYSTEM_TOOLBAR;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_TOOLTIP:
      ia_role = ROLE_SYSTEM_TOOLTIP;
      ia_state |= STATE_SYSTEM_READONLY;
      break;
    case ui::AX_ROLE_TREE:
      ia_role = ROLE_SYSTEM_OUTLINE;
      break;
    case ui::AX_ROLE_TREE_GRID:
      ia_role = ROLE_SYSTEM_OUTLINE;
      break;
    case ui::AX_ROLE_TREE_ITEM:
      ia_role = ROLE_SYSTEM_OUTLINEITEM;
      break;
    case ui::AX_ROLE_LINE_BREAK:
      ia_role = ROLE_SYSTEM_WHITESPACE;
      break;
    case ui::AX_ROLE_WINDOW:
      ia_role = ROLE_SYSTEM_WINDOW;
      break;

    // TODO(dmazzoni): figure out the proper MSAA role for all of these.
    case ui::AX_ROLE_DIRECTORY:
    case ui::AX_ROLE_IGNORED:
    case ui::AX_ROLE_LOG:
    case ui::AX_ROLE_NONE:
    case ui::AX_ROLE_PRESENTATIONAL:
    case ui::AX_ROLE_SLIDER_THUMB:
    default:
      ia_role = ROLE_SYSTEM_CLIENT;
      break;
  }

  // Compute the final value of READONLY for MSAA.
  //
  // We always set the READONLY state for elements that have the
  // aria-readonly attribute and for a few roles (in the switch above).
  // We clear the READONLY state on focusable controls and on a document.
  // Everything else, the majority of objects, do not have this state set.
  if (HasState(ui::AX_STATE_FOCUSABLE) &&
      ia_role != ROLE_SYSTEM_DOCUMENT) {
    ia_state &= ~(STATE_SYSTEM_READONLY);
  }
  if (!HasState(ui::AX_STATE_READ_ONLY))
    ia_state &= ~(STATE_SYSTEM_READONLY);
  if (GetBoolAttribute(ui::AX_ATTR_ARIA_READONLY))
    ia_state |= STATE_SYSTEM_READONLY;

  // The role should always be set.
  DCHECK(!role_name.empty() || ia_role);

  // If we didn't explicitly set the IAccessible2 role, make it the same
  // as the MSAA role.
  if (!ia2_role)
    ia2_role = ia_role;

  win_attributes_->ia_role = ia_role;
  win_attributes_->ia_state = ia_state;
  win_attributes_->role_name = role_name;
  win_attributes_->ia2_role = ia2_role;
  win_attributes_->ia2_state = ia2_state;
}

}  // namespace content
