// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLOB_STORAGE_BLOB_CONSOLIDATION_H_
#define CONTENT_CHILD_BLOB_STORAGE_BLOB_CONSOLIDATION_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "storage/common/data_element.h"
#include "third_party/WebKit/public/platform/WebThreadSafeData.h"

namespace content {

// This class facilitates the consolidation of memory items in blobs. No memory
// is copied to store items in this object. Instead, the memory is copied into
// the external char* array given to the ReadMemory method.
// Features:
// * Add_Item methods for building the blob.
// * consolidated_items for getting the consolidated items. This is
//   used to describe the blob to the browser.
// * total_memory to get the total memory size of the blob.
// * ReadMemory for reading arbitrary memory from any consolidated item.
//
// NOTE: this class does not do memory accounting or garbage collecting. The
// memory for the blob sticks around until this class is destructed.
class CONTENT_EXPORT BlobConsolidation {
 public:
  enum class ReadStatus {
    ERROR_UNKNOWN,
    ERROR_WRONG_TYPE,
    ERROR_OUT_OF_BOUNDS,
    OK
  };
  struct ConsolidatedItem {
    ConsolidatedItem();
    ConsolidatedItem(storage::DataElement::Type type,
                     uint64_t offset,
                     uint64_t length);
    ~ConsolidatedItem();

    storage::DataElement::Type type;
    uint64_t offset;
    uint64_t length;

    base::FilePath path;                // For TYPE_FILE.
    GURL filesystem_url;                // For TYPE_FILE_FILESYSTEM.
    double expected_modification_time;  // For TYPE_FILE, TYPE_FILE_FILESYSTEM.
    std::string blob_uuid;              // For TYPE_BLOB.
    // Only populated if len(items) > 1.  Used for binary search.
    // Since the offset of the first item is always 0, we exclude this.
    std::vector<size_t> offsets;                 // For TYPE_BYTES.
    std::vector<blink::WebThreadSafeData> data;  // For TYPE_BYTES.
  };

  BlobConsolidation();
  ~BlobConsolidation();

  void AddDataItem(const blink::WebThreadSafeData& data);
  void AddFileItem(const base::FilePath& path,
                   uint64_t offset,
                   uint64_t length,
                   double expected_modification_time);
  void AddBlobItem(const std::string& uuid, uint64_t offset, uint64_t length);
  void AddFileSystemItem(const GURL& url,
                         uint64_t offset,
                         uint64_t length,
                         double expected_modification_time);

  // These are the resulting consolidated items, constructed from the Add*
  // methods. This configuration is used to describe the data to the browser,
  // even though one consolidated memory items can contain multiple data parts.
  const std::vector<ConsolidatedItem>& consolidated_items() const {
    return consolidated_items_;
  }

  size_t total_memory() const { return total_memory_; }

  // Reads memory from the given item into the given buffer. Returns:
  // * ReadStatus::ERROR if the state or arguments are invalid (see error log),
  // * ReadStatus::ERROR_WRONG_TYPE if the item at the index isn't memory,
  // * ReadStatus::ERROR_OUT_OF_BOUNDS if index, offset, or size are invalid,
  // * ReadStatus::DONE if the memory has been successfully read.
  // Precondition: memory_out must be a valid pointer to memory with a size of
  //               at least consolidated_size.
  ReadStatus ReadMemory(size_t consolidated_item_index,
                        size_t consolidated_offset,
                        size_t consolidated_size,
                        void* memory_out);

 private:
  size_t total_memory_;
  std::vector<ConsolidatedItem> consolidated_items_;

  DISALLOW_COPY_AND_ASSIGN(BlobConsolidation);
};

}  // namespace content
#endif  // CONTENT_CHILD_BLOB_STORAGE_BLOB_CONSOLIDATION_H_
