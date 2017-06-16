// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_impl.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace net {

////////////////////////////////////////////////////////////////////////////////
// HttpServerPropertiesManager

// The manager for creating and updating an HttpServerProperties (for example it
// tracks if a server supports SPDY or not).
//
// This class interacts with both the pref thread, where notifications of pref
// changes are received from, and the network thread, which owns it, and it
// persists the changes from network stack whether server supports SPDY or not.
//
// It must be constructed on the pref thread, to set up |pref_task_runner_| and
// the prefs listeners.
//
// ShutdownOnPrefThread must be called from pref thread before destruction, to
// release the prefs listeners on the pref thread.
//
// Class requires that update tasks from the Pref thread can post safely to the
// network thread, so the destruction order must guarantee that if |this|
// exists in pref thread, then a potential destruction on network thread will
// come after any task posted to network thread from that method on pref thread.
// This is used to go through network thread before the actual update starts,
// and grab a WeakPtr.
class NET_EXPORT HttpServerPropertiesManager : public HttpServerProperties {
 public:
  // Create an instance of the HttpServerPropertiesManager. The lifetime of the
  // PrefService objects must be longer than that of the
  // HttpServerPropertiesManager object. Must be constructed on the Pref thread.
  HttpServerPropertiesManager(
      PrefService* pref_service,
      const char* pref_path,
      scoped_refptr<base::SequencedTaskRunner> network_task_runner);
  ~HttpServerPropertiesManager() override;

  // Initialize on Network thread.
  void InitializeOnNetworkThread();

  // Prepare for shutdown. Must be called on the Pref thread before destruction.
  void ShutdownOnPrefThread();

  // Helper function for unit tests to set the version in the dictionary.
  static void SetVersion(base::DictionaryValue* http_server_properties_dict,
                         int version_number);

  // Deletes all data. Works asynchronously, but if a |completion| callback is
  // provided, it will be fired on the pref thread when everything is done.
  void Clear(const base::Closure& completion);

  // ----------------------------------
  // HttpServerProperties methods:
  // ----------------------------------

  base::WeakPtr<HttpServerProperties> GetWeakPtr() override;
  void Clear() override;
  bool SupportsRequestPriority(const HostPortPair& server) override;
  bool GetSupportsSpdy(const HostPortPair& server) override;
  void SetSupportsSpdy(const HostPortPair& server, bool support_spdy) override;
  bool RequiresHTTP11(const HostPortPair& server) override;
  void SetHTTP11Required(const HostPortPair& server) override;
  void MaybeForceHTTP11(const HostPortPair& server,
                        SSLConfig* ssl_config) override;
  AlternativeServiceVector GetAlternativeServices(
      const HostPortPair& origin) override;
  bool SetAlternativeService(const HostPortPair& origin,
                             const AlternativeService& alternative_service,
                             double alternative_probability) override;
  bool SetAlternativeServices(const HostPortPair& origin,
                              const AlternativeServiceInfoVector&
                                  alternative_service_info_vector) override;
  void MarkAlternativeServiceBroken(
      const AlternativeService& alternative_service) override;
  void MarkAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service) override;
  bool IsAlternativeServiceBroken(
      const AlternativeService& alternative_service) const override;
  bool WasAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service) override;
  void ConfirmAlternativeService(
      const AlternativeService& alternative_service) override;
  void ClearAlternativeServices(const HostPortPair& origin) override;
  const AlternativeServiceMap& alternative_service_map() const override;
  scoped_ptr<base::Value> GetAlternativeServiceInfoAsValue() const override;
  void SetAlternativeServiceProbabilityThreshold(double threshold) override;
  const SettingsMap& GetSpdySettings(
      const HostPortPair& host_port_pair) override;
  bool SetSpdySetting(const HostPortPair& host_port_pair,
                      SpdySettingsIds id,
                      SpdySettingsFlags flags,
                      uint32 value) override;
  void ClearSpdySettings(const HostPortPair& host_port_pair) override;
  void ClearAllSpdySettings() override;
  const SpdySettingsMap& spdy_settings_map() const override;
  bool GetSupportsQuic(IPAddressNumber* last_address) const override;
  void SetSupportsQuic(bool used_quic,
                       const IPAddressNumber& last_address) override;
  void SetServerNetworkStats(const HostPortPair& host_port_pair,
                             ServerNetworkStats stats) override;
  const ServerNetworkStats* GetServerNetworkStats(
      const HostPortPair& host_port_pair) override;
  const ServerNetworkStatsMap& server_network_stats_map() const override;

 protected:
  // The location where ScheduleUpdatePrefsOnNetworkThread was called.
  enum Location {
    SUPPORTS_SPDY = 0,
    HTTP_11_REQUIRED = 1,
    SET_ALTERNATIVE_SERVICES = 2,
    MARK_ALTERNATIVE_SERVICE_BROKEN = 3,
    MARK_ALTERNATIVE_SERVICE_RECENTLY_BROKEN = 4,
    CONFIRM_ALTERNATIVE_SERVICE = 5,
    CLEAR_ALTERNATIVE_SERVICE = 6,
    SET_SPDY_SETTING = 7,
    CLEAR_SPDY_SETTINGS = 8,
    CLEAR_ALL_SPDY_SETTINGS = 9,
    SET_SUPPORTS_QUIC = 10,
    SET_SERVER_NETWORK_STATS = 11,
    DETECTED_CORRUPTED_PREFS = 12,
    NUM_LOCATIONS = 13,
  };

  // --------------------
  // SPDY related methods

  // These are used to delay updating of the cached data in
  // |http_server_properties_impl_| while the preferences are changing, and
  // execute only one update per simultaneous prefs changes.
  void ScheduleUpdateCacheOnPrefThread();

  // Starts the timers to update the cached prefs. This are overridden in tests
  // to prevent the delay.
  virtual void StartCacheUpdateTimerOnPrefThread(base::TimeDelta delay);

  // Update cached prefs in |http_server_properties_impl_| with data from
  // preferences. It gets the data on pref thread and calls
  // UpdateSpdyServersFromPrefsOnNetworkThread() to perform the update on
  // network thread.
  virtual void UpdateCacheFromPrefsOnPrefThread();

  // Starts the update of cached prefs in |http_server_properties_impl_| on the
  // network thread. Protected for testing.
  void UpdateCacheFromPrefsOnNetworkThread(
      std::vector<std::string>* spdy_servers,
      SpdySettingsMap* spdy_settings_map,
      AlternativeServiceMap* alternative_service_map,
      IPAddressNumber* last_quic_address,
      ServerNetworkStatsMap* server_network_stats_map,
      bool detected_corrupted_prefs);

  // These are used to delay updating the preferences when cached data in
  // |http_server_properties_impl_| is changing, and execute only one update per
  // simultaneous spdy_servers or spdy_settings or alternative_service changes.
  // |location| specifies where this method is called from. Virtual for testing.
  virtual void ScheduleUpdatePrefsOnNetworkThread(Location location);

  // Starts the timers to update the prefs from cache. This are overridden in
  // tests to prevent the delay.
  virtual void StartPrefsUpdateTimerOnNetworkThread(base::TimeDelta delay);

  // Update prefs::kHttpServerProperties in preferences with the cached data
  // from |http_server_properties_impl_|. This gets the data on network thread
  // and posts a task (UpdatePrefsOnPrefThread) to update preferences on pref
  // thread.
  void UpdatePrefsFromCacheOnNetworkThread();

  // Same as above, but fires an optional |completion| callback on pref thread
  // when finished. Virtual for testing.
  virtual void UpdatePrefsFromCacheOnNetworkThread(
      const base::Closure& completion);

  // Update prefs::kHttpServerProperties preferences on pref thread. Executes an
  // optional |completion| callback when finished. Protected for testing.
  void UpdatePrefsOnPrefThread(base::ListValue* spdy_server_list,
                               SpdySettingsMap* spdy_settings_map,
                               AlternativeServiceMap* alternative_service_map,
                               IPAddressNumber* last_quic_address,
                               ServerNetworkStatsMap* server_network_stats_map,
                               const base::Closure& completion);

 private:
  void OnHttpServerPropertiesChanged();

  bool ReadSupportsQuic(const base::DictionaryValue& server_dict,
                        IPAddressNumber* last_quic_address);
  void AddToSpdySettingsMap(const HostPortPair& server,
                            const base::DictionaryValue& server_dict,
                            SpdySettingsMap* spdy_settings_map);
  AlternativeServiceInfo ParseAlternativeServiceDict(
      const base::DictionaryValue& alternative_service_dict,
      const std::string& server_str);
  bool AddToAlternativeServiceMap(
      const HostPortPair& server,
      const base::DictionaryValue& server_dict,
      AlternativeServiceMap* alternative_service_map);
  bool AddToNetworkStatsMap(const HostPortPair& server,
                            const base::DictionaryValue& server_dict,
                            ServerNetworkStatsMap* network_stats_map);

  void SaveSpdySettingsToServerPrefs(const SettingsMap* spdy_settings_map,
                                     base::DictionaryValue* server_pref_dict);
  void SaveAlternativeServiceToServerPrefs(
      const AlternativeServiceInfoVector* alternative_service_info_vector,
      base::DictionaryValue* server_pref_dict);
  void SaveNetworkStatsToServerPrefs(
      const ServerNetworkStats* server_network_stats,
      base::DictionaryValue* server_pref_dict);

  void SaveSupportsQuicToPrefs(
      const IPAddressNumber* last_quic_address,
      base::DictionaryValue* http_server_properties_dict);

  // -----------
  // Pref thread
  // -----------

  const scoped_refptr<base::SequencedTaskRunner> pref_task_runner_;

  base::WeakPtr<HttpServerPropertiesManager> pref_weak_ptr_;

  // Used to post cache update tasks.
  scoped_ptr<base::OneShotTimer<HttpServerPropertiesManager> >
      pref_cache_update_timer_;

  // Used to track the spdy servers changes.
  PrefChangeRegistrar pref_change_registrar_;
  PrefService* pref_service_;  // Weak.
  bool setting_prefs_;
  const char* path_;

  // --------------
  // Network thread
  // --------------

  const scoped_refptr<base::SequencedTaskRunner> network_task_runner_;

  // Used to post |prefs::kHttpServerProperties| pref update tasks.
  scoped_ptr<base::OneShotTimer<HttpServerPropertiesManager> >
      network_prefs_update_timer_;

  scoped_ptr<HttpServerPropertiesImpl> http_server_properties_impl_;

  // Used to get |weak_ptr_| to self on the pref thread.
  scoped_ptr<base::WeakPtrFactory<HttpServerPropertiesManager> >
      pref_weak_ptr_factory_;

  // Used to get |weak_ptr_| to self on the network thread.
  scoped_ptr<base::WeakPtrFactory<HttpServerPropertiesManager> >
      network_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManager);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_
