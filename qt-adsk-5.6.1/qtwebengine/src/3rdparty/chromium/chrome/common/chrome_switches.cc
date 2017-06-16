// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace switches {

// -----------------------------------------------------------------------------
// Can't find the switch you are looking for? Try looking in:
// ash/ash_switches.cc
// base/base_switches.cc
// chromeos/chromeos_switches.cc
// etc.
//
// When commenting your switch, please use the same voice as surrounding
// comments. Imagine "This switch..." at the beginning of the phrase, and it'll
// all work out.
// -----------------------------------------------------------------------------

// Allows third-party content included on a page to prompt for a HTTP basic
// auth username/password pair.
const char kAllowCrossOriginAuthPrompt[]    = "allow-cross-origin-auth-prompt";

// On ChromeOS, file:// access is disabled except for certain whitelisted
// directories. This switch re-enables file:// for testing.
const char kAllowFileAccess[]               = "allow-file-access";

// Allow non-secure origins to use the screen capture API and the desktopCapture
// extension API.
const char kAllowHttpScreenCapture[] = "allow-http-screen-capture";

// Enables TLS/SSL errors on localhost to be ignored (no interstitial,
// no blocking of requests).
const char kAllowInsecureLocalhost[] = "allow-insecure-localhost";

#if defined(ENABLE_PLUGINS)
// Specifies comma-separated list of extension ids or hosts to grant
// access to CRX file system APIs.
const char kAllowNaClCrxFsAPI[]             = "allow-nacl-crxfs-api";

// Specifies comma-separated list of extension ids or hosts to grant
// access to file handle APIs.
const char kAllowNaClFileHandleAPI[]        = "allow-nacl-file-handle-api";

// Specifies comma-separated list of extension ids or hosts to grant
// access to TCP/UDP socket APIs.
const char kAllowNaClSocketAPI[]            = "allow-nacl-socket-api";

// Disables forcing on the experimental Plugin Power Saver feature.
const char kDisablePluginPowerSaver[] = "disable-plugin-power-saver";

// Enables forcing on the experimental Plugin Power Saver feature.
const char kEnablePluginPowerSaver[] = "enable-plugin-power-saver";
#endif

// Don't block outdated plugins.
const char kAllowOutdatedPlugins[]          = "allow-outdated-plugins";

// By default, an https page cannot run JavaScript, CSS or plugins from http
// URLs. This provides an override to get the old insecure behavior.
const char kAllowRunningInsecureContent[]   = "allow-running-insecure-content";

// Specifies the probability threshold for alternative services: an advertised
// alternative service will only be honored if the advertised probability is
// greater than or equal to this threshold.
const char kAlternativeServiceProbabilityThreshold[] =
    "alternative-service-probability-threshold";

// Prevents Chrome from requiring authorization to run certain widely installed
// but less commonly used plugins.
const char kAlwaysAuthorizePlugins[]        = "always-authorize-plugins";

// Specifies that the extension-app with the specified id should be launched
// according to its configuration.
const char kAppId[]                         = "app-id";

// Specifies that the associated value should be launched in "application"
// mode.
const char kApp[]                           = "app";

// Overrides the apps checkout URL, which is used to determine when to expose
// some private APIs.
const char kAppsCheckoutURL[]               = "apps-checkout-url";

// The URL that the webstore APIs download extensions from.
// Note: the URL must contain one '%s' for the extension ID.
const char kAppsGalleryDownloadURL[]        = "apps-gallery-download-url";

// The URL to use for the gallery link in the app launcher.
const char kAppsGalleryURL[]                = "apps-gallery-url";

// The update url used by gallery/webstore extensions.
const char kAppsGalleryUpdateURL[]          = "apps-gallery-update-url";

// Value of GAIA auth code for --force-app-mode.
const char kAppModeAuthCode[]               = "app-mode-auth-code";

// Value of OAuth2 refresh token for --force-app-mode.
const char kAppModeOAuth2Token[]            = "app-mode-oauth-token";

// Enables overriding the path for the default authentication extension.
const char kAuthExtensionPath[]             = "auth-ext-path";

// A flag that is used to tell Chrome that it was launched automatically at
// computer startup and not by some user action.
const char kAutoLaunchAtStartup[]           = "auto-launch-at-startup";

// This flag makes Chrome auto-select the provided choice when an extension asks
// permission to start desktop capture. Should only be used for tests. For
// instance, --auto-select-desktop-capture-source="Entire screen" will
// automatically select to share the entire screen in English locales.
const char kAutoSelectDesktopCaptureSource[] =
    "auto-select-desktop-capture-source";

// This flag causes the user engagement checks for showing app banners to be
// bypassed. It is intended to be used by developers who wish to test that their
// sites otherwise meet the criteria needed to show app banners.
const char kBypassAppBannerEngagementChecks[] =
    "bypass-app-banner-engagement-checks";

// Certificate Transparency: Uses the provided log(s) for checking Signed
// Certificate Timestamps provided with certificates.
// The switch's value is:
//   log_description:log_key:log_url,log_description:log_key:log_url,...
// where
//   log_description is a textual description of the log.
//   log_key is a Base64'd DER-encoded SubjectPublicKeyInfo of the log's
//   public key.
//   log_url is a URL for the log, excluding the schema (which is always
//   assumed to be HTTPS as required by RFC6962).
// Multiple logs can be specified by repeating description:key pairs,
// separated by a comma.
const char kCertificateTransparencyLog[] =
    "certificate-transparency-log";

// How often (in seconds) to check for updates. Should only be used for testing
// purposes.
const char kCheckForUpdateIntervalSec[]     = "check-for-update-interval";

// Comma-separated list of SSL cipher suites to disable.
const char kCipherSuiteBlacklist[]          = "cipher-suite-blacklist";

// Tells chrome to display the cloud print dialog and upload the specified file
// for printing.
const char kCloudPrintFile[]                = "cloud-print-file";

// Specifies the mime type to be used when uploading data from the file
// referenced by cloud-print-file. Defaults to "application/pdf" if
// unspecified.
const char kCloudPrintFileType[]            = "cloud-print-file-type";

// Used with kCloudPrintFile to specify a JSON print ticket for the resulting
// print job. Defaults to null if unspecified.
const char kCloudPrintPrintTicket[]         = "cloud-print-print-ticket";

// Used with kCloudPrintFile to specify a title for the resulting print job.
const char kCloudPrintJobTitle[]            = "cloud-print-job-title";

// Setup cloud print proxy for provided printers. This does not start
// service or register proxy for autostart.
const char kCloudPrintSetupProxy[]          = "cloud-print-setup-proxy";

// Comma-separated list of BrowserThreads that cause browser process to crash
// if the given browser thread is not responsive. UI,IO,DB,FILE,CACHE are the
// list of BrowserThreads that are supported.
//
// For example:
//    --crash-on-hang-threads=UI:3:18,IO:3:18 --> Crash the browser if UI or IO
//      is not responsive for 18 seconds and the number of browser threads that
//      are responding is less than or equal to 3.
const char kCrashOnHangThreads[]            = "crash-on-hang-threads";

// Some platforms like ChromeOS default to empty desktop.
// Browser tests may need to add this switch so that at least one browser
// instance is created on startup.
// TODO(nkostylev): Investigate if this switch could be removed.
// (http://crbug.com/148675)
const char kCreateBrowserOnStartupForTests[] =
    "create-browser-on-startup-for-tests";

// Enables a frame context menu item that toggles the frame in and out of glass
// mode (Windows Vista and up only).
const char kDebugEnableFrameToggle[]        = "debug-enable-frame-toggle";

// Adds debugging entries such as Inspect Element to context menus of packed
// apps.
const char kDebugPackedApps[]               = "debug-packed-apps";

// Triggers a plethora of diagnostic modes.
const char kDiagnostics[]                   = "diagnostics";

// Sets the output format for diagnostic modes enabled by diagnostics flag.
const char kDiagnosticsFormat[]             = "diagnostics-format";

// Tells the diagnostics mode to do the requested recovery step(s).
const char kDiagnosticsRecovery[]           = "diagnostics-recovery";

// When kEnableSettingsWindow is used, About is shown as an overlay in Settings
// instead of as a separate page, unless this flag is specified.
const char kDisableAboutInSettings[]        = "disable-about-in-settings";

// Disables the display of a banner allowing the user to add a web
// app to their shelf (or platform-specific equivalent)
const char kDisableAddToShelf[] = "disable-add-to-shelf";

// Disables the experimental asynchronous DNS client.
const char kDisableAsyncDns[]               = "disable-async-dns";

// Disable several subsystems which run network requests in the background.
// This is for use when doing network performance testing to avoid noise in the
// measurements.
const char kDisableBackgroundNetworking[]   = "disable-background-networking";

// Disables the bundled PPAPI version of Flash.
const char kDisableBundledPpapiFlash[]      = "disable-bundled-ppapi-flash";

// Disable hardware encoding support for Cast Streaming.
const char kDisableCastStreamingHWEncoding[] =
    "disable-cast-streaming-hw-encoding";

// Disables detection of child accounts.
const char kDisableChildAccountDetection[] =
    "disable-child-account-detection";

// Disables the client-side phishing detection feature. Note that even if
// client-side phishing detection is enabled, it will only be active if the
// user has opted in to UMA stats and SafeBrowsing is enabled in the
// preferences.
const char kDisableClientSidePhishingDetection[] =
    "disable-client-side-phishing-detection";

// Disable default component extensions with background pages - useful for
// performance tests where these pages may interfere with perf results.
const char kDisableComponentExtensionsWithBackgroundPages[] =
    "disable-component-extensions-with-background-pages";

const char kDisableComponentUpdate[]        = "disable-component-update";

// Disables installation of default apps on first run. This is used during
// automated testing.
const char kDisableDefaultApps[]            = "disable-default-apps";

// Disables device discovery notifications.
const char kDisableDeviceDiscoveryNotifications[] =
    "disable-device-discovery-notifications";

// Disables the dinosaur easter egg on the offline interstitial.
const char kDisableDinosaurEasterEgg[] = "disable-dinosaur-easter-egg";

// Disables Domain Reliability Monitoring.
const char kDisableDomainReliability[]      = "disable-domain-reliability";

// Disable extensions.
const char kDisableExtensions[]             = "disable-extensions";

// Disable checking for user opt-in for extensions that want to inject script
// into file URLs (ie, always allow it). This is used during automated testing.
const char kDisableExtensionsFileAccessCheck[] =
    "disable-extensions-file-access-check";

// Disable the net::URLRequestThrottlerManager functionality for
// requests originating from extensions.
const char kDisableExtensionsHttpThrottling[] =
    "disable-extensions-http-throttling";

// Disable the behavior that the second click on a launcher item (the click when
// the item is already active) minimizes the item.
const char kDisableMinimizeOnSecondLauncherItemClick[] =
    "disable-minimize-on-second-launcher-item-click";

// Disables the new bookmark app system.
const char kDisableNewBookmarkApps[]        = "disable-new-bookmark-apps";

// Disables the new offline error page generated by NetErrorHelper for ChromeOS
// and instead uses the old error page generated by OfflineResourceThrottle.
const char kDisableNewOfflineErrorPage[] = "disable-new-offline-error-page";

// Disable auto-reload of error pages if offline.
const char kDisableOfflineAutoReload[]       = "disable-offline-auto-reload";

// Disable only auto-reloading error pages when the tab is visible.
const char kDisableOfflineAutoReloadVisibleOnly[] =
    "disable-offline-auto-reload-visible-only";

// Disable out-of-process V8 proxy resolver.
const char kDisableOutOfProcessPac[] = "disable-out-of-process-pac";

// Disable the setting to prompt the user for their OS account password before
// revealing plaintext passwords in the password manager.
const char kDisablePasswordManagerReauthentication[] =
    "disable-password-manager-reauthentication";

// Disable the new material UI - requires out of process PDF plugin.
const char kDisablePdfMaterialUI[]          = "disable-pdf-material-ui";

// Don't use bubbles for content permissions requests instead of infobars.
const char kDisablePermissionsBubbles[]      = "disable-permissions-bubbles";

// Disable pop-up blocking.
const char kDisablePopupBlocking[]          = "disable-popup-blocking";

// Disable speculative TCP/IP preconnection.
const char kDisablePreconnect[]             = "disable-preconnect";

// Disables print preview (For testing, and for users who don't like us. :[ )
const char kDisablePrintPreview[]           = "disable-print-preview";

// Normally when the user attempts to navigate to a page that was the result of
// a post we prompt to make sure they want to. This switch may be used to
// disable that check. This switch is used during automated testing.
const char kDisablePromptOnRepost[]         = "disable-prompt-on-repost";

// Disables support for the QUIC protocol.
const char kDisableQuic[]                   = "disable-quic";

// Disable use of Chromium's port selection for the ephemeral port via bind().
// This only has an effect if QUIC protocol is enabled.
const char kDisableQuicPortSelection[]      = "disable-quic-port-selection";

// Prevents the save password bubble from being enabled.
const char kDisableSavePasswordBubble[] = "disable-save-password-bubble";

// Prevents SDCH persistence from being used.
const char kDisableSdchPersistence[] = "disable-sdch-persistence";

// Disables using bubbles for session restore request.
const char kDisableSessionCrashedBubble[] = "disable-session-crashed-bubble";

// Disables the Site Engagement service, which records interaction with sites
// and allocates certain resources accordingly.
const char kDisableSiteEngagementService[] = "disable-site-engagement-service";

// Disables the suggestions service.
const char kDisableSuggestionsService[]     = "disable-suggestions-service";

// Disables syncing browser data to a Google Account.
const char kDisableSync[]                   = "disable-sync";

// Disables syncing one or more sync data types that are on by default.
// See sync/internal_api/public/base/model_type.h for possible types. Types
// should be comma separated, and follow the naming convention for string
// representation of model types, e.g.:
// --disable-synctypes='Typed URLs, Bookmarks, Autofill Profiles'
const char kDisableSyncTypes[]              = "disable-sync-types";

// Disables the backend service for web resources.
const char kDisableWebResources[]           = "disable-web-resources";

// Some tests seem to require the application to close when the last
// browser window is closed. Thus, we need a switch to force this behavior
// for ChromeOS Aura, disable "zero window mode".
// TODO(pkotwicz): Investigate if this bug can be removed.
// (http://crbug.com/119175)
const char kDisableZeroBrowsersOpenForTests[] =
    "disable-zero-browsers-open-for-tests";

// Use a specific disk cache location, rather than one derived from the
// UserDatadir.
const char kDiskCacheDir[]                  = "disk-cache-dir";

// Forces the maximum disk space to be used by the disk cache, in bytes.
const char kDiskCacheSize[]                 = "disk-cache-size";

const char kDnsLogDetails[]                 = "dns-log-details";

// Disables prefetching of DNS information.
const char kDnsPrefetchDisable[]            = "dns-prefetch-disable";

// Requests that a running browser process dump its collected histograms to a
// given file. The file is overwritten if it exists.
const char kDumpBrowserHistograms[]         = "dump-browser-histograms";

// Overrides the path of Easy Unlock component app.
const char kEasyUnlockAppPath[]             = "easy-unlock-app-path";

// Enables the display of a banner allowing the user to add a web
// app to their shelf (or platform-specific equivalent)
const char kEnableAddToShelf[] = "enable-add-to-shelf";

// Enable OS integration for Chrome app file associations.
const char kEnableAppsFileAssociations[]    = "enable-apps-file-associations";

// Enables the benchmarking extensions.
const char kEnableBenchmarking[]            = "enable-benchmarking";

// Enables the multi-level undo system for bookmarks.
const char kEnableBookmarkUndo[]            = "enable-bookmark-undo";

// Enables detection of child accounts.
const char kEnableChildAccountDetection[] =
    "enable-child-account-detection";

// This applies only when the process type is "service". Enables the Cloud
// Print Proxy component within the service process.
const char kEnableCloudPrintProxy[]         = "enable-cloud-print-proxy";

// If true devtools experimental settings are enabled.
const char kEnableDevToolsExperiments[]     = "enable-devtools-experiments";

// Enable device discovery notifications.
const char kEnableDeviceDiscoveryNotifications[] =
    "enable-device-discovery-notifications";

// Enables the DOM distiller.
const char kEnableDomDistiller[] = "enable-dom-distiller";

// Enables Domain Reliability Monitoring.
const char kEnableDomainReliability[] = "enable-domain-reliability";

// Enables Download Notification.
const char kEnableDownloadNotification[] = "enable-download-notification";

// Enable Enhanced Bookmarks.
const char kEnhancedBookmarksExperiment[] = "enhanced-bookmarks-experiment";

// Enables experimentation with ephemeral apps to be launched from the webstore.
const char kEnableEphemeralAppsInWebstore[] =
    "enable-ephemeral-apps-in-webstore";

// Enables experimental hotword features specific to always-on.
const char kEnableExperimentalHotwordHardware[] = "enable-hotword-hardware";

// Enables logging for extension activity.
const char kEnableExtensionActivityLogging[] =
    "enable-extension-activity-logging";

const char kEnableExtensionActivityLogTesting[] =
    "enable-extension-activity-log-testing";

// Enable the fast unload controller, which speeds up tab/window close by
// running a tab's onunload js handler independently of the GUI -
// crbug.com/142458 .
const char kEnableFastUnload[]         = "enable-fast-unload";

// Enables experimentation with launching ephemeral apps via hyperlinks.
const char kEnableLinkableEphemeralApps[]   = "enable-linkable-ephemeral-apps";

// Enables an experimental Material Design version of chrome://downloads.
const char kEnableMaterialDesignDownloads[] = "enable-md-downloads";

// Enables the material design Settings feature.
const char kEnableMaterialDesignSettings[]  = "enable-md-settings";

#if defined(ENABLE_MEDIA_ROUTER)
// Enables Media Router.
const char kEnableMediaRouter[]             = "enable-media-router";
#endif

// Runs the Native Client inside the renderer process and enables GPU plugin
// (internally adds lEnableGpuPlugin to the command line).
const char kEnableNaCl[]                    = "enable-nacl";

// Enables tracing for each navigation. It will attempt to trace each navigation
// for 10s, until the buffer is full, or until the next navigation.
// It only works if a URL was provided by --trace-upload-url.
const char kEnableNavigationTracing[] = "enable-navigation-tracing";

// Enables the network-related benchmarking extensions.
const char kEnableNetBenchmarking[]         = "enable-net-benchmarking";

// Enables the new bookmark app system.
const char kEnableNewBookmarkApps[]         = "enable-new-bookmark-apps";

// Enable auto-reload of error pages if offline.
const char kEnableOfflineAutoReload[]       = "enable-offline-auto-reload";

// Only auto-reload error pages when the tab is visible.
const char kEnableOfflineAutoReloadVisibleOnly[] =
    "enable-offline-auto-reload-visible-only";

// Enables panels (always on-top docked pop-up windows).
const char kEnablePanels[]                  = "enable-panels";

// Enable the new material UI - requires out of process PDF plugin.
const char kEnablePdfMaterialUI[]           = "enable-pdf-material-ui";

// Enables presenting plugin placeholder content as shadow DOM.
const char kEnablePluginPlaceholderShadowDom[] =
    "enable-plugin-placeholder-shadow-dom";

// Enables a number of potentially annoying security features (strict mixed
// content mode, powerful feature restrictions, etc.)
const char kEnablePotentiallyAnnoyingSecurityFeatures[] =
    "enable-potentially-annoying-security-features";

// Enables the Power overlay in Settings.
const char kEnablePowerOverlay[]            = "enable-power-overlay";

// Enables showing unregistered printers in print preview
const char kEnablePrintPreviewRegisterPromos[] =
    "enable-print-preview-register-promos";

// Enables tracking of tasks in profiler for viewing via about:profiler.
// To predominantly disable tracking (profiling), use the command line switch:
// --enable-profiling=0
// Some tracking will still take place at startup, but it will be turned off
// during chrome_browser_main.
const char kEnableProfiling[]               = "enable-profiling";

// Enables query in the omnibox.
const char kEnableQueryExtraction[]         = "enable-query-extraction";

// Enables support for the QUIC protocol.  This is a temporary testing flag.
const char kEnableQuic[]                    = "enable-quic";

// Enable use of Chromium's port selection for the ephemeral port via bind().
// This only has an effect if QUIC protocol is enabled.
const char kEnableQuicPortSelection[]       = "enable-quic-port-selection";

// Enables save password prompt bubble.
const char kEnableSavePasswordBubble[]      = "enable-save-password-bubble";

// Enables SDCH persistence.
const char kEnableSdchPersistence[] = "enable-sdch-persistence";

// Enables using bubbles for session restore request instead of infobars.
const char kEnableSessionCrashedBubble[] = "enable-session-crashed-bubble";

// Enable or disable settings in a separate browser window per profile
// (see SettingsWindowEnabled() below).
const char kEnableSettingsWindow[]           = "enable-settings-window";
const char kDisableSettingsWindow[]          = "disable-settings-window";

// Enable the Site Engagement service, which records interaction with sites and
// allocates certain resources accordingly.
const char kEnableSiteEngagementService[]   = "enable-site-engagement-service";

// Enables the suggestions service.
const char kEnableSuggestionsService[]      = "enable-suggestions-service";

// Enables the supervised user managed bookmarks folder.
const char kEnableSupervisedUserManagedBookmarksFolder[] =
    "enable-supervised-user-managed-bookmarks-folder";

// Enables synced articles.
const char kEnableSyncArticles[]            = "enable-sync-articles";

// Enables user control over muting tab audio from the tab strip.
const char kEnableTabAudioMuting[]  = "enable-tab-audio-muting";

// Enables fanciful thumbnail processing. Used with NTP for
// instant-extended-api, where thumbnails are generally smaller.
const char kEnableThumbnailRetargeting[]   = "enable-thumbnail-retargeting";

// Enables Alternate-Protocol when the port is user controlled (> 1024).
const char kEnableUserAlternateProtocolPorts[] =
    "enable-user-controlled-alternate-protocol-ports";

// Enables a new "web app" style frame for hosted apps (including bookmark
// apps).
extern const char kEnableWebAppFrame[] = "enable-web-app-frame";

// Enables synchronizing WiFi credentials across devices, using Chrome Sync.
const char kEnableWifiCredentialSync[]      = "enable-wifi-credential-sync";

// Values for the kExtensionContentVerification flag.
// See ContentVerifierDelegate::Mode for more explanation.
const char kExtensionContentVerificationBootstrap[] = "bootstrap";
const char kExtensionContentVerificationEnforceStrict[] = "enforce_strict";
const char kExtensionContentVerificationEnforce[] = "enforce";

// Name of the command line flag to force content verification to be on in one
// of various modes.
const char kExtensionContentVerification[] = "extension-content-verification";

// Turns on extension install verification if it would not otherwise have been
// turned on.
const char kExtensionsInstallVerification[] = "extensions-install-verification";

// Specifies a comma-separated list of extension ids that should be forced to
// be treated as not from the webstore when doing install verification.
const char kExtensionsNotWebstore[] = "extensions-not-webstore";

// Frequency in seconds for Extensions auto-update.
const char kExtensionsUpdateFrequency[]     = "extensions-update-frequency";

// Fakes the channel of the browser for purposes of Variations filtering. This
// is to be used for testing only. Possible values are "stable", "beta", "dev"
// and "canary". Note that this only applies if the browser's reported channel
// is UNKNOWN.
const char kFakeVariationsChannel[]         = "fake-variations-channel";

// If this flag is present then this command line is being delegated to an
// already running chrome process via the fast path, ie: before chrome.dll is
// loaded. It is useful to tell the difference for tracking purposes.
const char kFastStart[]            = "fast-start";

// These two flags are added around the switches about:flags adds to the
// command line. This is useful to see which switches were added by about:flags
// on about:version. They don't have any effect.
const char kFlagSwitchesBegin[]             = "flag-switches-begin";
const char kFlagSwitchesEnd[]               = "flag-switches-end";

// Forces application mode. This hides certain system UI elements and forces
// the app to be installed if it hasn't been already.
const char kForceAppMode[]                  = "force-app-mode";

// This option can be used to force parameters of field trials when testing
// changes locally. The argument is a list of key/value pairs prefixed by
// Trial/Group pair. The following shows setting parameters to 2 experiments
// where in the first, it forces "id" to be "foo" for the "Enabled" group of
// the "EnhancedBookmarks" trial:
// "EnhancedBookmarks.Enabled:id/foo,Experiment2.Group1:key1/value1"
// Trial names, groups names, parameter names, and value should all be URL
// escaped for all non-alphanumeric characters.
const char kForceFieldTrialParams[] = "force-fieldtrial-params";

// Displays the First Run experience when the browser is started, regardless of
// whether or not it's actually the First Run (this overrides kNoFirstRun).
const char kForceFirstRun[]                 = "force-first-run";

// Forces additional Chrome Variation Ids that will be sent in X-Client-Data
// header, specified as a 64-bit encoded list of numeric experiment ids. Ids
// prefixed with the character "t" will be treated as Trigger Variation Ids.
const char kForceVariationIds[]             = "force-variation-ids";

// Enables grouping websites by domain and filtering them by period.
const char kHistoryEnableGroupByDomain[]    = "enable-grouped-history";

// Specifies which page will be displayed in newly-opened tabs. We need this
// for testing purposes so that the UI tests don't depend on what comes up for
// http://google.com.
const char kHomePage[]                      = "homepage";

// Comma-separated list of rules that control how hostnames are mapped.
//
// For example:
//    "MAP * 127.0.0.1" --> Forces all hostnames to be mapped to 127.0.0.1
//    "MAP *.google.com proxy" --> Forces all google.com subdomains to be
//                                 resolved to "proxy".
//    "MAP test.com [::1]:77 --> Forces "test.com" to resolve to IPv6 loopback.
//                               Will also force the port of the resulting
//                               socket address to be 77.
//    "MAP * baz, EXCLUDE www.google.com" --> Remaps everything to "baz",
//                                            except for "www.google.com".
//
// These mappings apply to the endpoint host in a net::URLRequest (the TCP
// connect and host resolver in a direct connection, and the CONNECT in an http
// proxy connection, and the endpoint host in a SOCKS proxy connection).
const char kHostRules[]                     = "host-rules";

// The maximum number of retry attempts to resolve the host. Set this to zero
// to disable host resolver retry attempts.
const char kHostResolverRetryAttempts[]     = "host-resolver-retry-attempts";

// Causes net::URLFetchers to ignore requests for SSL client certificates,
// causing them to attempt an unauthenticated SSL/TLS session. This is intended
// for use when testing various service URLs (eg: kPromoServerURL, kSbURLPrefix,
// kSyncServiceURL, etc)
const char kIgnoreUrlFetcherCertRequests[]  =
    "ignore-urlfetcher-cert-requests";

// Causes the browser to launch directly in incognito mode.
const char kIncognito[]                     = "incognito";

// Causes Chrome to initiate an installation flow for the given app.
const char kInstallChromeApp[]              = "install-chrome-app";

// Causes Chrome to attempt to get metadata from the webstore for the
// app/extension ID given, and then prompt the user to download and install it.
// This is allowed *only* for ephemeral apps. All other ids will be ignored.
const char kInstallEphemeralAppFromWebstore[] =
    "install-ephemeral-app-from-webstore";

// A list of whitelists to install for a supervised user, for testing.
// The list is of the following form: <id>[:<name>],[<id>[:<name>],...]
const char kInstallSupervisedUserWhitelists[] =
    "install-supervised-user-whitelists";

// Marks a renderer as an Instant process.
const char kInstantProcess[]                = "instant-process";

// Invalidation service should use GCM network channel even if experiment is not
// enabled.
const char kInvalidationUseGCMChannel[]     = "invalidation-use-gcm-channel";

// Disable latest shipping ECMAScript 6 features.
const char kDisableJavaScriptHarmonyShipping[] =
    "disable-javascript-harmony-shipping";

// Enables experimental Harmony (ECMAScript 6) features.
const char kJavaScriptHarmony[]             = "javascript-harmony";

// Dumps IPC messages sent from renderer processes to the browser process to
// the given directory. Used primarily to gather samples for IPC fuzzing.
const char kIpcDumpDirectory[]              = "ipc-dump-directory";

// Used for testing - keeps browser alive after last browser window closes.
const char kKeepAliveForTest[]              = "keep-alive-for-test";

// Enable Kiosk mode.
const char kKioskMode[]                     = "kiosk";

// Print automatically in kiosk mode. |kKioskMode| must be set as well.
// See http://crbug.com/31395.
const char kKioskModePrinting[]             = "kiosk-printing";

// Comma-separated list of directories with component extensions to load.
const char kLoadComponentExtension[]        = "load-component-extension";

// Loads an extension from the specified directory.
const char kLoadExtension[]                 = "load-extension";

// Makes Chrome default browser
const char kMakeDefaultBrowser[]            = "make-default-browser";

// Use to opt-in to marking HTTP as non-secure.
const char kMarkNonSecureAs[]               = "mark-non-secure-as";
const char kMarkNonSecureAsNeutral[]        = "neutral";
const char kMarkNonSecureAsDubious[]        = "dubious";
const char kMarkNonSecureAsNonSecure[]      = "non-secure";

// Forces the maximum disk space to be used by the media cache, in bytes.
const char kMediaCacheSize[]                = "media-cache-size";

// Enables histograming of tasks served by MessageLoop. See
// about:histograms/Loop for results, which show frequency of messages on each
// thread, including APC count, object signalling count, etc.
const char kMessageLoopHistogrammer[]       = "message-loop-histogrammer";

// Enables the recording of metrics reports but disables reporting. In contrast
// to kDisableMetrics, this executes all the code that a normal client would
// use for reporting, except the report is dropped rather than sent to the
// server. This is useful for finding issues in the metrics code during UI and
// performance tests.
const char kMetricsRecordingOnly[]          = "metrics-recording-only";

// Allows setting a different destination ID for connection-monitoring GCM
// messages. Useful when running against a non-prod management server.
const char kMonitoringDestinationID[]       = "monitoring-destination-id";

// Sets the granularity of events to capture in the network log. The mode can be
// set to one of the following values:
//   "Default"
//   "IncludeCookiesAndCredentials"
//   "IncludeSocketBytes"
//
// See the functions of the corresponding name in net_log_capture_mode.h for a
// description of their meaning.
const char kNetLogCaptureMode[]             = "net-log-capture-mode";

// Disables the default browser check. Useful for UI/browser tests where we
// want to avoid having the default browser info-bar displayed.
const char kNoDefaultBrowserCheck[]         = "no-default-browser-check";

// By default, an https page can load images, fonts or frames from an http
// page. This switch overrides this to block this lesser mixed-content problem.
const char kNoDisplayingInsecureContent[]   = "no-displaying-insecure-content";

// Disables all experiments set on about:flags. Does not disable about:flags
// itself. Useful if an experiment makes chrome crash at startup: One can start
// chrome with --no-experiments, disable the problematic lab at about:flags and
// then restart chrome without this switch again.
const char kNoExperiments[]                 = "no-experiments";

// Skip First Run tasks, whether or not it's actually the First Run. Overridden
// by kForceFirstRun. This does not drop the First Run sentinel and thus doesn't
// prevent first run from occuring the next time chrome is launched without this
// flag.
const char kNoFirstRun[]                    = "no-first-run";

// Don't send hyperlink auditing pings
const char kNoPings[]                       = "no-pings";

// Don't use a proxy server, always make direct connections. Overrides any
// other proxy server flags that are passed.
const char kNoProxyServer[]                 = "no-proxy-server";

// Disables the service process from adding itself as an autorun process. This
// does not delete existing autorun registrations, it just prevents the service
// from registering a new one.
const char kNoServiceAutorun[]              = "no-service-autorun";

// Does not automatically open a browser window on startup (used when
// launching Chrome for the purpose of hosting background apps).
const char kNoStartupWindow[]               = "no-startup-window";

// Disables checking whether we received an acknowledgment when registering
// a supervised user. Also disables the timeout during registration that waits
// for the ack. Useful when debugging against a server that does not
// support notifications.
const char kNoSupervisedUserAcknowledgmentCheck[]  =
    "no-managed-user-acknowledgment-check";

// Specifies the maximum number of threads to use for running the Proxy
// Autoconfig (PAC) script.
const char kNumPacThreads[]                 = "num-pac-threads";

// Launches URL in new browser window.
const char kOpenInNewWindow[]               = "new-window";

// Force use of QUIC for requests to the specified origin.
const char kOriginToForceQuicOn[]           = "origin-to-force-quic-on";

// The time that a new chrome process which is delegating to an already running
// chrome process started. (See ProcessSingleton for more details.)
const char kOriginalProcessStartTime[]      = "original-process-start-time";

// Packages an extension to a .crx installable file from a given directory.
const char kPackExtension[]                 = "pack-extension";

// Optional PEM private key to use in signing packaged .crx.
const char kPackExtensionKey[]              = "pack-extension-key";

// Specifies the path to the user data folder for the parent profile.
const char kParentProfile[]                 = "parent-profile";

// Development flag for permission request API. This flag is needed until
// the API is finalized.
// TODO(bauerb): Remove when this flag is not needed anymore.
const char kPermissionRequestApiScope[]     = "permission-request-api-scope";

// Development flag for permission request API. This flag is needed until
// the API is finalized.
// TODO(bauerb): Remove when this flag is not needed anymore.
const char kPermissionRequestApiUrl[]       = "permission-request-api-url";

// Use the PPAPI (Pepper) Flash found at the given path.
const char kPpapiFlashPath[]                = "ppapi-flash-path";

// Report the given version for the PPAPI (Pepper) Flash. The version should be
// numbers separated by '.'s (e.g., "12.3.456.78"). If not specified, it
// defaults to "10.2.999.999".
const char kPpapiFlashVersion[]             = "ppapi-flash-version";

// Triggers prerendering of pages from suggestions in the omnibox. Only has an
// effect when Instant is either disabled or restricted to search, and when
// prerender is enabled.
const char kPrerenderFromOmnibox[]          = "prerender-from-omnibox";
// These are the values the kPrerenderFromOmnibox switch may have, as in
// "--prerender-from-omnibox=auto". auto: Allow field trial selection.
const char kPrerenderFromOmniboxSwitchValueAuto[] = "auto";
//   disabled: No prerendering.
const char kPrerenderFromOmniboxSwitchValueDisabled[] = "disabled";
//   enabled: Guaranteed prerendering.
const char kPrerenderFromOmniboxSwitchValueEnabled[] = "enabled";
// Controls speculative prerendering of pages, and content prefetching. Both
// are dispatched from <link rel=prefetch href=...> elements.
const char kPrerenderMode[]                 = "prerender";
// These are the values the kPrerenderMode switch may have, as in
// "--prerender=disabled".
//   disabled: No prerendering.
const char kPrerenderModeSwitchValueDisabled[] = "disabled";
//   enabled: Prerendering.
const char kPrerenderModeSwitchValueEnabled[] = "enabled";

// Use IPv6 only for privet HTTP.
const char kPrivetIPv6Only[]                   = "privet-ipv6-only";

// Outputs the product version information and quit. Used as an internal api to
// detect the installed version of Chrome on Linux.
const char kProductVersion[]                = "product-version";

// Selects directory of profile to associate with the first browser launched.
const char kProfileDirectory[]              = "profile-directory";

// Starts the sampling based profiler for the browser process at startup. This
// will only work if chrome has been built with the gyp variable profiling=1.
// The output will go to the value of kProfilingFile.
const char kProfilingAtStart[]              = "profiling-at-start";

// Specifies a location for profiling output. This will only work if chrome has
// been built with the gyp variable profiling=1.
//
//   {pid} if present will be replaced by the pid of the process.
//   {count} if present will be incremented each time a profile is generated
//           for this process.
// The default is chrome-profile-{pid}.
const char kProfilingFile[]                 = "profiling-file";

// Controls whether profile data is periodically flushed to a file. Normally
// the data gets written on exit but cases exist where chrome doesn't exit
// cleanly (especially when using single-process). A time in seconds can be
// specified.
const char kProfilingFlush[]                = "profiling-flush";

// Forces proxy auto-detection.
const char kProxyAutoDetect[]               = "proxy-auto-detect";

// Specifies a list of hosts for whom we bypass proxy settings and use direct
// connections. Ignored if --proxy-auto-detect or --no-proxy-server are also
// specified. This is a comma-separated list of bypass rules. See:
// "net/proxy/proxy_bypass_rules.h" for the format of these rules.
const char kProxyBypassList[]               = "proxy-bypass-list";

// Uses the pac script at the given URL
const char kProxyPacUrl[]                   = "proxy-pac-url";

// Uses a specified proxy server, overrides system settings. This switch only
// affects HTTP and HTTPS requests.
const char kProxyServer[]                   = "proxy-server";

// Specifies a comma separated list of QUIC connection options to send to
// the server.
const char kQuicConnectionOptions[]         = "quic-connection-options";

// Specifies the maximum length for a QUIC packet.
const char kQuicMaxPacketLength[]           = "quic-max-packet-length";

// Specifies the version of QUIC to use.
const char kQuicVersion[]                   = "quic-version";

// Porvides a list of addresses to discover DevTools remote debugging targets.
// The format is <host>:<port>,...,<host>:port.
const char kRemoteDebuggingTargets[] = "remote-debugging-targets";

// Indicates the last session should be restored on startup. This overrides the
// preferences value and is primarily intended for testing. The value of this
// switch is the number of tabs to wait until loaded before 'load completed' is
// sent to the ui_test.
const char kRestoreLastSession[]            = "restore-last-session";

// Causes all iframes to be denied all permissions by default. Allowing
// particular permissions for an iframe may involve listing the names of
// these permissions as values of new iframe attributes.
const char kRestrictIFramePermissions[] = "restrict-iframe-permissions";

// Disable saving pages as HTML-only, disable saving pages as HTML Complete
// (with a directory of sub-resources). Enable only saving pages as MHTML.
// See http://crbug.com/120416 for how to remove this switch.
const char kSavePageAsMHTML[]               = "save-page-as-mhtml";

// If present, safebrowsing only performs update when
// SafeBrowsingProtocolManager::ForceScheduleNextUpdate() is explicitly called.
// This is used for testing only.
const char kSbDisableAutoUpdate[] = "safebrowsing-disable-auto-update";

// TODO(lzheng): Remove this flag once the feature works fine
// (http://crbug.com/74848).
//
// Disables safebrowsing feature that checks download url and downloads
// content's hash to make sure the content are not malicious.
const char kSbDisableDownloadProtection[] =
    "safebrowsing-disable-download-protection";

// Disables safebrowsing feature that checks for blacklisted extensions.
const char kSbDisableExtensionBlacklist[] =
    "safebrowsing-disable-extension-blacklist";

// Causes the process to run as a service process.
const char kServiceProcess[]                = "service";

// If true the app list will be shown.
const char kShowAppList[]                   = "show-app-list";

// Command line flag offering a "Show saved copy" option to the user if offline.
// The various modes are disabled, primary, or secondary. Primary/secondary
// refers to button placement (for experiment).
const char kShowSavedCopy[]                 = "show-saved-copy";

// Values for the kShowSavedCopy flag.
const char kEnableShowSavedCopyPrimary[]    = "primary";
const char kEnableShowSavedCopySecondary[]  = "secondary";
const char kDisableShowSavedCopy[]          = "disable";

// Does not show an infobar when an extension attaches to a page using
// chrome.debugger page. Required to attach to extension background pages.
const char kSilentDebuggerExtensionAPI[]    = "silent-debugger-extension-api";

// Causes Chrome to launch without opening any windows by default. Useful if
// one wishes to use Chrome as an ash server.
const char kSilentLaunch[]                  = "silent-launch";

// Simulates that elevation is needed to recover upgrade channel.
const char kSimulateElevatedRecovery[]      = "simulate-elevated-recovery";

// Simulates a critical update being available.
const char kSimulateCriticalUpdate[]        = "simulate-critical-update";

// Simulates that current version is outdated.
const char kSimulateOutdated[]              = "simulate-outdated";

// Simulates that current version is outdated and auto-update is off.
const char kSimulateOutdatedNoAU[]          = "simulate-outdated-no-au";

// Simulates an update being available.
const char kSimulateUpgrade[]               = "simulate-upgrade";

// Speculative resource prefetching.
const char kSpeculativeResourcePrefetching[] =
    "speculative-resource-prefetching";

// Speculative resource prefetching is disabled.
const char kSpeculativeResourcePrefetchingDisabled[] = "disabled";

// Speculative resource prefetching will only learn about resources that need to
// be prefetched but will not prefetch them.
const char kSpeculativeResourcePrefetchingLearning[] = "learning";

// Speculative resource prefetching is enabled.
const char kSpeculativeResourcePrefetchingEnabled[] = "enabled";

#if defined(ENABLE_SPELLCHECK)
#if defined(OS_ANDROID)
// Enables use of the Android spellchecker.
const char kEnableAndroidSpellChecker[] = "enable-android-spellchecker";
#endif

// Enables the multilingual spellchecker.
const char kEnableMultilingualSpellChecker[] =
    "enable-multilingual-spellchecker";

// Enables auto correction for misspelled words.
const char kEnableSpellingAutoCorrect[]     = "enable-spelling-auto-correct";

// Enables participation in the field trial for user feedback to spelling
// service.
const char kEnableSpellingFeedbackFieldTrial[] =
    "enable-spelling-feedback-field-trial";

// Specifies the URL where spelling service feedback data will be sent instead
// of the default URL. This switch is for temporary testing only.
// TODO(rouslan): Remove this flag when feedback testing is complete. Revisit by
// August 2013.
const char kSpellingServiceFeedbackUrl[] = "spelling-service-feedback-url";

// Specifies the number of seconds between sending batches of feedback to
// spelling service. The default is 30 minutes. The minimum is 5 seconds. This
// switch is for temporary testing only.
// TODO(rouslan): Remove this flag when feedback testing is complete. Revisit by
// August 2013.
const char kSpellingServiceFeedbackIntervalSeconds[] =
    "spelling-service-feedback-interval-seconds";
#endif

// Specifies the maximum SSL/TLS version ("tls1", "tls1.1", or "tls1.2").
const char kSSLVersionMax[]                 = "ssl-version-max";

// Specifies the minimum SSL/TLS version ("tls1", "tls1.1", or "tls1.2").
const char kSSLVersionMin[]                 = "ssl-version-min";

// Specifies the minimum SSL/TLS version ("tls1", "tls1.1", or "tls1.2") that
// TLS fallback will accept.
const char kSSLVersionFallbackMin[]         = "ssl-version-fallback-min";

// These values aren't switches, but rather the values that kSSLVersionMax,
// kSSLVersionMin and kSSLVersionFallbackMin can have.
const char kSSLVersionTLSv1[]               = "tls1";
const char kSSLVersionTLSv11[]              = "tls1.1";
const char kSSLVersionTLSv12[]              = "tls1.2";

// Starts the browser maximized, regardless of any previous settings.
const char kStartMaximized[]                = "start-maximized";

// Sets the supervised user ID for any loaded or newly created profile to the
// given value. Pass an empty string to mark the profile as non-supervised.
// Used for testing.
const char kSupervisedUserId[]              = "managed-user-id";

// Enables/disables SafeSites filtering for supervised users. Possible values
// are "enabled", "disabled", "blacklist-only", and "online-check-only".
const char kSupervisedUserSafeSites[]       = "supervised-user-safesites";

// Used to authenticate requests to the Sync service for supervised users.
// Setting this switch also causes Sync to be set up for a supervised user.
const char kSupervisedUserSyncToken[]       = "managed-user-sync-token";

// This flag causes sync to retry very quickly (see polling_constants.h) the
// when it encounters an error, as the first step towards exponential backoff.
const char kSyncShortInitialRetryOverride[] =
    "sync-short-initial-retry-override";

// Overrides the default server used for profile sync.
const char kSyncServiceURL[]                = "sync-url";

// Enables deferring sync backend initialization until user initiated changes
// occur.
const char kSyncDisableDeferredStartup[]     = "sync-disable-deferred-startup";

// Allows overriding the deferred init fallback timeout.
const char kSyncDeferredStartupTimeoutSeconds[] =
    "sync-deferred-startup-timeout-seconds";

// Enables feature to avoid unnecessary GetUpdate requests.
const char kSyncEnableGetUpdateAvoidance[]   =
    "sync-enable-get-update-avoidance";

// Enables clearing of sync data when a user enables passphrase encryption.
const char kSyncEnableClearDataOnPassphraseEncryption[] =
    "enable-clear-sync-data-on-passphrase-encryption";

// Disable data backup when user's not signed in.
const char kSyncDisableBackup[] = "disable-sync-backup";

// Disable sync rollback.
const char kSyncDisableRollback[] = "disable-sync-rollback";

// Passes the name of the current running automated test to Chrome.
const char kTestName[]                      = "test-name";

// Disables same-origin check on HTTP resources pushed via a SPDY proxy.
// The value is the host:port of the trusted proxy.
const char kTrustedSpdyProxy[]              = "trusted-spdy-proxy";

// Experimental. Shows a dialog asking the user to try chrome. This flag is to
// be used only by the upgrade process.
const char kTryChromeAgain[]                = "try-chrome-again";

// Overrides per-origin quota settings to unlimited storage for any
// apps/origins.  This should be used only for testing purpose.
const char kUnlimitedStorage[]              = "unlimited-storage";

// Treat given (insecure) origins as secure origins. Multiple origins can be
// supplied. Has no effect unless --user-data-dir is also supplied.
// Example:
// --unsafely-treat-insecure-origin-as-secure=http://a.test,http://b.test
// --user-data-dir=/test/only/profile/dir
const char kUnsafelyTreatInsecureOriginAsSecure[] =
    "unsafely-treat-insecure-origin-as-secure";

// Uses Spdy for the transport protocol instead of HTTP. This is a temporary
// testing flag.
const char kUseSpdy[]                       = "use-spdy";

// A string used to override the default user agent with a custom one.
const char kUserAgent[]                     = "user-agent";

// Specifies the user data directory, which is where the browser will look for
// all of its state.
const char kUserDataDir[]                   = "user-data-dir";

// Examines a .crx for validity and prints the result.
const char kValidateCrx[]                   = "validate-crx";

// Uses experimental simple cache backend if possible.
const char kUseSimpleCacheBackend[]         = "use-simple-cache-backend";

// Enables using an in-process Mojo service for the v8 proxy resolver.
const char kV8PacMojoInProcess[] = "v8-pac-mojo-in-process";

// Enables using an out-of-process Mojo service for the v8 proxy resolver.
const char kV8PacMojoOutOfProcess[] = "v8-pac-mojo-out-of-process";

// Specifies a custom URL for the server which reports variation data to the
// client. Specifying this switch enables the Variations service on
// unofficial builds. See variations_service.cc.
const char kVariationsServerURL[]           = "variations-server-url";

// Prints version information and quits.
const char kVersion[]                       = "version";

// Specify the initial window position: --window-position=x,y
const char kWindowPosition[]                = "window-position";

// Specify the initial window size: --window-size=w,h
const char kWindowSize[]                    = "window-size";

// Uses WinHTTP to fetch and evaluate PAC scripts. Otherwise the default is to
// use Chromium's network stack to fetch, and V8 to evaluate.
const char kWinHttpProxyResolver[]          = "winhttp-proxy-resolver";

// Specifies which category option was clicked in the Windows Jumplist that
// resulted in a browser startup.
const char kWinJumplistAction[]             = "win-jumplist-action";

#if defined(OS_ANDROID)
// Disables support for playing videos on Chromecast devices.
const char kDisableCast[]                    = "disable-cast";

// Disables Contextual Search.
const char kDisableContextualSearch[]        = "disable-contextual-search";

// Disables zero suggest experiment on Dev channel.
const char kDisableZeroSuggest[] = "disable-zero-suggest";

// Enable the accessibility tab switcher.
const char kEnableAccessibilityTabSwitcher[] =
    "enable-accessibility-tab-switcher";

// Enables Contextual Search.
const char kEnableContextualSearch[]        = "enable-contextual-search";

// Enables the DOM distiller animated button UI for Android.
const char kEnableDomDistillerButtonAnimation[] =
    "enable-dom-distiller-button-animation";

// Enables chrome hosted mode for Android.
const char kEnableHostedMode[] = "enable-hosted-mode";

// Enables instant search clicks feature.
const char kEnableInstantSearchClicks[] = "enable-instant-search-clicks";

// Enables context-sensitive reader mode button in the toolbar.
const char kEnableReaderModeToolbarIcon[] =
    "enable-reader-mode-toolbar-icon";

// Enables zero suggest functionality on Dev channel, showing most visited
// sites as default suggestions.
const char kEnableZeroSuggestMostVisited[] =
    "enable-zero-suggest-most-visited";

// Enable zero suggest functionality on Dev channel, showing most visited
// sites on non-search-result pages as default suggestions.
const char kEnableZeroSuggestMostVisitedWithoutSerp[] =
    "enable-zero-suggest-most-visited-without-serp";

// Triggers prerendering of search base page to prefetch results for the typed
// omnibox query. Only has an effect when prerender is enabled.
const char kPrefetchSearchResults[]         = "prefetch-search-results";
#endif  // defined(OS_ANDROID)

#if defined(USE_ASH)
const char kOpenAsh[]                       = "open-ash";
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_CHROMEOS)
// These flags show the man page on Linux. They are equivalent to each
// other.
const char kHelp[]                          = "help";
const char kHelpShort[]                     = "h";

// Specifies which password store to use (detect, default, gnome, kwallet).
const char kPasswordStore[]                 = "password-store";
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Triggers migration of user data directory to another directory
// specified as a parameter. The migration is done under singleton lock,
// and sanity checks are made to avoid corrupting the profile.
// The browser exits after migration is complete.
const char kMigrateDataDirForSxS[]          = "migrate-data-dir-for-sxs";
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
// Prevents Chrome from quitting when Chrome Apps are open.
const char kAppsKeepChromeAliveInTests[]    = "apps-keep-chrome-alive-in-tests";

// Shows a notification when quitting Chrome with hosted apps running. Default
// behavior is to also quit all hosted apps.
const char kHostedAppQuitNotification[] = "enable-hosted-app-quit-notification";

// Disable the toolkit-views App Info dialog for Mac.
const char kDisableAppInfoDialogMac[] = "disable-app-info-dialog-mac";

// Disables custom Cmd+` window cycling for platform apps and hosted apps.
const char kDisableAppWindowCycling[] = "disable-app-window-cycling";

// Disables app shim creation for hosted apps on Mac.
const char kDisableHostedAppShimCreation[] = "disable-hosted-app-shim-creation";

// Disables use of toolkit-views based native app windows.
const char kDisableMacViewsNativeAppWindows[] =
    "disable-mac-views-native-app-windows";

// Enable the toolkit-views App Info dialog for Mac. This is accessible from
// chrome://apps and chrome://extensions and is already enabled on non-mac.
const char kEnableAppInfoDialogMac[] = "enable-app-info-dialog-mac";

// Enables custom Cmd+` window cycling for platform apps and hosted apps.
const char kEnableAppWindowCycling[] = "enable-app-window-cycling";

// Enables use of toolkit-views based native app windows.
const char kEnableMacViewsNativeAppWindows[] =
    "enable-mac-views-native-app-windows";

// Enables Translate experimental new UX which replaces the infobar.
const char kEnableTranslateNewUX[]         = "enable-translate-new-ux";

// This is how the metrics client ID is passed from the browser process to its
// children. With Crashpad, the metrics client ID is distinct from the crash
// client ID.
const char kMetricsClientID[]               = "metrics-client-id";

// A process type (switches::kProcessType) that relaunches the browser. See
// chrome/browser/mac/relauncher.h.
const char kRelauncherProcess[]             = "relauncher";
#endif  // defined(OS_MACOSX)

// Use bubbles for content permissions requests instead of infobars.
const char kEnablePermissionsBubbles[]      = "enable-permissions-bubbles";

#if defined(OS_WIN)
// Fallback to XPS. By default connector uses CDD.
const char kEnableCloudPrintXps[]           = "enable-cloud-print-xps";

// Force-enables the profile shortcut manager. This is needed for tests since
// they use a custom-user-data-dir which disables this.
const char kEnableProfileShortcutManager[]  = "enable-profile-shortcut-manager";

// For the DelegateExecute verb handler to launch Chrome in metro mode on
// Windows 8 and higher.  Used when relaunching metro Chrome.
const char kForceImmersive[]                = "force-immersive";

// Whether or not the browser should warn if the profile is on a network share.
// This flag is only relevant for Windows currently.
const char kNoNetworkProfileWarning[]       = "no-network-profile-warning";

// For the DelegateExecute verb handler to launch Chrome in desktop mode on
// Windows 8 and higher.  Used when relaunching metro Chrome.
const char kForceDesktop[]                  = "force-desktop";

// Makes Windows happy by allowing it to show "Enable access to this program"
// checkbox in Add/Remove Programs->Set Program Access and Defaults. This only
// shows an error box because the only way to hide Chrome is by uninstalling
// it.
const char kHideIcons[]                     = "hide-icons";

// Relaunches metro Chrome on Windows 8 and higher using a given shortcut.
const char kRelaunchShortcut[]              = "relaunch-shortcut";

// See kHideIcons.
const char kShowIcons[]                     = "show-icons";

// Runs un-installation steps that were done by chrome first-run.
const char kUninstall[]                     = "uninstall";

// Requests that Chrome launch the Metro viewer process via the given appid
// (which is assumed to be registered as default browser) and synchronously
// connect to it.
const char kViewerLaunchViaAppId[]          = "viewer-launch-via-appid";

// Waits for the given handle to be signaled before relaunching metro Chrome on
// Windows 8 and higher.
const char kWaitForMutex[]                  = "wait-for-mutex";

// Indicates that chrome was launched to service a search request in Windows 8.
const char kWindows8Search[]                = "windows8-search";
#endif  // defined(OS_WIN)

#if defined(ENABLE_IPC_FUZZER)
// Specifies the testcase used by the IPC fuzzer.
const char kIpcFuzzerTestcase[]             = "ipc-fuzzer-testcase";
#endif

#if defined(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
// Enables support to debug printing subsystem.
const char kDebugPrint[] = "debug-print";
#endif

bool AboutInSettingsEnabled() {
  return SettingsWindowEnabled() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             ::switches::kDisableAboutInSettings);
}

bool MdDownloadsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableMaterialDesignDownloads);
}

bool MdSettingsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableMaterialDesignSettings);
}

bool MediaRouterEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableMediaRouter);
}

// Will return true as a default value if the "disable-new-offline-error-page"
// command-line switch is not available.
bool NewOfflineErrorPageEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kDisableNewOfflineErrorPage);
}

bool PdfMaterialUIEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kEnablePdfMaterialUI))
    return true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kDisablePdfMaterialUI))
    return false;

  // Default.
  return false;
}

bool SettingsWindowEnabled() {
#if defined(OS_CHROMEOS)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kDisableSettingsWindow);
#else
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableSettingsWindow);
#endif
}

#if defined(OS_CHROMEOS)
bool PowerOverlayEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnablePowerOverlay);
}
#endif

// -----------------------------------------------------------------------------
// DO NOT ADD YOUR CRAP TO THE BOTTOM OF THIS FILE.
//
// You were going to just dump your switches here, weren't you? Instead, please
// put them in alphabetical order above, or in order inside the appropriate
// ifdef at the bottom. The order should match the header.
// -----------------------------------------------------------------------------

}  // namespace switches
