// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_KNOWN_LOGS_H_
#define NET_CERT_CT_KNOWN_LOGS_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"

namespace net {

class CTLogVerifier;

namespace ct {

// CreateLogVerifiersForKnownLogs returns a vector of CT logs for all the known
// and trusted logs.
NET_EXPORT std::vector<scoped_refptr<CTLogVerifier>>
CreateLogVerifiersForKnownLogs();

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_CT_KNOWN_LOGS_H_
