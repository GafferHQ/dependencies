// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/internal_blob_data.h"

#include "base/containers/hash_tables.h"
#include "base/metrics/histogram.h"
#include "storage/browser/blob/blob_data_item.h"
#include "storage/common/data_element.h"

namespace storage {

InternalBlobData::Builder::Builder() : data_(new InternalBlobData()) {
}
InternalBlobData::Builder::~Builder() {
}

void InternalBlobData::Builder::AppendSharedBlobItem(
    scoped_refptr<ShareableBlobDataItem> item) {
  DCHECK(item);
  DCHECK(data_);
  data_->items_.push_back(item);
}

void InternalBlobData::Builder::RemoveBlobFromShareableItems(
    const std::string& blob_uuid) {
  DCHECK(data_);
  data_->RemoveBlobFromShareableItems(blob_uuid);
}

void InternalBlobData::Builder::set_content_type(
    const std::string& content_type) {
  DCHECK(data_);
  data_->content_type_ = content_type;
}

void InternalBlobData::Builder::set_content_disposition(
    const std::string& content_disposition) {
  DCHECK(data_);
  data_->content_disposition_ = content_disposition;
}

size_t InternalBlobData::Builder::GetNonsharedMemoryUsage() const {
  DCHECK(data_);
  return data_->GetUnsharedMemoryUsage();
}

scoped_ptr<InternalBlobData> InternalBlobData::Builder::Build() {
  DCHECK(data_);
  return data_.Pass();
}

InternalBlobData::InternalBlobData() {
}

InternalBlobData::~InternalBlobData() {
}

const std::vector<scoped_refptr<ShareableBlobDataItem>>&
InternalBlobData::items() const {
  return items_;
}
const std::string& InternalBlobData::content_type() const {
  return content_type_;
}
const std::string& InternalBlobData::content_disposition() const {
  return content_disposition_;
}

void InternalBlobData::RemoveBlobFromShareableItems(
    const std::string& blob_uuid) {
  for (auto& data_item : items_) {
    data_item->referencing_blobs().erase(blob_uuid);
  }
}

size_t InternalBlobData::GetUnsharedMemoryUsage() const {
  size_t memory = 0;
  base::hash_set<void*> seen_items;
  for (const auto& data_item : items_) {
    if (data_item->item()->type() != DataElement::TYPE_BYTES ||
        data_item->referencing_blobs().size() > 1 ||
        seen_items.find(data_item.get()) != seen_items.end()) {
      continue;
    }
    memory += data_item->item()->length();
    seen_items.insert(data_item.get());
  }
  return memory;
}

void InternalBlobData::GetMemoryUsage(size_t* total_memory,
                                      size_t* unshared_memory) {
  *total_memory = 0;
  *unshared_memory = 0;
  base::hash_set<void*> seen_items;
  for (const auto& data_item : items_) {
    if (data_item->item()->type() == DataElement::TYPE_BYTES) {
      *total_memory += data_item->item()->length();
      if (data_item->referencing_blobs().size() == 1 &&
          seen_items.find(data_item.get()) == seen_items.end()) {
        *unshared_memory += data_item->item()->length();
        seen_items.insert(data_item.get());
      }
    }
  }
}

}  // namespace storage
