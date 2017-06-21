// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/sdch_net_log_params.h"

#include "base/values.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace net {

scoped_ptr<base::Value> NetLogSdchResourceProblemCallback(
    SdchProblemCode problem,
    NetLogCaptureMode capture_mode) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("sdch_problem_code", problem);
  dict->SetInteger("net_error", ERR_FAILED);
  return dict.Pass();
}

scoped_ptr<base::Value> NetLogSdchDictionaryFetchProblemCallback(
    SdchProblemCode problem,
    const GURL& url,
    bool is_error,
    NetLogCaptureMode capture_mode) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetInteger("sdch_problem_code", problem);
  dict->SetString("dictionary_url", url.spec());
  if (is_error)
    dict->SetInteger("net_error", ERR_FAILED);
  return dict.Pass();
}

}  // namespace net
