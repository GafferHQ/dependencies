// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_UTIL_NET_HTTP_MULTIPART_BUILDER_H_
#define CRASHPAD_UTIL_NET_HTTP_MULTIPART_BUILDER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "util/net/http_headers.h"

namespace crashpad {

class HTTPBodyStream;

//! \brief This class is used to build a MIME multipart message, conforming to
//!     RFC 2046, for use as a HTTP request body.
class HTTPMultipartBuilder {
 public:
  HTTPMultipartBuilder();
  ~HTTPMultipartBuilder();

  //! \brief Sets a `Content-Disposition: form-data` key-value pair.
  //!
  //! \param[in] key The key of the form data, specified as the `name` in the
  //!     multipart message. Any data previously set on this class with this
  //!     key will be overwritten.
  //! \param[in] value The value to set at the \a key.
  void SetFormData(const std::string& key, const std::string& value);

  //! \brief Specifies the file at \a path to have its contents uploaded as
  //!     multipart data, available at `name` of \a upload_file_name.
  //!
  //! \param[in] key The key of the form data, specified as the `name` in the
  //!     multipart message. Any data previously set on this class with this
  //!     key will be overwritten.
  //! \param[in] upload_file_name The `filename` to specify for this multipart
  //!     data attachment.
  //! \param[in] path The path of the file whose contents will be uploaded.
  //! \param[in] content_type The `Content-Type` to specify for the attachment.
  //!     If this is empty, `"application/octet-stream"` will be used.
  void SetFileAttachment(const std::string& key,
                         const std::string& upload_file_name,
                         const base::FilePath& path,
                         const std::string& content_type);

  //! \brief Generates the HTTPBodyStream for the data currently supplied to
  //!     the builder.
  //!
  //! \return A caller-owned HTTPBodyStream object.
  scoped_ptr<HTTPBodyStream> GetBodyStream();

  //! \brief Gets the header pair for `"Content-Type"`.
  HTTPHeaders::value_type GetContentType() const;

 private:
  struct FileAttachment {
    std::string filename;
    std::string content_type;
    base::FilePath path;
  };

  // Removes elements from both data maps at the specified |key|, to ensure
  // uniqueness across the entire HTTP body.
  void EraseKey(const std::string& key);

  std::string boundary_;
  std::map<std::string, std::string> form_data_;
  std::map<std::string, FileAttachment> file_attachments_;

  DISALLOW_COPY_AND_ASSIGN(HTTPMultipartBuilder);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_NET_HTTP_MULTIPART_BUILDER_H_
