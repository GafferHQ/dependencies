// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "net/base/net_export.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_server.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {

// Implementation of ProxyConfigService that retrieves the system proxy
// settings from environment variables, gconf, gsettings, or kioslaverc (KDE).
class NET_EXPORT_PRIVATE ProxyConfigServiceLinux : public ProxyConfigService {
 public:

  // Forward declaration of Delegate.
  class Delegate;

  class SettingGetter {
   public:
    // Buffer size used in some implementations of this class when reading
    // files. Defined here so unit tests can construct worst-case inputs.
    static const size_t BUFFER_SIZE = 512;

    SettingGetter() {}
    virtual ~SettingGetter() {}

    // Initializes the class: obtains a gconf/gsettings client, or simulates
    // one, in the concrete implementations. Returns true on success. Must be
    // called before using other methods, and should be called on the thread
    // running the glib main loop.
    // One of |glib_task_runner| and |file_task_runner| will be
    // used for gconf/gsettings calls or reading necessary files, depending on
    // the implementation.
    virtual bool Init(
        const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner,
        const scoped_refptr<base::SingleThreadTaskRunner>&
            file_task_runner) = 0;

    // Releases the gconf/gsettings client, which clears cached directories and
    // stops notifications.
    virtual void ShutDown() = 0;

    // Requests notification of gconf/gsettings changes for proxy
    // settings. Returns true on success.
    virtual bool SetUpNotifications(Delegate* delegate) = 0;

    // Returns the message loop for the thread on which this object
    // handles notifications, and also on which it must be destroyed.
    // Returns NULL if it does not matter.
    virtual const scoped_refptr<base::SingleThreadTaskRunner>&
        GetNotificationTaskRunner() = 0;

    // Returns the source of proxy settings.
    virtual ProxyConfigSource GetConfigSource() = 0;

    // These are all the values that can be fetched. We used to just use the
    // corresponding paths in gconf for these, but gconf is now obsolete and
    // in the future we'll be using mostly gsettings/kioslaverc so we
    // enumerate them instead to avoid unnecessary string operations.
    enum StringSetting {
      PROXY_MODE,
      PROXY_AUTOCONF_URL,
      PROXY_HTTP_HOST,
      PROXY_HTTPS_HOST,
      PROXY_FTP_HOST,
      PROXY_SOCKS_HOST,
    };
    enum BoolSetting {
      PROXY_USE_HTTP_PROXY,
      PROXY_USE_SAME_PROXY,
      PROXY_USE_AUTHENTICATION,
    };
    enum IntSetting {
      PROXY_HTTP_PORT,
      PROXY_HTTPS_PORT,
      PROXY_FTP_PORT,
      PROXY_SOCKS_PORT,
    };
    enum StringListSetting {
      PROXY_IGNORE_HOSTS,
    };

    // Given a PROXY_*_HOST value, return the corresponding PROXY_*_PORT value.
    static IntSetting HostSettingToPortSetting(StringSetting host) {
      switch (host) {
        case PROXY_HTTP_HOST:
          return PROXY_HTTP_PORT;
        case PROXY_HTTPS_HOST:
          return PROXY_HTTPS_PORT;
        case PROXY_FTP_HOST:
          return PROXY_FTP_PORT;
        case PROXY_SOCKS_HOST:
          return PROXY_SOCKS_PORT;
        default:
          NOTREACHED();
          return PROXY_HTTP_PORT;  // Placate compiler.
      }
    }

    // Gets a string type value from the data source and stores it in
    // |*result|. Returns false if the key is unset or on error. Must only be
    // called after a successful call to Init(), and not after a failed call
    // to SetUpNotifications() or after calling Release().
    virtual bool GetString(StringSetting key, std::string* result) = 0;
    // Same thing for a bool typed value.
    virtual bool GetBool(BoolSetting key, bool* result) = 0;
    // Same for an int typed value.
    virtual bool GetInt(IntSetting key, int* result) = 0;
    // And for a string list.
    virtual bool GetStringList(StringListSetting key,
                               std::vector<std::string>* result) = 0;

    // Returns true if the bypass list should be interpreted as a proxy
    // whitelist rather than blacklist. (This is KDE-specific.)
    virtual bool BypassListIsReversed() = 0;

    // Returns true if the bypass rules should be interpreted as
    // suffix-matching rules.
    virtual bool MatchHostsUsingSuffixMatching() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(SettingGetter);
  };

  // ProxyConfigServiceLinux is created on the UI thread, and
  // SetUpAndFetchInitialConfig() is immediately called to synchronously
  // fetch the original configuration and set up change notifications on
  // the UI thread.
  //
  // Past that point, it is accessed periodically through the
  // ProxyConfigService interface (GetLatestProxyConfig, AddObserver,
  // RemoveObserver) from the IO thread.
  //
  // Setting change notification callbacks can occur at any time and are
  // run on either the UI thread (gconf/gsettings) or the file thread
  // (KDE). The new settings are fetched on that thread, and the resulting
  // proxy config is posted to the IO thread through
  // Delegate::SetNewProxyConfig(). We then notify observers on the IO
  // thread of the configuration change.
  //
  // ProxyConfigServiceLinux is deleted from the IO thread.
  //
  // The substance of the ProxyConfigServiceLinux implementation is
  // wrapped in the Delegate ref counted class. On deleting the
  // ProxyConfigServiceLinux, Delegate::OnDestroy() is posted to either
  // the UI thread (gconf/gsettings) or the file thread (KDE) where change
  // notifications will be safely stopped before releasing Delegate.

  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    // Constructor receives env var getter implementation to use, and
    // takes ownership of it. This is the normal constructor.
    explicit Delegate(base::Environment* env_var_getter);
    // Constructor receives setting and env var getter implementations
    // to use, and takes ownership of them. Used for testing.
    Delegate(base::Environment* env_var_getter, SettingGetter* setting_getter);

    // Synchronously obtains the proxy configuration. If gconf,
    // gsettings, or kioslaverc are used, also enables notifications for
    // setting changes. gconf/gsettings must only be accessed from the
    // thread running the default glib main loop, and so this method
    // must be called from the UI thread. The message loop for the IO
    // thread is specified so that notifications can post tasks to it
    // (and for assertions). The message loop for the file thread is
    // used to read any files needed to determine proxy settings.
    void SetUpAndFetchInitialConfig(
        const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner,
        const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
        const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner);

    // Handler for setting change notifications: fetches a new proxy
    // configuration from settings, and if this config is different
    // than what we had before, posts a task to have it stored in
    // cached_config_.
    // Left public for simplicity.
    void OnCheckProxyConfigSettings();

    // Called from IO thread.
    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);
    ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
        ProxyConfig* config);

    // Posts a call to OnDestroy() to the UI or FILE thread, depending on the
    // setting getter in use. Called from ProxyConfigServiceLinux's destructor.
    void PostDestroyTask();
    // Safely stops change notifications. Posted to either the UI or FILE
    // thread, depending on the setting getter in use.
    void OnDestroy();

   private:
    friend class base::RefCountedThreadSafe<Delegate>;

    ~Delegate();

    // Obtains an environment variable's value. Parses a proxy server
    // specification from it and puts it in result. Returns true if the
    // requested variable is defined and the value valid.
    bool GetProxyFromEnvVarForScheme(const char* variable,
                                     ProxyServer::Scheme scheme,
                                     ProxyServer* result_server);
    // As above but with scheme set to HTTP, for convenience.
    bool GetProxyFromEnvVar(const char* variable, ProxyServer* result_server);
    // Fills proxy config from environment variables. Returns true if
    // variables were found and the configuration is valid.
    bool GetConfigFromEnv(ProxyConfig* config);

    // Obtains host and port config settings and parses a proxy server
    // specification from it and puts it in result. Returns true if the
    // requested variable is defined and the value valid.
    bool GetProxyFromSettings(SettingGetter::StringSetting host_key,
                              ProxyServer* result_server);
    // Fills proxy config from settings. Returns true if settings were found
    // and the configuration is valid.
    bool GetConfigFromSettings(ProxyConfig* config);

    // This method is posted from the UI thread to the IO thread to
    // carry the new config information.
    void SetNewProxyConfig(const ProxyConfig& new_config);

    // This method is run on the getter's notification thread.
    void SetUpNotifications();

    scoped_ptr<base::Environment> env_var_getter_;
    scoped_ptr<SettingGetter> setting_getter_;

    // Cached proxy configuration, to be returned by
    // GetLatestProxyConfig. Initially populated from the UI thread, but
    // afterwards only accessed from the IO thread.
    ProxyConfig cached_config_;

    // A copy kept on the UI thread of the last seen proxy config, so as
    // to avoid posting a call to SetNewProxyConfig when we get a
    // notification but the config has not actually changed.
    ProxyConfig reference_config_;

    // The task runner for the glib thread, aka main browser thread. This thread
    // is where we run the glib main loop (see
    // base/message_loop/message_pump_glib.h). It is the glib default loop in
    // the sense that it runs the glib default context: as in the context where
    // sources are added by g_timeout_add and g_idle_add, and returned by
    // g_main_context_default. gconf uses glib timeouts and idles and possibly
    // other callbacks that will all be dispatched on this thread. Since gconf
    // is not thread safe, any use of gconf must be done on the thread running
    // this loop.
    scoped_refptr<base::SingleThreadTaskRunner> glib_task_runner_;
    // Task runner for the IO thread. GetLatestProxyConfig() is called from
    // the thread running this loop.
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

    base::ObserverList<Observer> observers_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Thin wrapper shell around Delegate.

  // Usual constructor
  ProxyConfigServiceLinux();
  // For testing: take alternate setting and env var getter implementations.
  explicit ProxyConfigServiceLinux(base::Environment* env_var_getter);
  ProxyConfigServiceLinux(base::Environment* env_var_getter,
                          SettingGetter* setting_getter);

  ~ProxyConfigServiceLinux() override;

  void SetupAndFetchInitialConfig(
      const scoped_refptr<base::SingleThreadTaskRunner>& glib_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner) {
    delegate_->SetUpAndFetchInitialConfig(glib_task_runner,
                                          io_task_runner,
                                          file_task_runner);
  }
  void OnCheckProxyConfigSettings() {
    delegate_->OnCheckProxyConfigSettings();
  }

  // ProxyConfigService methods:
  // Called from IO thread.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      ProxyConfig* config) override;

 private:
  scoped_refptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceLinux);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_
