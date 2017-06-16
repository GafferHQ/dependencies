// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

// The framework for these tests is that each test sets up |treedata0_|
// and |treedata1_| and then calls GetTreeSerializer, which creates a
// serializer for a tree that's initially in state |treedata0_|, but then
// changes to state |treedata1_|. This allows each test to check the
// updates created by AXTreeSerializer or unit-test its private
// member functions.
class AXTreeSerializerTest : public testing::Test {
 public:
  AXTreeSerializerTest() {}
  ~AXTreeSerializerTest() override {}

 protected:
  void CreateTreeSerializer();

  AXTreeUpdate treedata0_;
  AXTreeUpdate treedata1_;
  scoped_ptr<AXSerializableTree> tree0_;
  scoped_ptr<AXSerializableTree> tree1_;
  scoped_ptr<AXTreeSource<const AXNode*> > tree0_source_;
  scoped_ptr<AXTreeSource<const AXNode*> > tree1_source_;
  scoped_ptr<AXTreeSerializer<const AXNode*> > serializer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXTreeSerializerTest);
};

void AXTreeSerializerTest::CreateTreeSerializer() {
  if (serializer_)
    return;

  tree0_.reset(new AXSerializableTree(treedata0_));
  tree1_.reset(new AXSerializableTree(treedata1_));

  // Serialize tree0 so that AXTreeSerializer thinks that its client
  // is totally in sync.
  tree0_source_.reset(tree0_->CreateTreeSource());
  serializer_.reset(new AXTreeSerializer<const AXNode*>(tree0_source_.get()));
  AXTreeUpdate unused_update;
  serializer_->SerializeChanges(tree0_->root(), &unused_update);

  // Pretend that tree0_ turned into tree1_. The next call to
  // AXTreeSerializer will force it to consider these changes to
  // the tree and send them as part of the next update.
  tree1_source_.reset(tree1_->CreateTreeSource());
  serializer_->ChangeTreeSourceForTesting(tree1_source_.get());
}

// In this test, one child is added to the root. Only the root and
// new child should be added.
TEST_F(AXTreeSerializerTest, UpdateContainsOnlyChangedNodes) {
  // (1 (2 3))
  treedata0_.nodes.resize(3);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[0].child_ids.push_back(3);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[2].id = 3;

  // (1 (4 2 3))
  treedata1_.nodes.resize(4);
  treedata1_.nodes[0].id = 1;
  treedata1_.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  treedata1_.nodes[0].child_ids.push_back(4);
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[0].child_ids.push_back(3);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[3].id = 4;

  CreateTreeSerializer();
  AXTreeUpdate update;
  serializer_->SerializeChanges(tree1_->GetFromId(1), &update);

  // The update should only touch nodes 1 and 4 - nodes 2 and 3 are unchanged
  // and shouldn't be affected.
  EXPECT_EQ(0, update.node_id_to_clear);
  ASSERT_EQ(static_cast<size_t>(2), update.nodes.size());
  EXPECT_EQ(1, update.nodes[0].id);
  EXPECT_EQ(4, update.nodes[1].id);
}

// When the root changes, the whole tree is updated, even if some of it
// is unaffected.
TEST_F(AXTreeSerializerTest, NewRootUpdatesEntireTree) {
  // (1 (2 (3 (4))))
  treedata0_.nodes.resize(4);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[2].child_ids.push_back(4);
  treedata0_.nodes[3].id = 4;

  // (5 (2 (3 (4))))
  treedata1_.nodes.resize(4);
  treedata1_.nodes[0].id = 5;
  treedata1_.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[1].child_ids.push_back(3);
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[2].child_ids.push_back(4);
  treedata1_.nodes[3].id = 4;

  CreateTreeSerializer();
  AXTreeUpdate update;
  serializer_->SerializeChanges(tree1_->GetFromId(4), &update);

  // The update should delete the subtree rooted at node id=1, and
  // then include all four nodes in the update, even though the
  // subtree rooted at id=2 didn't actually change.
  EXPECT_EQ(1, update.node_id_to_clear);
  ASSERT_EQ(static_cast<size_t>(4), update.nodes.size());
  EXPECT_EQ(5, update.nodes[0].id);
  EXPECT_EQ(2, update.nodes[1].id);
  EXPECT_EQ(3, update.nodes[2].id);
  EXPECT_EQ(4, update.nodes[3].id);
}

// When a node is reparented, the subtree including both the old parent
// and new parent of the reparented node must be deleted and recreated.
TEST_F(AXTreeSerializerTest, ReparentingUpdatesSubtree) {
  // (1 (2 (3 (4) 5)))
  treedata0_.nodes.resize(5);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[1].child_ids.push_back(5);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[2].child_ids.push_back(4);
  treedata0_.nodes[3].id = 4;
  treedata0_.nodes[4].id = 5;

  // Node 5 has been reparented from being a child of node 2,
  // to a child of node 4.
  // (1 (2 (3 (4 (5)))))
  treedata1_.nodes.resize(5);
  treedata1_.nodes[0].id = 1;
  treedata1_.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  treedata1_.nodes[0].child_ids.push_back(2);
  treedata1_.nodes[1].id = 2;
  treedata1_.nodes[1].child_ids.push_back(3);
  treedata1_.nodes[2].id = 3;
  treedata1_.nodes[2].child_ids.push_back(4);
  treedata1_.nodes[3].id = 4;
  treedata1_.nodes[3].child_ids.push_back(5);
  treedata1_.nodes[4].id = 5;

  CreateTreeSerializer();
  AXTreeUpdate update;
  serializer_->SerializeChanges(tree1_->GetFromId(4), &update);

  // The update should delete the subtree rooted at node id=2, and
  // then include nodes 2...5.
  EXPECT_EQ(2, update.node_id_to_clear);
  ASSERT_EQ(static_cast<size_t>(4), update.nodes.size());
  EXPECT_EQ(2, update.nodes[0].id);
  EXPECT_EQ(3, update.nodes[1].id);
  EXPECT_EQ(4, update.nodes[2].id);
  EXPECT_EQ(5, update.nodes[3].id);
}

// A variant of AXTreeSource that returns true for IsValid() for one
// particular id.
class AXTreeSourceWithInvalidId : public AXTreeSource<const AXNode*> {
 public:
  AXTreeSourceWithInvalidId(AXTree* tree, int invalid_id)
      : tree_(tree),
        invalid_id_(invalid_id) {}
  ~AXTreeSourceWithInvalidId() override {}

  // AXTreeSource implementation.
  AXNode* GetRoot() const override { return tree_->root(); }
  AXNode* GetFromId(int32 id) const override { return tree_->GetFromId(id); }
  int32 GetId(const AXNode* node) const override { return node->id(); }
  void GetChildren(const AXNode* node,
                   std::vector<const AXNode*>* out_children) const override {
    for (int i = 0; i < node->child_count(); ++i)
      out_children->push_back(node->ChildAtIndex(i));
  }
  AXNode* GetParent(const AXNode* node) const override {
    return node->parent();
  }
  bool IsValid(const AXNode* node) const override {
    return node != NULL && node->id() != invalid_id_;
  }
  bool IsEqual(const AXNode* node1, const AXNode* node2) const override {
    return node1 == node2;
  }
  const AXNode* GetNull() const override { return NULL; }
  void SerializeNode(const AXNode* node, AXNodeData* out_data) const override {
    *out_data = node->data();
    if (node->id() == invalid_id_)
      out_data->id = -1;
  }

 private:
  AXTree* tree_;
  int invalid_id_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceWithInvalidId);
};

// Test that the serializer skips invalid children.
TEST(AXTreeSerializerInvalidTest, InvalidChild) {
  // (1 (2 3))
  AXTreeUpdate treedata;
  treedata.nodes.resize(3);
  treedata.nodes[0].id = 1;
  treedata.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  treedata.nodes[0].child_ids.push_back(2);
  treedata.nodes[0].child_ids.push_back(3);
  treedata.nodes[1].id = 2;
  treedata.nodes[2].id = 3;

  AXTree tree(treedata);
  AXTreeSourceWithInvalidId source(&tree, 3);

  AXTreeSerializer<const AXNode*> serializer(&source);
  AXTreeUpdate update;
  serializer.SerializeChanges(tree.root(), &update);

  ASSERT_EQ(2U, update.nodes.size());
  EXPECT_EQ(1, update.nodes[0].id);
  EXPECT_EQ(2, update.nodes[1].id);
}

// Test that we can set a maximum number of nodes to serialize.
TEST_F(AXTreeSerializerTest, MaximumSerializedNodeCount) {
  // (1 (2 (3 4) 5 (6 7)))
  treedata0_.nodes.resize(7);
  treedata0_.nodes[0].id = 1;
  treedata0_.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  treedata0_.nodes[0].child_ids.push_back(2);
  treedata0_.nodes[0].child_ids.push_back(5);
  treedata0_.nodes[1].id = 2;
  treedata0_.nodes[1].child_ids.push_back(3);
  treedata0_.nodes[1].child_ids.push_back(4);
  treedata0_.nodes[2].id = 3;
  treedata0_.nodes[3].id = 4;
  treedata0_.nodes[4].id = 5;
  treedata0_.nodes[4].child_ids.push_back(6);
  treedata0_.nodes[4].child_ids.push_back(7);
  treedata0_.nodes[5].id = 6;
  treedata0_.nodes[6].id = 7;

  tree0_.reset(new AXSerializableTree(treedata0_));
  tree0_source_.reset(tree0_->CreateTreeSource());
  serializer_.reset(new AXTreeSerializer<const AXNode*>(tree0_source_.get()));
  serializer_->set_max_node_count(4);
  AXTreeUpdate update;
  serializer_->SerializeChanges(tree0_->root(), &update);
  // It actually serializes 5 nodes, not 4 - to be consistent.
  // It skips the children of node 5.
  ASSERT_EQ(static_cast<size_t>(5), update.nodes.size());
}

}  // namespace ui
