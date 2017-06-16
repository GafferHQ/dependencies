// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_database.h"

#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"

namespace net {

// static
CertDatabase* CertDatabase::GetInstance() {
  // Leaky so it can be initialized on worker threads, and because there is no
  // useful cleanup to do.
  return Singleton<CertDatabase, LeakySingletonTraits<CertDatabase> >::get();
}

void CertDatabase::AddObserver(Observer* observer) {
  observer_list_->AddObserver(observer);
}

void CertDatabase::RemoveObserver(Observer* observer) {
  observer_list_->RemoveObserver(observer);
}

void CertDatabase::NotifyObserversOfCertAdded(const X509Certificate* cert) {
  observer_list_->Notify(FROM_HERE, &Observer::OnCertAdded,
                         make_scoped_refptr(cert));
}

void CertDatabase::NotifyObserversOfCertRemoved(const X509Certificate* cert) {
  observer_list_->Notify(FROM_HERE, &Observer::OnCertRemoved,
                         make_scoped_refptr(cert));
}

void CertDatabase::NotifyObserversOfCACertChanged(
    const X509Certificate* cert) {
  observer_list_->Notify(FROM_HERE, &Observer::OnCACertChanged,
                         make_scoped_refptr(cert));
}

}  // namespace net
