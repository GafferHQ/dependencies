// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/local_fetcher.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/util/filename_util.h"
#include "url/url_util.h"

namespace mojo {
namespace shell {

namespace {

void IgnoreResult(bool result) {
}

}  // namespace

// A loader for local files.
LocalFetcher::LocalFetcher(const GURL& url,
                           const GURL& url_without_query,
                           const FetchCallback& loader_callback)
    : Fetcher(loader_callback),
      url_(url),
      path_(util::UrlToFilePath(url_without_query)) {
  TRACE_EVENT1("mojo_shell", "LocalFetcher::LocalFetcher", "url", url.spec());
  loader_callback_.Run(make_scoped_ptr(this));
}

const GURL& LocalFetcher::GetURL() const {
  return url_;
}

GURL LocalFetcher::GetRedirectURL() const {
  return GURL::EmptyGURL();
}

GURL LocalFetcher::GetRedirectReferer() const {
  return GURL::EmptyGURL();
}

URLResponsePtr LocalFetcher::AsURLResponse(base::TaskRunner* task_runner,
                                           uint32_t skip) {
  URLResponsePtr response(URLResponse::New());
  response->url = String::From(url_);
  DataPipe data_pipe;
  response->body = data_pipe.consumer_handle.Pass();
  int64 file_size;
  if (base::GetFileSize(path_, &file_size)) {
    response->headers = Array<HttpHeaderPtr>(1);
    HttpHeaderPtr header = HttpHeader::New();
    header->name = "Content-Length";
    header->value = base::StringPrintf("%" PRId64, file_size);
    response->headers[0] = header.Pass();
  }
  common::CopyFromFile(path_, data_pipe.producer_handle.Pass(), skip,
                       task_runner, base::Bind(&IgnoreResult));
  return response.Pass();
}

void LocalFetcher::AsPath(
    base::TaskRunner* task_runner,
    base::Callback<void(const base::FilePath&, bool)> callback) {
  // Async for consistency with network case.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, path_, base::PathExists(path_)));
}

std::string LocalFetcher::MimeType() {
  return "";
}

bool LocalFetcher::HasMojoMagic() {
  std::string magic;
  ReadFileToString(path_, &magic, strlen(kMojoMagic));
  return magic == kMojoMagic;
}

bool LocalFetcher::PeekFirstLine(std::string* line) {
  std::string start_of_file;
  ReadFileToString(path_, &start_of_file, kMaxShebangLength);
  size_t return_position = start_of_file.find('\n');
  if (return_position == std::string::npos)
    return false;
  *line = start_of_file.substr(0, return_position + 1);
  return true;
}

}  // namespace shell
}  // namespace mojo
