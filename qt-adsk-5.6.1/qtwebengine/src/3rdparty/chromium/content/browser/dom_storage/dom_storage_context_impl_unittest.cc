// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/public/browser/local_storage_usage_info.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace content {

class DOMStorageContextImplTest : public testing::Test {
 public:
  DOMStorageContextImplTest()
    : kOrigin(GURL("http://dom_storage/")),
      kKey(ASCIIToUTF16("key")),
      kValue(ASCIIToUTF16("value")),
      kDontIncludeFileInfo(false),
      kDoIncludeFileInfo(true) {
  }

  const GURL kOrigin;
  const base::string16 kKey;
  const base::string16 kValue;
  const bool kDontIncludeFileInfo;
  const bool kDoIncludeFileInfo;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    storage_policy_ = new MockSpecialStoragePolicy;
    task_runner_ =
        new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get());
    context_ = new DOMStorageContextImpl(temp_dir_.path(),
                                         base::FilePath(),
                                         storage_policy_.get(),
                                         task_runner_.get());
  }

  void TearDown() override { base::MessageLoop::current()->RunUntilIdle(); }

  void VerifySingleOriginRemains(const GURL& origin) {
    // Use a new instance to examine the contexts of temp_dir_.
    scoped_refptr<DOMStorageContextImpl> context =
        new DOMStorageContextImpl(temp_dir_.path(), base::FilePath(),
                                  NULL, NULL);
    std::vector<LocalStorageUsageInfo> infos;
    context->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
    ASSERT_EQ(1u, infos.size());
    EXPECT_EQ(origin, infos[0].origin);
  }

  int session_id_offset() { return context_->session_id_offset_; }

 protected:
  base::MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<MockDOMStorageTaskRunner> task_runner_;
  scoped_refptr<DOMStorageContextImpl> context_;
  DISALLOW_COPY_AND_ASSIGN(DOMStorageContextImplTest);
};

TEST_F(DOMStorageContextImplTest, Basics) {
  // This test doesn't do much, checks that the constructor
  // initializes members properly and that invoking methods
  // on a newly created object w/o any data on disk do no harm.
  EXPECT_EQ(temp_dir_.path(), context_->localstorage_directory());
  EXPECT_EQ(base::FilePath(), context_->sessionstorage_directory());
  EXPECT_EQ(storage_policy_.get(), context_->special_storage_policy_.get());
  context_->DeleteLocalStorage(GURL("http://chromium.org/"));
  const int kFirstSessionStorageNamespaceId = 1 + session_id_offset();
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId));
  EXPECT_FALSE(context_->GetStorageNamespace(kFirstSessionStorageNamespaceId));
  EXPECT_EQ(kFirstSessionStorageNamespaceId, context_->AllocateSessionId());
  std::vector<LocalStorageUsageInfo> infos;
  context_->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
  EXPECT_TRUE(infos.empty());
  context_->Shutdown();
}

TEST_F(DOMStorageContextImplTest, UsageInfo) {
  // Should be empty initially
  std::vector<LocalStorageUsageInfo> infos;
  context_->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
  EXPECT_TRUE(infos.empty());
  context_->GetLocalStorageUsage(&infos, kDoIncludeFileInfo);
  EXPECT_TRUE(infos.empty());

  // Put some data into local storage and shutdown the context
  // to ensure data is written to disk.
  base::NullableString16 old_value;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)->
      OpenStorageArea(kOrigin)->SetItem(kKey, kValue, &old_value));
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();

  // Create a new context that points to the same directory, see that
  // it knows about the origin that we stored data for.
  context_ = new DOMStorageContextImpl(temp_dir_.path(), base::FilePath(),
                                       NULL, NULL);
  context_->GetLocalStorageUsage(&infos, kDontIncludeFileInfo);
  EXPECT_EQ(1u, infos.size());
  EXPECT_EQ(kOrigin, infos[0].origin);
  EXPECT_EQ(0u, infos[0].data_size);
  EXPECT_EQ(base::Time(), infos[0].last_modified);
  infos.clear();
  context_->GetLocalStorageUsage(&infos, kDoIncludeFileInfo);
  EXPECT_EQ(1u, infos.size());
  EXPECT_EQ(kOrigin, infos[0].origin);
  EXPECT_NE(0u, infos[0].data_size);
  EXPECT_NE(base::Time(), infos[0].last_modified);
}

TEST_F(DOMStorageContextImplTest, SessionOnly) {
  const GURL kSessionOnlyOrigin("http://www.sessiononly.com/");
  storage_policy_->AddSessionOnly(kSessionOnlyOrigin);

  // Store data for a normal and a session-only origin and then
  // invoke Shutdown() which should delete data for session-only
  // origins.
  base::NullableString16 old_value;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)->
      OpenStorageArea(kOrigin)->SetItem(kKey, kValue, &old_value));
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)->
      OpenStorageArea(kSessionOnlyOrigin)->SetItem(kKey, kValue, &old_value));
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();

  // Verify that the session-only origin data is gone.
  VerifySingleOriginRemains(kOrigin);
}

TEST_F(DOMStorageContextImplTest, SetForceKeepSessionState) {
  const GURL kSessionOnlyOrigin("http://www.sessiononly.com/");
  storage_policy_->AddSessionOnly(kSessionOnlyOrigin);

  // Store data for a session-only origin, setup to save session data, then
  // shutdown.
  base::NullableString16 old_value;
  EXPECT_TRUE(context_->GetStorageNamespace(kLocalStorageNamespaceId)->
      OpenStorageArea(kSessionOnlyOrigin)->SetItem(kKey, kValue, &old_value));
  context_->SetForceKeepSessionState();  // Should override clear behavior.
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();

  VerifySingleOriginRemains(kSessionOnlyOrigin);
}

TEST_F(DOMStorageContextImplTest, PersistentIds) {
  const int kFirstSessionStorageNamespaceId = 1 + session_id_offset();
  const std::string kPersistentId = "persistent";
  context_->CreateSessionNamespace(kFirstSessionStorageNamespaceId,
                                   kPersistentId);
  DOMStorageNamespace* dom_namespace =
      context_->GetStorageNamespace(kFirstSessionStorageNamespaceId);
  ASSERT_TRUE(dom_namespace);
  EXPECT_EQ(kPersistentId, dom_namespace->persistent_namespace_id());
  // Verify that the areas inherit the persistent ID.
  DOMStorageArea* area = dom_namespace->OpenStorageArea(kOrigin);
  EXPECT_EQ(kPersistentId, area->persistent_namespace_id_);

  // Verify that the persistent IDs are handled correctly when cloning.
  const int kClonedSessionStorageNamespaceId = 2 + session_id_offset();
  const std::string kClonedPersistentId = "cloned";
  context_->CloneSessionNamespace(kFirstSessionStorageNamespaceId,
                                  kClonedSessionStorageNamespaceId,
                                  kClonedPersistentId);
  DOMStorageNamespace* cloned_dom_namespace =
      context_->GetStorageNamespace(kClonedSessionStorageNamespaceId);
  ASSERT_TRUE(dom_namespace);
  EXPECT_EQ(kClonedPersistentId,
            cloned_dom_namespace->persistent_namespace_id());
  // Verify that the areas inherit the persistent ID.
  DOMStorageArea* cloned_area = cloned_dom_namespace->OpenStorageArea(kOrigin);
  EXPECT_EQ(kClonedPersistentId, cloned_area->persistent_namespace_id_);
}

TEST_F(DOMStorageContextImplTest, DeleteSessionStorage) {
  // Create a DOMStorageContextImpl which will save sessionStorage on disk.
  context_ = new DOMStorageContextImpl(temp_dir_.path(),
                                       temp_dir_.path(),
                                       storage_policy_.get(),
                                       task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();
  ASSERT_EQ(temp_dir_.path(), context_->sessionstorage_directory());

  // Write data.
  const int kSessionStorageNamespaceId = 1 + session_id_offset();
  const std::string kPersistentId = "persistent";
  context_->CreateSessionNamespace(kSessionStorageNamespaceId,
                                   kPersistentId);
  DOMStorageNamespace* dom_namespace =
      context_->GetStorageNamespace(kSessionStorageNamespaceId);
  DOMStorageArea* area = dom_namespace->OpenStorageArea(kOrigin);
  const base::string16 kKey(ASCIIToUTF16("foo"));
  const base::string16 kValue(ASCIIToUTF16("bar"));
  base::NullableString16 old_nullable_value;
  area->SetItem(kKey, kValue, &old_nullable_value);
  dom_namespace->CloseStorageArea(area);

  // Destroy and recreate the DOMStorageContextImpl.
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();
  context_ = new DOMStorageContextImpl(
      temp_dir_.path(), temp_dir_.path(),
      storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();

  // Read the data back.
  context_->CreateSessionNamespace(kSessionStorageNamespaceId,
                                   kPersistentId);
  dom_namespace = context_->GetStorageNamespace(kSessionStorageNamespaceId);
  area = dom_namespace->OpenStorageArea(kOrigin);
  base::NullableString16 read_value;
  read_value = area->GetItem(kKey);
  EXPECT_EQ(kValue, read_value.string());
  dom_namespace->CloseStorageArea(area);

  SessionStorageUsageInfo info;
  info.origin = kOrigin;
  info.persistent_namespace_id = kPersistentId;
  context_->DeleteSessionStorage(info);

  // Destroy and recreate again.
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();
  context_ = new DOMStorageContextImpl(
      temp_dir_.path(), temp_dir_.path(),
      storage_policy_.get(), task_runner_.get());
  context_->SetSaveSessionStorageOnDisk();

  // Now there should be no data.
  context_->CreateSessionNamespace(kSessionStorageNamespaceId,
                                   kPersistentId);
  dom_namespace = context_->GetStorageNamespace(kSessionStorageNamespaceId);
  area = dom_namespace->OpenStorageArea(kOrigin);
  read_value = area->GetItem(kKey);
  EXPECT_TRUE(read_value.is_null());
  dom_namespace->CloseStorageArea(area);
  context_->Shutdown();
  context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();
}

}  // namespace content
