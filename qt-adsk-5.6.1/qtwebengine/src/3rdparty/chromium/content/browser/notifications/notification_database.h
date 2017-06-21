// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_H_

#include <stdint.h>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"

class GURL;

namespace leveldb {
class DB;
class Env;
class FilterPolicy;
class WriteBatch;
}

namespace content {

struct NotificationDatabaseData;

// Implementation of the persistent notification database.
//
// The database is built on top of a LevelDB database, either in memory or on
// the filesystem depending on the path passed to the constructor. When writing
// a new notification, it will be assigned an id guaranteed to be unique for the
// lifetime of the database.
//
// This class must only be used on a thread or sequenced task runner that allows
// file I/O. The same thread or task runner must be used for all method calls.
class CONTENT_EXPORT NotificationDatabase {
 public:
  // Result status codes for interations with the database. Will be used for
  // UMA, so the assigned ids must remain stable.
  enum Status {
    STATUS_OK = 0,

    // The database, a notification, or a LevelDB key associated with the
    // operation could not be found.
    STATUS_ERROR_NOT_FOUND = 1,

    // The database, or data in the database, could not be parsed as valid data.
    STATUS_ERROR_CORRUPTED = 2,

    // General failure code. More specific failures should be used if available.
    STATUS_ERROR_FAILED = 3,

    // Number of entries in the status enumeration. Used by UMA. Must always be
    // one higher than the otherwise highest value in this enumeration.
    STATUS_COUNT = 4
  };

  explicit NotificationDatabase(const base::FilePath& path);
  ~NotificationDatabase();

  // Opens the database. If |path| is non-empty, it will be created on the given
  // directory on the filesystem. If |path| is empty, the database will be
  // created in memory instead, and its lifetime will be tied to this instance.
  // |create_if_missing| determines whether to create the database if necessary.
  Status Open(bool create_if_missing);

  // Reads the notification data for the notification identified by
  // |notification_id| and belonging to |origin| from the database, and stores
  // it in |notification_database_data|. Returns the status code.
  Status ReadNotificationData(
      int64_t notification_id,
      const GURL& origin,
      NotificationDatabaseData* notification_database_data) const;

  // Reads all notification data for all origins from the database, and appends
  // the data to |notification_data_vector|. Returns the status code.
  Status ReadAllNotificationData(
      std::vector<NotificationDatabaseData>* notification_data_vector) const;

  // Reads all notification data associated with |origin| from the database, and
  // appends the data to |notification_data_vector|. Returns the status code.
  Status ReadAllNotificationDataForOrigin(
      const GURL& origin,
      std::vector<NotificationDatabaseData>* notification_data_vector) const;

  // Reads all notification data associated to |service_worker_registration_id|
  // belonging to |origin| from the database, and appends the data to the
  // |notification_data_vector|. Returns the status code.
  Status ReadAllNotificationDataForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      std::vector<NotificationDatabaseData>* notification_data_vector) const;

  // Writes the |notification_database_data| for a new notification belonging to
  // |origin| to the database, and returns the status code of the writing
  // operation. The id of the new notification will be set in |notification_id|.
  Status WriteNotificationData(
      const GURL& origin,
      const NotificationDatabaseData& notification_database_data,
      int64_t* notification_id);

  // Deletes all data associated with the notification identified by
  // |notification_id| belonging to |origin| from the database. Returns the
  // status code of the deletion operation. Note that it is not considered a
  // failure if the to-be-deleted notification does not exist.
  Status DeleteNotificationData(int64_t notification_id, const GURL& origin);

  // Deletes all data associated with |origin| from the database, and appends
  // the deleted notification ids to |deleted_notification_set|. Returns the
  // status code of the deletion operation.
  Status DeleteAllNotificationDataForOrigin(
      const GURL& origin,
      std::set<int64_t>* deleted_notification_set);

  // Deletes all data associated with the |service_worker_registration_id|
  // belonging to |origin| from the database, and appends the deleted
  // notification ids to |deleted_notification_set|. Returns the status code
  // of the deletion operation.
  Status DeleteAllNotificationDataForServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id,
      std::set<int64_t>* deleted_notification_set);

  // Completely destroys the contents of this database.
  Status Destroy();

 private:
  friend class NotificationDatabaseTest;

  // TODO(peter): Convert to an enum class when DCHECK_EQ supports this.
  // See https://crbug.com/463869.
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZED,
    STATE_DISABLED,
  };

  // Reads the next available notification id from the database and returns
  // the status code of the reading operation. The value will be stored in
  // the |next_notification_id_| member.
  Status ReadNextNotificationId();

  // Reads all notification data with the given constraints. |origin| may be
  // empty to read all notification data from all origins. If |origin| is
  // set, but |service_worker_registration_id| is invalid, then all notification
  // data for |origin| will be read. If both are set, then all notification data
  // for the given |service_worker_registration_id| will be read.
  Status ReadAllNotificationDataInternal(
      const GURL& origin,
      int64_t service_worker_registration_id,
      std::vector<NotificationDatabaseData>* notification_data_vector) const;

  // Deletes all notification data with the given constraints. |origin| must
  // always be set - use Destroy() when the goal is to empty the database. If
  // |service_worker_registration_id| is invalid, all notification data for the
  // |origin| will be deleted.
  // All deleted notification ids will be written to |deleted_notification_set|.
  Status DeleteAllNotificationDataInternal(
      const GURL& origin,
      int64_t service_worker_registration_id,
      std::set<int64_t>* deleted_notification_set);

  // Returns whether the database has been opened.
  bool IsOpen() const { return db_ != nullptr; }

  // Returns whether the database should only exist in memory.
  bool IsInMemoryDatabase() const { return path_.empty(); }

  // Exposes the LevelDB database used to back this notification database.
  // Should only be used for testing purposes.
  leveldb::DB* GetDBForTesting() const { return db_.get(); }

  base::FilePath path_;

  int64_t next_notification_id_ = 0;

  scoped_ptr<const leveldb::FilterPolicy> filter_policy_;

  // The declaration order for these members matters, as |db_| depends on |env_|
  // and thus has to be destructed first.
  scoped_ptr<leveldb::Env> env_;
  scoped_ptr<leveldb::DB> db_;

  State state_ = STATE_UNINITIALIZED;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_H_
