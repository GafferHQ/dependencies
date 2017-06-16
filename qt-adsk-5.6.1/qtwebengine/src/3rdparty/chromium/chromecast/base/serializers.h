// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_SERIALIZERS_H_
#define CHROMECAST_BASE_SERIALIZERS_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class Value;
}

namespace chromecast {

// Helper function which deserializes JSON |text| into a base::Value. If |text|
// is empty, is not valid JSON, or if some other deserialization error occurs,
// the return value will hold the NULL pointer.
scoped_ptr<base::Value> DeserializeFromJson(const std::string& text);

// Helper function which serializes |value| into a JSON string. If a
// serialization error occurs,the return value will hold the NULL pointer.
scoped_ptr<std::string> SerializeToJson(const base::Value& value);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_SERIALIZERS_H_
