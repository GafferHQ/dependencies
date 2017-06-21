// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/serializers.h"

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"

namespace chromecast {

scoped_ptr<base::Value> DeserializeFromJson(const std::string& text) {
  JSONStringValueDeserializer deserializer(text);

  int error_code;
  std::string error_msg;
  scoped_ptr<base::Value> value(
      deserializer.Deserialize(&error_code, &error_msg));
  DLOG_IF(ERROR, !value) << "JSON error " << error_code << ":" << error_msg;

  // Value will hold the nullptr in case of an error.
  return value.Pass();
}

scoped_ptr<std::string> SerializeToJson(const base::Value& value) {
  scoped_ptr<std::string> json_str(new std::string());
  JSONStringValueSerializer serializer(json_str.get());
  if (!serializer.Serialize(value))
    json_str.reset(nullptr);
  return json_str.Pass();
}

}  // namespace chromecast
