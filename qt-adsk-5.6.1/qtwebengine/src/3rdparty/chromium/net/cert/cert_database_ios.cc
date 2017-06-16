// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_database.h"

#include "base/logging.h"
#include "base/observer_list_threadsafe.h"
#include "net/base/net_errors.h"

namespace net {

CertDatabase::CertDatabase()
    : observer_list_(new base::ObserverListThreadSafe<Observer>) {
}

CertDatabase::~CertDatabase() {}

int CertDatabase::CheckUserCert(X509Certificate* cert_obj) {
  // iOS doesn't handle user certificates.
  NOTREACHED();
  return OK;
}

int CertDatabase::AddUserCert(X509Certificate* cert_obj) {
  // iOS doesn't handle user certificates.
  NOTREACHED();
  return OK;
}

}  // namespace net
