// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/mojo/sql_test_base.h"

#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/util/capture_util.h"
#include "sql/mojo/mojo_vfs.h"
#include "sql/test/test_helpers.h"

using mojo::Capture;

namespace sql {

SQLTestBase::SQLTestBase() {
}

SQLTestBase::~SQLTestBase() {
}

base::FilePath SQLTestBase::db_path() {
  return base::FilePath(FILE_PATH_LITERAL("SQLTest.db"));
}

sql::Connection& SQLTestBase::db() {
  return db_;
}

bool SQLTestBase::Reopen() {
  db_.Close();
  return db_.Open(db_path());
}

bool SQLTestBase::GetPathExists(const base::FilePath& path) {
  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  bool exists = false;
  vfs_->GetDirectory()->Exists(path.AsUTF8Unsafe(), Capture(&error, &exists));
  vfs_->GetDirectory().WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK)
    return false;
  return exists;
}

bool SQLTestBase::CorruptSizeInHeaderOfDB() {
  // See http://www.sqlite.org/fileformat.html#database_header
  const size_t kHeaderSize = 100;

  mojo::Array<uint8_t> header;

  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  filesystem::FilePtr file_ptr;
  vfs_->GetDirectory()->OpenFile(
      mojo::String(db_path().AsUTF8Unsafe()), GetProxy(&file_ptr),
      filesystem::kFlagRead | filesystem::kFlagWrite |
          filesystem::kFlagOpenAlways,
      Capture(&error));
  vfs_->GetDirectory().WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK)
    return false;

  file_ptr->Read(kHeaderSize, 0, filesystem::WHENCE_FROM_BEGIN,
                 Capture(&error, &header));
  file_ptr.WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK)
    return false;

  filesystem::FileInformationPtr info;
  file_ptr->Stat(Capture(&error, &info));
  file_ptr.WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK)
    return false;
  int64_t db_size = info->size;

  test::CorruptSizeInHeaderMemory(&header.front(), db_size);

  uint32_t num_bytes_written = 0;
  file_ptr->Write(header.Pass(), 0, filesystem::WHENCE_FROM_BEGIN,
                  Capture(&error, &num_bytes_written));
  file_ptr.WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK)
    return false;
  if (num_bytes_written != kHeaderSize)
    return false;

  return true;
}

void SQLTestBase::WriteJunkToDatabase(WriteJunkType type) {
  uint32_t flags = 0;
  if (type == TYPE_OVERWRITE_AND_TRUNCATE)
    flags = filesystem::kFlagWrite | filesystem::kFlagCreate;
  else
    flags = filesystem::kFlagWrite | filesystem::kFlagOpen;

  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  filesystem::FilePtr file_ptr;
  vfs_->GetDirectory()->OpenFile(
      mojo::String(db_path().AsUTF8Unsafe()), GetProxy(&file_ptr),
      flags,
      Capture(&error));
  vfs_->GetDirectory().WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK)
    return;

  const char* kJunk = "Now is the winter of our discontent.";
  mojo::Array<uint8_t> data(strlen(kJunk));
  memcpy(&data.front(), kJunk, strlen(kJunk));

  uint32_t num_bytes_written = 0;
  file_ptr->Write(data.Pass(), 0, filesystem::WHENCE_FROM_BEGIN,
                  Capture(&error, &num_bytes_written));
  file_ptr.WaitForIncomingResponse();
}

void SQLTestBase::TruncateDatabase() {
  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  filesystem::FilePtr file_ptr;
  vfs_->GetDirectory()->OpenFile(
      mojo::String(db_path().AsUTF8Unsafe()), GetProxy(&file_ptr),
      filesystem::kFlagWrite | filesystem::kFlagOpen,
      Capture(&error));
  vfs_->GetDirectory().WaitForIncomingResponse();
  if (error != filesystem::FILE_ERROR_OK)
    return;

  file_ptr->Truncate(0, Capture(&error));
  file_ptr.WaitForIncomingResponse();
  ASSERT_EQ(filesystem::FILE_ERROR_OK, error);
}

void SQLTestBase::SetUp() {
  ApplicationTestBase::SetUp();

  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:filesystem");
  application_impl()->ConnectToService(request.Pass(), &files_);

  filesystem::FileError error = filesystem::FILE_ERROR_FAILED;
  filesystem::DirectoryPtr directory;
  files()->OpenFileSystem("temp", GetProxy(&directory), Capture(&error));
  ASSERT_TRUE(files().WaitForIncomingResponse());
  ASSERT_EQ(filesystem::FILE_ERROR_OK, error);

  vfs_.reset(new ScopedMojoFilesystemVFS(directory.Pass()));
  ASSERT_TRUE(db_.Open(db_path()));
}

void SQLTestBase::TearDown() {
  db_.Close();
  vfs_.reset();

  ApplicationTestBase::TearDown();
}

}  // namespace sql
