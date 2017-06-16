// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_DATABASE_H_
#define NET_CERT_CERT_DATABASE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

template <typename T> struct DefaultSingletonTraits;

namespace base {
template <class ObserverType>
class ObserverListThreadSafe;
}

namespace net {

// This class provides cross-platform functions to verify and add user
// certificates, and to observe changes to the underlying certificate stores.

// TODO(gauravsh): This class could be augmented with methods
// for all operations that manipulate the underlying system
// certificate store.

class NET_EXPORT CertDatabase {
 public:
  // A CertDatabase::Observer will be notified on certificate database changes.
  // The change could be either a user certificate is added/removed or trust on
  // a certificate is changed. Observers can be registered via
  // CertDatabase::AddObserver, and can un-register with
  // CertDatabase::RemoveObserver.
  class NET_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // Will be called when a new certificate is added. If the imported cert can
    // be determined, |cert| will be non-NULL, but if not, or if multiple
    // certificates were imported, |cert| may be NULL.
    virtual void OnCertAdded(const X509Certificate* cert) {}

    // Will be called when a certificate is removed.
    virtual void OnCertRemoved(const X509Certificate* cert) {}

    // Will be called when a CA certificate was added, removed, or its trust
    // changed. This can also mean that a client certificate's trust changed.
    virtual void OnCACertChanged(const X509Certificate* cert) {}

   protected:
    Observer() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  // Returns the CertDatabase singleton.
  static CertDatabase* GetInstance();

  // Check whether this is a valid user cert that we have the private key for.
  // Returns OK or a network error code such as ERR_CERT_CONTAINS_ERRORS.
  int CheckUserCert(X509Certificate* cert);

  // Store user (client) certificate. Assumes CheckUserCert has already passed.
  // Returns OK, or ERR_ADD_USER_CERT_FAILED if there was a problem saving to
  // the platform cert database, or possibly other network error codes.
  int AddUserCert(X509Certificate* cert);

  // Registers |observer| to receive notifications of certificate changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.
  void AddObserver(Observer* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.
  void RemoveObserver(Observer* observer);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Configures the current message loop to observe and forward events from
  // Keychain services. The MessageLoop must have an associated CFRunLoop,
  // which means that this must be called from a MessageLoop of TYPE_UI.
  void SetMessageLoopForKeychainEvents();
#endif

#if defined(OS_ANDROID)
  // On Android, the system key store may be replaced with a device-specific
  // KeyStore used for storing client certificates. When the Java side replaces
  // the KeyStore used for client certificates, notifies the observers as if a
  // new client certificate was added.
  void OnAndroidKeyStoreChanged();

  // On Android, the system database is used. When the system notifies the
  // application that the certificates changed, the observers must be notified.
  void OnAndroidKeyChainChanged();
#endif

  // Synthetically injects notifications to all observers. In general, this
  // should only be called by the creator of the CertDatabase. Used to inject
  // notifcations from other DB interfaces.
  void NotifyObserversOfCertAdded(const X509Certificate* cert);
  void NotifyObserversOfCertRemoved(const X509Certificate* cert);
  void NotifyObserversOfCACertChanged(const X509Certificate* cert);

 private:
  friend struct DefaultSingletonTraits<CertDatabase>;

  CertDatabase();
  ~CertDatabase();

  const scoped_refptr<base::ObserverListThreadSafe<Observer>> observer_list_;

#if defined(OS_MACOSX) && !defined(OS_IOS)
  class Notifier;
  friend class Notifier;
  scoped_ptr<Notifier> notifier_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CertDatabase);
};

}  // namespace net

#endif  // NET_CERT_CERT_DATABASE_H_
