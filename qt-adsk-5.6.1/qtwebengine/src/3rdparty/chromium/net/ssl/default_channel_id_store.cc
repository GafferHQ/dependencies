// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/default_channel_id_store.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "crypto/ec_private_key.h"
#include "net/base/net_errors.h"

namespace net {

// --------------------------------------------------------------------------
// Task
class DefaultChannelIDStore::Task {
 public:
  virtual ~Task();

  // Runs the task and invokes the client callback on the thread that
  // originally constructed the task.
  virtual void Run(DefaultChannelIDStore* store) = 0;

 protected:
  void InvokeCallback(base::Closure callback) const;
};

DefaultChannelIDStore::Task::~Task() {
}

void DefaultChannelIDStore::Task::InvokeCallback(
    base::Closure callback) const {
  if (!callback.is_null())
    callback.Run();
}

// --------------------------------------------------------------------------
// GetChannelIDTask
class DefaultChannelIDStore::GetChannelIDTask
    : public DefaultChannelIDStore::Task {
 public:
  GetChannelIDTask(const std::string& server_identifier,
                   const GetChannelIDCallback& callback);
  ~GetChannelIDTask() override;
  void Run(DefaultChannelIDStore* store) override;

 private:
  std::string server_identifier_;
  GetChannelIDCallback callback_;
};

DefaultChannelIDStore::GetChannelIDTask::GetChannelIDTask(
    const std::string& server_identifier,
    const GetChannelIDCallback& callback)
    : server_identifier_(server_identifier),
      callback_(callback) {
}

DefaultChannelIDStore::GetChannelIDTask::~GetChannelIDTask() {
}

void DefaultChannelIDStore::GetChannelIDTask::Run(
    DefaultChannelIDStore* store) {
  scoped_ptr<crypto::ECPrivateKey> key_result;
  int err = store->GetChannelID(server_identifier_, &key_result,
                                GetChannelIDCallback());
  DCHECK(err != ERR_IO_PENDING);

  InvokeCallback(base::Bind(callback_, err, server_identifier_,
                            base::Passed(key_result.Pass())));
}

// --------------------------------------------------------------------------
// SetChannelIDTask
class DefaultChannelIDStore::SetChannelIDTask
    : public DefaultChannelIDStore::Task {
 public:
  SetChannelIDTask(scoped_ptr<ChannelID> channel_id);
  ~SetChannelIDTask() override;
  void Run(DefaultChannelIDStore* store) override;

 private:
  scoped_ptr<ChannelID> channel_id_;
};

DefaultChannelIDStore::SetChannelIDTask::SetChannelIDTask(
    scoped_ptr<ChannelID> channel_id)
    : channel_id_(channel_id.Pass()) {
}

DefaultChannelIDStore::SetChannelIDTask::~SetChannelIDTask() {
}

void DefaultChannelIDStore::SetChannelIDTask::Run(
    DefaultChannelIDStore* store) {
  store->SyncSetChannelID(channel_id_.Pass());
}

// --------------------------------------------------------------------------
// DeleteChannelIDTask
class DefaultChannelIDStore::DeleteChannelIDTask
    : public DefaultChannelIDStore::Task {
 public:
  DeleteChannelIDTask(const std::string& server_identifier,
                      const base::Closure& callback);
  ~DeleteChannelIDTask() override;
  void Run(DefaultChannelIDStore* store) override;

 private:
  std::string server_identifier_;
  base::Closure callback_;
};

DefaultChannelIDStore::DeleteChannelIDTask::
    DeleteChannelIDTask(
        const std::string& server_identifier,
        const base::Closure& callback)
        : server_identifier_(server_identifier),
          callback_(callback) {
}

DefaultChannelIDStore::DeleteChannelIDTask::
    ~DeleteChannelIDTask() {
}

void DefaultChannelIDStore::DeleteChannelIDTask::Run(
    DefaultChannelIDStore* store) {
  store->SyncDeleteChannelID(server_identifier_);

  InvokeCallback(callback_);
}

// --------------------------------------------------------------------------
// DeleteAllCreatedBetweenTask
class DefaultChannelIDStore::DeleteAllCreatedBetweenTask
    : public DefaultChannelIDStore::Task {
 public:
  DeleteAllCreatedBetweenTask(base::Time delete_begin,
                              base::Time delete_end,
                              const base::Closure& callback);
  ~DeleteAllCreatedBetweenTask() override;
  void Run(DefaultChannelIDStore* store) override;

 private:
  base::Time delete_begin_;
  base::Time delete_end_;
  base::Closure callback_;
};

DefaultChannelIDStore::DeleteAllCreatedBetweenTask::
    DeleteAllCreatedBetweenTask(
        base::Time delete_begin,
        base::Time delete_end,
        const base::Closure& callback)
        : delete_begin_(delete_begin),
          delete_end_(delete_end),
          callback_(callback) {
}

DefaultChannelIDStore::DeleteAllCreatedBetweenTask::
    ~DeleteAllCreatedBetweenTask() {
}

void DefaultChannelIDStore::DeleteAllCreatedBetweenTask::Run(
    DefaultChannelIDStore* store) {
  store->SyncDeleteAllCreatedBetween(delete_begin_, delete_end_);

  InvokeCallback(callback_);
}

// --------------------------------------------------------------------------
// GetAllChannelIDsTask
class DefaultChannelIDStore::GetAllChannelIDsTask
    : public DefaultChannelIDStore::Task {
 public:
  explicit GetAllChannelIDsTask(const GetChannelIDListCallback& callback);
  ~GetAllChannelIDsTask() override;
  void Run(DefaultChannelIDStore* store) override;

 private:
  std::string server_identifier_;
  GetChannelIDListCallback callback_;
};

DefaultChannelIDStore::GetAllChannelIDsTask::
    GetAllChannelIDsTask(const GetChannelIDListCallback& callback)
        : callback_(callback) {
}

DefaultChannelIDStore::GetAllChannelIDsTask::
    ~GetAllChannelIDsTask() {
}

void DefaultChannelIDStore::GetAllChannelIDsTask::Run(
    DefaultChannelIDStore* store) {
  ChannelIDList key_list;
  store->SyncGetAllChannelIDs(&key_list);

  InvokeCallback(base::Bind(callback_, key_list));
}

// --------------------------------------------------------------------------
// DefaultChannelIDStore

DefaultChannelIDStore::DefaultChannelIDStore(
    PersistentStore* store)
    : initialized_(false),
      loaded_(false),
      store_(store),
      weak_ptr_factory_(this) {}

int DefaultChannelIDStore::GetChannelID(
    const std::string& server_identifier,
    scoped_ptr<crypto::ECPrivateKey>* key_result,
    const GetChannelIDCallback& callback) {
  DCHECK(CalledOnValidThread());
  InitIfNecessary();

  if (!loaded_) {
    EnqueueTask(scoped_ptr<Task>(
        new GetChannelIDTask(server_identifier, callback)));
    return ERR_IO_PENDING;
  }

  ChannelIDMap::iterator it = channel_ids_.find(server_identifier);

  if (it == channel_ids_.end())
    return ERR_FILE_NOT_FOUND;

  ChannelID* channel_id = it->second;
  key_result->reset(channel_id->key()->Copy());

  return OK;
}

void DefaultChannelIDStore::SetChannelID(scoped_ptr<ChannelID> channel_id) {
  auto task = new SetChannelIDTask(channel_id.Pass());
  RunOrEnqueueTask(scoped_ptr<Task>(task));
}

void DefaultChannelIDStore::DeleteChannelID(
    const std::string& server_identifier,
    const base::Closure& callback) {
  RunOrEnqueueTask(scoped_ptr<Task>(
      new DeleteChannelIDTask(server_identifier, callback)));
}

void DefaultChannelIDStore::DeleteAllCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    const base::Closure& callback) {
  RunOrEnqueueTask(scoped_ptr<Task>(
      new DeleteAllCreatedBetweenTask(delete_begin, delete_end, callback)));
}

void DefaultChannelIDStore::DeleteAll(
    const base::Closure& callback) {
  DeleteAllCreatedBetween(base::Time(), base::Time(), callback);
}

void DefaultChannelIDStore::GetAllChannelIDs(
    const GetChannelIDListCallback& callback) {
  RunOrEnqueueTask(scoped_ptr<Task>(new GetAllChannelIDsTask(callback)));
}

int DefaultChannelIDStore::GetChannelIDCount() {
  DCHECK(CalledOnValidThread());

  return channel_ids_.size();
}

void DefaultChannelIDStore::SetForceKeepSessionState() {
  DCHECK(CalledOnValidThread());
  InitIfNecessary();

  if (store_.get())
    store_->SetForceKeepSessionState();
}

DefaultChannelIDStore::~DefaultChannelIDStore() {
  DeleteAllInMemory();
}

void DefaultChannelIDStore::DeleteAllInMemory() {
  DCHECK(CalledOnValidThread());

  for (ChannelIDMap::iterator it = channel_ids_.begin();
       it != channel_ids_.end(); ++it) {
    delete it->second;
  }
  channel_ids_.clear();
}

void DefaultChannelIDStore::InitStore() {
  DCHECK(CalledOnValidThread());
  DCHECK(store_.get()) << "Store must exist to initialize";
  DCHECK(!loaded_);

  store_->Load(base::Bind(&DefaultChannelIDStore::OnLoaded,
                          weak_ptr_factory_.GetWeakPtr()));
}

void DefaultChannelIDStore::OnLoaded(
    scoped_ptr<ScopedVector<ChannelID> > channel_ids) {
  DCHECK(CalledOnValidThread());

  for (std::vector<ChannelID*>::const_iterator it = channel_ids->begin();
       it != channel_ids->end(); ++it) {
    DCHECK(channel_ids_.find((*it)->server_identifier()) ==
           channel_ids_.end());
    channel_ids_[(*it)->server_identifier()] = *it;
  }
  channel_ids->weak_clear();

  loaded_ = true;

  base::TimeDelta wait_time;
  if (!waiting_tasks_.empty())
    wait_time = base::TimeTicks::Now() - waiting_tasks_start_time_;
  DVLOG(1) << "Task delay " << wait_time.InMilliseconds();
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.TaskMaxWaitTime",
                             wait_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1),
                             50);
  UMA_HISTOGRAM_COUNTS_100("DomainBoundCerts.TaskWaitCount",
                           waiting_tasks_.size());


  for (ScopedVector<Task>::iterator i = waiting_tasks_.begin();
       i != waiting_tasks_.end(); ++i)
    (*i)->Run(this);
  waiting_tasks_.clear();
}

void DefaultChannelIDStore::SyncSetChannelID(scoped_ptr<ChannelID> channel_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);

  InternalDeleteChannelID(channel_id->server_identifier());
  InternalInsertChannelID(channel_id.Pass());
}

void DefaultChannelIDStore::SyncDeleteChannelID(
    const std::string& server_identifier) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);
  InternalDeleteChannelID(server_identifier);
}

void DefaultChannelIDStore::SyncDeleteAllCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);
  for (ChannelIDMap::iterator it = channel_ids_.begin();
       it != channel_ids_.end();) {
    ChannelIDMap::iterator cur = it;
    ++it;
    ChannelID* channel_id = cur->second;
    if ((delete_begin.is_null() ||
         channel_id->creation_time() >= delete_begin) &&
        (delete_end.is_null() || channel_id->creation_time() < delete_end)) {
      if (store_.get())
        store_->DeleteChannelID(*channel_id);
      delete channel_id;
      channel_ids_.erase(cur);
    }
  }
}

void DefaultChannelIDStore::SyncGetAllChannelIDs(
    ChannelIDList* channel_id_list) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);
  for (ChannelIDMap::iterator it = channel_ids_.begin();
       it != channel_ids_.end(); ++it)
    channel_id_list->push_back(*it->second);
}

void DefaultChannelIDStore::EnqueueTask(scoped_ptr<Task> task) {
  DCHECK(CalledOnValidThread());
  DCHECK(!loaded_);
  if (waiting_tasks_.empty())
    waiting_tasks_start_time_ = base::TimeTicks::Now();
  waiting_tasks_.push_back(task.Pass());
}

void DefaultChannelIDStore::RunOrEnqueueTask(scoped_ptr<Task> task) {
  DCHECK(CalledOnValidThread());
  InitIfNecessary();

  if (!loaded_) {
    EnqueueTask(task.Pass());
    return;
  }

  task->Run(this);
}

void DefaultChannelIDStore::InternalDeleteChannelID(
    const std::string& server_identifier) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);

  ChannelIDMap::iterator it = channel_ids_.find(server_identifier);
  if (it == channel_ids_.end())
    return;  // There is nothing to delete.

  ChannelID* channel_id = it->second;
  if (store_.get())
    store_->DeleteChannelID(*channel_id);
  channel_ids_.erase(it);
  delete channel_id;
}

void DefaultChannelIDStore::InternalInsertChannelID(
    scoped_ptr<ChannelID> channel_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded_);

  if (store_.get())
    store_->AddChannelID(*(channel_id.get()));
  const std::string& server_identifier = channel_id->server_identifier();
  channel_ids_[server_identifier] = channel_id.release();
}

DefaultChannelIDStore::PersistentStore::PersistentStore() {}

DefaultChannelIDStore::PersistentStore::~PersistentStore() {}

}  // namespace net
