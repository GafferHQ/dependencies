// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree.h"

#include <set>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/accessibility/ax_node.h"

namespace ui {

namespace {

std::string TreeToStringHelper(AXNode* node, int indent) {
  std::string result = std::string(2 * indent, ' ');
  result += node->data().ToString() + "\n";
  for (int i = 0; i < node->child_count(); ++i)
    result += TreeToStringHelper(node->ChildAtIndex(i), indent + 1);
  return result;
}

}  // namespace

// Intermediate state to keep track of during a tree update.
struct AXTreeUpdateState {
  AXTreeUpdateState() : new_root(nullptr) {}

  // During an update, this keeps track of all nodes that have been
  // implicitly referenced as part of this update, but haven't been
  // updated yet. It's an error if there are any pending nodes at the
  // end of Unserialize.
  std::set<AXNode*> pending_nodes;

  // Keeps track of new nodes created during this update.
  std::set<AXNode*> new_nodes;

  // The new root in this update, if any.
  AXNode* new_root;
};

AXTreeDelegate::AXTreeDelegate() {
}

AXTreeDelegate::~AXTreeDelegate() {
}

AXTree::AXTree()
    : delegate_(NULL), root_(NULL) {
  AXNodeData root;
  root.id = -1;
  root.role = AX_ROLE_ROOT_WEB_AREA;

  AXTreeUpdate initial_state;
  initial_state.nodes.push_back(root);
  CHECK(Unserialize(initial_state)) << error();
}

AXTree::AXTree(const AXTreeUpdate& initial_state)
    : delegate_(NULL), root_(NULL) {
  CHECK(Unserialize(initial_state)) << error();
}

AXTree::~AXTree() {
  if (root_)
    DestroyNodeAndSubtree(root_, nullptr);
}

void AXTree::SetDelegate(AXTreeDelegate* delegate) {
  delegate_ = delegate;
}

AXNode* AXTree::GetFromId(int32 id) const {
  base::hash_map<int32, AXNode*>::const_iterator iter = id_map_.find(id);
  return iter != id_map_.end() ? iter->second : NULL;
}

bool AXTree::Unserialize(const AXTreeUpdate& update) {
  AXTreeUpdateState update_state;
  int32 old_root_id = root_ ? root_->id() : 0;

  if (update.node_id_to_clear != 0) {
    AXNode* node = GetFromId(update.node_id_to_clear);
    if (!node) {
      error_ = base::StringPrintf("Bad node_id_to_clear: %d",
                                  update.node_id_to_clear);
      return false;
    }
    if (node == root_) {
      DestroySubtree(root_, &update_state);
      root_ = NULL;
    } else {
      for (int i = 0; i < node->child_count(); ++i)
        DestroySubtree(node->ChildAtIndex(i), &update_state);
      std::vector<AXNode*> children;
      node->SwapChildren(children);
      update_state.pending_nodes.insert(node);
    }
  }

  for (size_t i = 0; i < update.nodes.size(); ++i) {
    if (!UpdateNode(update.nodes[i], &update_state))
      return false;
  }

  if (!update_state.pending_nodes.empty()) {
    error_ = "Nodes left pending by the update:";
    for (std::set<AXNode*>::iterator iter = update_state.pending_nodes.begin();
         iter != update_state.pending_nodes.end(); ++iter) {
      error_ += base::StringPrintf(" %d", (*iter)->id());
    }
    return false;
  }

  if (delegate_) {
    std::set<AXNode*>& new_nodes = update_state.new_nodes;
    std::vector<AXTreeDelegate::Change> changes;
    changes.reserve(update.nodes.size());
    for (size_t i = 0; i < update.nodes.size(); ++i) {
      AXNode* node = GetFromId(update.nodes[i].id);
      if (new_nodes.find(node) != new_nodes.end()) {
        if (new_nodes.find(node->parent()) == new_nodes.end()) {
          changes.push_back(
              AXTreeDelegate::Change(node, AXTreeDelegate::SUBTREE_CREATED));
        } else {
          changes.push_back(
              AXTreeDelegate::Change(node, AXTreeDelegate::NODE_CREATED));
        }
      } else {
        changes.push_back(
            AXTreeDelegate::Change(node, AXTreeDelegate::NODE_CHANGED));
      }
    }
    delegate_->OnAtomicUpdateFinished(
        this, root_->id() != old_root_id, changes);
  }

  return true;
}

std::string AXTree::ToString() const {
  return TreeToStringHelper(root_, 0);
}

AXNode* AXTree::CreateNode(AXNode* parent, int32 id, int32 index_in_parent) {
  AXNode* new_node = new AXNode(parent, id, index_in_parent);
  id_map_[new_node->id()] = new_node;
  if (delegate_)
    delegate_->OnNodeCreated(this, new_node);
  return new_node;
}

bool AXTree::UpdateNode(const AXNodeData& src,
                        AXTreeUpdateState* update_state) {
  // This method updates one node in the tree based on serialized data
  // received in an AXTreeUpdate. See AXTreeUpdate for pre and post
  // conditions.

  // Look up the node by id. If it's not found, then either the root
  // of the tree is being swapped, or we're out of sync with the source
  // and this is a serious error.
  AXNode* node = GetFromId(src.id);
  if (node) {
    update_state->pending_nodes.erase(node);
    node->SetData(src);
  } else {
    if (src.role != AX_ROLE_ROOT_WEB_AREA &&
        src.role != AX_ROLE_DESKTOP) {
      error_ = base::StringPrintf(
          "%d is not in the tree and not the new root", src.id);
      return false;
    }
    if (update_state->new_root) {
      error_ = "Tree update contains two new roots";
      return false;
    }

    update_state->new_root = CreateNode(NULL, src.id, 0);
    node = update_state->new_root;
    update_state->new_nodes.insert(node);
    node->SetData(src);
  }

  if (delegate_)
    delegate_->OnNodeChanged(this, node);

  // First, delete nodes that used to be children of this node but aren't
  // anymore.
  if (!DeleteOldChildren(node, src.child_ids, update_state)) {
    if (update_state->new_root)
      DestroySubtree(update_state->new_root, update_state);
    return false;
  }

  // Now build a new children vector, reusing nodes when possible,
  // and swap it in.
  std::vector<AXNode*> new_children;
  bool success = CreateNewChildVector(
      node, src.child_ids, &new_children, update_state);
  node->SwapChildren(new_children);

  // Update the root of the tree if needed.
  if ((src.role == AX_ROLE_ROOT_WEB_AREA || src.role == AX_ROLE_DESKTOP) &&
      (!root_ || root_->id() != src.id)) {
    if (root_)
      DestroySubtree(root_, update_state);
    root_ = node;
  }

  return success;
}

void AXTree::DestroySubtree(AXNode* node,
                            AXTreeUpdateState* update_state) {
  if (delegate_)
    delegate_->OnSubtreeWillBeDeleted(this, node);
  DestroyNodeAndSubtree(node, update_state);
}

void AXTree::DestroyNodeAndSubtree(AXNode* node,
                                   AXTreeUpdateState* update_state) {
  id_map_.erase(node->id());
  for (int i = 0; i < node->child_count(); ++i)
    DestroyNodeAndSubtree(node->ChildAtIndex(i), update_state);
  if (delegate_)
    delegate_->OnNodeWillBeDeleted(this, node);
  if (update_state) {
    update_state->pending_nodes.erase(node);
  }
  node->Destroy();
}

bool AXTree::DeleteOldChildren(AXNode* node,
                               const std::vector<int32>& new_child_ids,
                               AXTreeUpdateState* update_state) {
  // Create a set of child ids in |src| for fast lookup, and return false
  // if a duplicate is found;
  std::set<int32> new_child_id_set;
  for (size_t i = 0; i < new_child_ids.size(); ++i) {
    if (new_child_id_set.find(new_child_ids[i]) != new_child_id_set.end()) {
      error_ = base::StringPrintf("Node %d has duplicate child id %d",
                                  node->id(), new_child_ids[i]);
      return false;
    }
    new_child_id_set.insert(new_child_ids[i]);
  }

  // Delete the old children.
  const std::vector<AXNode*>& old_children = node->children();
  for (size_t i = 0; i < old_children.size(); ++i) {
    int old_id = old_children[i]->id();
    if (new_child_id_set.find(old_id) == new_child_id_set.end())
      DestroySubtree(old_children[i], update_state);
  }

  return true;
}

bool AXTree::CreateNewChildVector(AXNode* node,
                                  const std::vector<int32>& new_child_ids,
                                  std::vector<AXNode*>* new_children,
                                  AXTreeUpdateState* update_state) {
  bool success = true;
  for (size_t i = 0; i < new_child_ids.size(); ++i) {
    int32 child_id = new_child_ids[i];
    int32 index_in_parent = static_cast<int32>(i);
    AXNode* child = GetFromId(child_id);
    if (child) {
      if (child->parent() != node) {
        // This is a serious error - nodes should never be reparented.
        // If this case occurs, continue so this node isn't left in an
        // inconsistent state, but return failure at the end.
        error_ = base::StringPrintf(
            "Node %d reparented from %d to %d",
            child->id(),
            child->parent() ? child->parent()->id() : 0,
            node->id());
        success = false;
        continue;
      }
      child->SetIndexInParent(index_in_parent);
    } else {
      child = CreateNode(node, child_id, index_in_parent);
      update_state->pending_nodes.insert(child);
      update_state->new_nodes.insert(child);
    }
    new_children->push_back(child);
  }

  return success;
}

}  // namespace ui
