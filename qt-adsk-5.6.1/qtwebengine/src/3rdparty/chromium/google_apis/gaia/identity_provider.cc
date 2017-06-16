// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/scoped_tracker.h"
#include "google_apis/gaia/identity_provider.h"

IdentityProvider::Observer::~Observer() {}

IdentityProvider::~IdentityProvider() {}

void IdentityProvider::AddActiveAccountRefreshTokenObserver(
    OAuth2TokenService::Observer* observer) {
  OAuth2TokenService* token_service = GetTokenService();
  if (!token_service || token_service_observers_.HasObserver(observer))
    return;

  token_service_observers_.AddObserver(observer);
  if (++token_service_observer_count_ == 1)
    token_service->AddObserver(this);
}

void IdentityProvider::RemoveActiveAccountRefreshTokenObserver(
    OAuth2TokenService::Observer* observer) {
  OAuth2TokenService* token_service = GetTokenService();
  if (!token_service || !token_service_observers_.HasObserver(observer))
    return;

  token_service_observers_.RemoveObserver(observer);
  if (--token_service_observer_count_ == 0)
    token_service->RemoveObserver(this);
}

void IdentityProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IdentityProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void IdentityProvider::OnRefreshTokenAvailable(const std::string& account_id) {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 IdentityProvider::OnRefreshTokenAvailable"));

  if (account_id != GetActiveAccountId())
    return;
  FOR_EACH_OBSERVER(OAuth2TokenService::Observer,
                    token_service_observers_,
                    OnRefreshTokenAvailable(account_id));
}

void IdentityProvider::OnRefreshTokenRevoked(const std::string& account_id) {
  if (account_id != GetActiveAccountId())
    return;
  FOR_EACH_OBSERVER(OAuth2TokenService::Observer,
                    token_service_observers_,
                    OnRefreshTokenRevoked(account_id));
}

void IdentityProvider::OnRefreshTokensLoaded() {
  FOR_EACH_OBSERVER(OAuth2TokenService::Observer,
                    token_service_observers_,
                    OnRefreshTokensLoaded());
}

IdentityProvider::IdentityProvider() : token_service_observer_count_(0) {}

void IdentityProvider::FireOnActiveAccountLogin() {
  FOR_EACH_OBSERVER(Observer, observers_, OnActiveAccountLogin());
}

void IdentityProvider::FireOnActiveAccountLogout() {
  FOR_EACH_OBSERVER(Observer, observers_, OnActiveAccountLogout());
}
