// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/message_center/views/message_list_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {
const int kAnimateClearingNextNotificationDelayMS = 40;
}  // namespace

MessageListView::MessageListView(MessageCenterView* message_center_view,
                                 bool top_down)
    : message_center_view_(message_center_view),
      reposition_top_(-1),
      fixed_height_(0),
      has_deferred_task_(false),
      clear_all_started_(false),
      top_down_(top_down),
      weak_ptr_factory_(this) {
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1);
  layout->SetDefaultFlex(1);
  SetLayoutManager(layout);

  // Set the margin to 0 for the layout. BoxLayout assumes the same margin
  // for top and bottom, but the bottom margin here should be smaller
  // because of the shadow of message view. Use an empty border instead
  // to provide this margin.
  gfx::Insets shadow_insets = MessageView::GetShadowInsets();
  set_background(
      views::Background::CreateSolidBackground(kMessageCenterBackgroundColor));
  SetBorder(views::Border::CreateEmptyBorder(
      top_down ? 0 : kMarginBetweenItems - shadow_insets.top(),    /* top */
      kMarginBetweenItems - shadow_insets.left(),                  /* left */
      top_down ? kMarginBetweenItems - shadow_insets.bottom() : 0, /* bottom */
      kMarginBetweenItems - shadow_insets.right() /* right */));
}

MessageListView::~MessageListView() {
  if (animator_.get())
    animator_->RemoveObserver(this);
}

void MessageListView::Layout() {
  if (animator_.get() && animator_->IsAnimating())
    return;

  gfx::Rect child_area = GetContentsBounds();
  int top = child_area.y();
  int between_items =
      kMarginBetweenItems - MessageView::GetShadowInsets().bottom();

  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;
    int height = child->GetHeightForWidth(child_area.width());
    child->SetBounds(child_area.x(), top, child_area.width(), height);
    top += height + between_items;
  }
}

void MessageListView::AddNotificationAt(MessageView* view, int index) {
  // |index| refers to a position in a subset of valid children. |real_index|
  // in a list includes the invalid children, so we compute the real index by
  // walking the list until |index| number of valid children are encountered,
  // or to the end of the list.
  int real_index = 0;
  while (real_index < child_count()) {
    if (IsValidChild(child_at(real_index))) {
      --index;
      if (index < 0)
        break;
    }
    ++real_index;
  }

  AddChildViewAt(view, real_index);
  if (GetContentsBounds().IsEmpty())
    return;

  adding_views_.insert(view);
  DoUpdateIfPossible();
}

void MessageListView::RemoveNotification(MessageView* view) {
  DCHECK_EQ(view->parent(), this);
  if (GetContentsBounds().IsEmpty()) {
    delete view;
  } else {
    if (view->layer()) {
      deleting_views_.insert(view);
    } else {
      if (animator_.get())
        animator_->StopAnimatingView(view);
      delete view;
    }
    DoUpdateIfPossible();
  }
}

void MessageListView::UpdateNotification(MessageView* view,
                                         const Notification& notification) {
  int index = GetIndexOf(view);
  DCHECK_LE(0, index);  // GetIndexOf is negative if not a child.

  if (animator_.get())
    animator_->StopAnimatingView(view);
  if (deleting_views_.find(view) != deleting_views_.end())
    deleting_views_.erase(view);
  if (deleted_when_done_.find(view) != deleted_when_done_.end())
    deleted_when_done_.erase(view);
  view->UpdateWithNotification(notification);
  DoUpdateIfPossible();
}

gfx::Size MessageListView::GetPreferredSize() const {
  int width = 0;
  for (int i = 0; i < child_count(); i++) {
    const views::View* child = child_at(i);
    if (IsValidChild(child))
      width = std::max(width, child->GetPreferredSize().width());
  }

  return gfx::Size(width + GetInsets().width(),
                   GetHeightForWidth(width + GetInsets().width()));
}

int MessageListView::GetHeightForWidth(int width) const {
  if (fixed_height_ > 0)
    return fixed_height_;

  width -= GetInsets().width();
  int height = 0;
  int padding = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(i);
    if (!IsValidChild(child))
      continue;
    height += child->GetHeightForWidth(width) + padding;
    padding = kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
  }

  return height + GetInsets().height();
}

void MessageListView::PaintChildren(const ui::PaintContext& context) {
  // Paint in the inversed order. Otherwise upper notification may be
  // hidden by the lower one.
  for (int i = child_count() - 1; i >= 0; --i) {
    if (!child_at(i)->layer())
      child_at(i)->Paint(context);
  }
}

void MessageListView::ReorderChildLayers(ui::Layer* parent_layer) {
  // Reorder children to stack the last child layer at the top. Otherwise
  // upper notification may be hidden by the lower one.
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->layer())
      parent_layer->StackAtBottom(child_at(i)->layer());
  }
}

void MessageListView::SetRepositionTarget(const gfx::Rect& target) {
  reposition_top_ = target.y();
  fixed_height_ = GetHeightForWidth(width());
}

void MessageListView::ResetRepositionSession() {
  // Don't call DoUpdateIfPossible(), but let Layout() do the task without
  // animation. Reset will cause the change of the bubble size itself, and
  // animation from the old location will look weird.
  if (reposition_top_ >= 0 && animator_.get()) {
    has_deferred_task_ = false;
    // cancel cause OnBoundsAnimatorDone which deletes |deleted_when_done_|.
    animator_->Cancel();
    STLDeleteContainerPointers(deleting_views_.begin(), deleting_views_.end());
    deleting_views_.clear();
    adding_views_.clear();
    animator_.reset();
  }

  reposition_top_ = -1;
  fixed_height_ = 0;
}

void MessageListView::ClearAllNotifications(
    const gfx::Rect& visible_scroll_rect) {
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;
    if (gfx::IntersectRects(child->bounds(), visible_scroll_rect).IsEmpty())
      continue;
    clearing_all_views_.push_back(child);
  }
  DoUpdateIfPossible();
}

void MessageListView::OnBoundsAnimatorProgressed(
    views::BoundsAnimator* animator) {
  DCHECK_EQ(animator_.get(), animator);
  for (std::set<views::View*>::iterator iter = deleted_when_done_.begin();
       iter != deleted_when_done_.end(); ++iter) {
    const gfx::SlideAnimation* animation = animator->GetAnimationForView(*iter);
    if (animation)
      (*iter)->layer()->SetOpacity(animation->CurrentValueBetween(1.0, 0.0));
  }
}

void MessageListView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  STLDeleteContainerPointers(deleted_when_done_.begin(),
                             deleted_when_done_.end());
  deleted_when_done_.clear();

  if (clear_all_started_) {
    clear_all_started_ = false;
    message_center_view()->OnAllNotificationsCleared();
  }

  if (has_deferred_task_) {
    has_deferred_task_ = false;
    DoUpdateIfPossible();
  }

  if (GetWidget())
    GetWidget()->SynthesizeMouseMoveEvent();
}

bool MessageListView::IsValidChild(const views::View* child) const {
  return child->visible() &&
         deleting_views_.find(const_cast<views::View*>(child)) ==
             deleting_views_.end() &&
         deleted_when_done_.find(const_cast<views::View*>(child)) ==
             deleted_when_done_.end();
}

void MessageListView::DoUpdateIfPossible() {
  gfx::Rect child_area = GetContentsBounds();
  if (child_area.IsEmpty())
    return;

  if (animator_.get() && animator_->IsAnimating()) {
    has_deferred_task_ = true;
    return;
  }

  if (!animator_.get()) {
    animator_.reset(new views::BoundsAnimator(this));
    animator_->AddObserver(this);
  }

  if (!clearing_all_views_.empty()) {
    AnimateClearingOneNotification();
    return;
  }

  if (top_down_ ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMessageCenterAlwaysScrollUpUponNotificationRemoval))
    AnimateNotificationsBelowTarget();
  else
    AnimateNotificationsAboveTarget();

  adding_views_.clear();
  deleting_views_.clear();
}

void MessageListView::AnimateNotificationsBelowTarget() {
  int last_index = -1;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!IsValidChild(child)) {
      AnimateChild(child, child->y(), child->height());
    } else if (reposition_top_ < 0 || child->y() > reposition_top_) {
      // Find first notification below target (or all notifications if no
      // target).
      last_index = i;
      break;
    }
  }
  if (last_index > 0) {
    int between_items =
        kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
    int top = (reposition_top_ > 0) ? reposition_top_ : GetInsets().top();

    for (int i = last_index; i < child_count(); ++i) {
      // Animate notifications below target upwards.
      views::View* child = child_at(i);
      if (AnimateChild(child, top, child->height()))
        top += child->height() + between_items;
    }
  }
}

void MessageListView::AnimateNotificationsAboveTarget() {
  int last_index = -1;
  for (int i = child_count() - 1; i >= 0; --i) {
    views::View* child = child_at(i);
    if (!IsValidChild(child)) {
      AnimateChild(child, child->y(), child->height());
    } else if (reposition_top_ < 0 || child->y() < reposition_top_) {
      // Find first notification above target (or all notifications if no
      // target).
      last_index = i;
      break;
    }
  }
  if (last_index >= 0) {
    int between_items =
        kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
    int bottom = (reposition_top_ > 0)
                     ? reposition_top_ + child_at(last_index)->height()
                     : GetHeightForWidth(width()) - GetInsets().bottom();
    for (int i = last_index; i >= 0; --i) {
      // Animate notifications above target downwards.
      views::View* child = child_at(i);
      if (AnimateChild(child, bottom - child->height(), child->height()))
        bottom -= child->height() + between_items;
    }
  }
}

bool MessageListView::AnimateChild(views::View* child, int top, int height) {
  gfx::Rect child_area = GetContentsBounds();
  if (adding_views_.find(child) != adding_views_.end()) {
    child->SetBounds(child_area.right(), top, child_area.width(), height);
    animator_->AnimateViewTo(
        child, gfx::Rect(child_area.x(), top, child_area.width(), height));
  } else if (deleting_views_.find(child) != deleting_views_.end()) {
    DCHECK(child->layer());
    // No moves, but animate to fade-out.
    animator_->AnimateViewTo(child, child->bounds());
    deleted_when_done_.insert(child);
    return false;
  } else {
    gfx::Rect target(child_area.x(), top, child_area.width(), height);
    if (child->bounds().origin() != target.origin())
      animator_->AnimateViewTo(child, target);
    else
      child->SetBoundsRect(target);
  }
  return true;
}

void MessageListView::AnimateClearingOneNotification() {
  DCHECK(!clearing_all_views_.empty());

  clear_all_started_ = true;

  views::View* child = clearing_all_views_.front();
  clearing_all_views_.pop_front();

  // Slide from left to right.
  gfx::Rect new_bounds = child->bounds();
  new_bounds.set_x(new_bounds.right() + kMarginBetweenItems);
  animator_->AnimateViewTo(child, new_bounds);

  // Schedule to start sliding out next notification after a short delay.
  if (!clearing_all_views_.empty()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&MessageListView::AnimateClearingOneNotification,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kAnimateClearingNextNotificationDelayMS));
  }
}

}  // namespace message_center
