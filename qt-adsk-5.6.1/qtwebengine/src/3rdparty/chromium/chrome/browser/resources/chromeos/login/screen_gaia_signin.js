// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

login.createScreen('GaiaSigninScreen', 'gaia-signin', function() {
  // Gaia loading time after which error message must be displayed and
  // lazy portal check should be fired.
  /** @const */ var GAIA_LOADING_PORTAL_SUSSPECT_TIME_SEC = 7;

  // GAIA animation guard timer. Started when GAIA page is loaded
  // (Authenticator 'ready' event) and is intended to guard against edge cases
  // when 'showView' message is not generated/received.
  /** @const */ var GAIA_ANIMATION_GUARD_MILLISEC = 300;

  // Maximum Gaia loading time in seconds.
  /** @const */ var MAX_GAIA_LOADING_TIME_SEC = 60;

  /** @const */ var HELP_TOPIC_ENTERPRISE_REPORTING = 2535613;

  // The help topic regarding user not being in the whitelist.
  /** @const */ var HELP_CANT_ACCESS_ACCOUNT = 188036;

  return {
    EXTERNAL_API: [
      'loadAuthExtension',
      'updateAuthExtension',
      'doReload',
      'onWebviewError',
      'onFrameError',
      'updateCancelButtonState',
      'showWhitelistCheckFailedError',
    ],

    /**
     * Frame loading error code (0 - no error).
     * @type {number}
     * @private
     */
    error_: 0,

    /**
     * Saved gaia auth host load params.
     * @type {?string}
     * @private
     */
    gaiaAuthParams_: null,

    /**
     * Whether local version of Gaia page is used.
     * @type {boolean}
     * @private
     */
    isLocal_: false,

    /**
     * Whether new Gaia flow is active.
     * @type {boolean}
     */
    isNewGaiaFlow: false,

    /**
     * Email of the user, which is logging in using offline mode.
     * @type {string}
     */
    email: '',

    /**
     * Whether consumer management enrollment is in progress.
     * @type {boolean}
     * @private
     */
    isEnrollingConsumerManagement_: false,

    /**
     * Timer id of pending load.
     * @type {number}
     * @private
     */
    loadingTimer_: undefined,

    /**
     * Timer id of a guard timer that is fired in case 'showView' message
     * is not received from GAIA.
     * @type {number}
     * @private
     */
    loadAnimationGuardTimer_: undefined,

    /**
     * Whether we've processed 'showView' message - either from GAIA or from
     * guard timer.
     * @type {boolean}
     * @private
     */
    showViewProcessed_: undefined,

    /**
     * Whether user can cancel Gaia screen.
     * @type {boolean}
     * @private
     */
    cancelAllowed_: undefined,

    /**
     * Whether we should show user pods on the login screen.
     * @type {boolean}
     * @private
     */
    isShowUsers_: undefined,

    /**
     * SAML password confirmation attempt count.
     * @type {number}
     */
    samlPasswordConfirmAttempt_: 0,

    /**
     * Whether we should show webview based signin.
     * @type {boolean}
     * @private
     */
    isWebviewSignin: false,

    /** @override */
    decorate: function() {
      this.isWebviewSignin = loadTimeData.getValue('isWebviewSignin');
      if (this.isWebviewSignin) {
        // Replace iframe with webview.
        var webview = this.ownerDocument.createElement('webview');
        webview.id = 'signin-frame';
        webview.name = 'signin-frame';
        webview.hidden = true;
        $('signin-frame').parentNode.replaceChild(webview, $('signin-frame'));
        this.gaiaAuthHost_ = new cr.login.GaiaAuthHost(webview);

        $('offline-gaia').addEventListener('authCompleted',
            this.onAuthCompletedMessage_.bind(this));
      } else {
        this.gaiaAuthHost_ = new cr.login.GaiaAuthHost($('signin-frame'));
      }
      this.gaiaAuthHost_.addEventListener(
          'ready', this.onAuthReady_.bind(this));
      this.gaiaAuthHost_.addEventListener(
          'dialogShown', this.onDialogShown_.bind(this));
      this.gaiaAuthHost_.addEventListener(
          'dialogHidden', this.onDialogHidden_.bind(this));
      this.gaiaAuthHost_.addEventListener(
          'backButton', this.onBackButton_.bind(this));
      this.gaiaAuthHost_.addEventListener(
          'showView', this.onShowView_.bind(this));
      this.gaiaAuthHost_.confirmPasswordCallback =
          this.onAuthConfirmPassword_.bind(this);
      this.gaiaAuthHost_.noPasswordCallback =
          this.onAuthNoPassword_.bind(this);
      this.gaiaAuthHost_.insecureContentBlockedCallback =
          this.onInsecureContentBlocked_.bind(this);
      this.gaiaAuthHost_.missingGaiaInfoCallback =
          this.missingGaiaInfo_.bind(this);
      this.gaiaAuthHost_.samlApiUsedCallback =
          this.samlApiUsed_.bind(this);
      this.gaiaAuthHost_.addEventListener('authDomainChange',
          this.onAuthDomainChange_.bind(this));
      this.gaiaAuthHost_.addEventListener('authFlowChange',
          this.onAuthFlowChange_.bind(this));
      this.gaiaAuthHost_.addEventListener('authCompleted',
          this.onAuthCompletedMessage_.bind(this));
      this.gaiaAuthHost_.addEventListener('loadAbort',
        this.onLoadAbortMessage_.bind(this));
      this.gaiaAuthHost_.addEventListener(
          'identifierEntered', this.onIdentifierEnteredMessage_.bind(this));

      $('enterprise-info-hint-link').addEventListener('click', function(e) {
        chrome.send('launchHelpApp', [HELP_TOPIC_ENTERPRISE_REPORTING]);
        e.preventDefault();
      });

      $('back-button-item').addEventListener('click', function(e) {
        $('back-button-item').hidden = true;
        $('signin-frame').back();
        e.preventDefault();
      });

      $('close-button-item').addEventListener('click', function(e) {
        this.cancel();
        e.preventDefault();
      }.bind(this));

      $('gaia-whitelist-error').addEventListener('buttonclick', function() {
         this.showWhitelistCheckFailedError(false);
      }.bind(this));

      $('gaia-whitelist-error').addEventListener('linkclick', function() {
        chrome.send('launchHelpApp', [HELP_CANT_ACCESS_ACCOUNT]);
      });

      this.updateLocalizedContent();
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('signinScreenTitle');
    },

    /**
     * Returns true if local version of Gaia is used.
     * @type {boolean}
     */
    get isLocal() {
      return this.isLocal_;
    },

    /**
     * Sets whether local version of Gaia is used.
     * @param {boolean} value Whether local version of Gaia is used.
     */
    set isLocal(value) {
      this.isLocal_ = value;
      if (this.isNewGaiaFlow) {
        $('signin-frame').hidden = this.isLocal_;
        $('offline-gaia').hidden = !this.isLocal_;
      }
      chrome.send('updateOfflineLogin', [value]);
    },

    /**
     * Shows/hides loading UI.
     * @param {boolean} show True to show loading UI.
     * @private
     */
    showLoadingUI_: function(show) {
      $('gaia-loading').hidden = !show;
      if (this.isNewGaiaFlow && this.isLocal) {
        $('offline-gaia').hidden = show;
      } else {
        $('signin-frame').hidden = show;
      }
      $('signin-right').hidden = show;
      $('enterprise-info-container').hidden = show;
      $('gaia-signin-divider').hidden = show;
      this.classList.toggle('loading', show);
      $('signin-frame').classList.remove('show');
      if (!show)
        this.classList.remove('auth-completed');
    },

    /**
     * Handler for Gaia loading suspiciously long timeout.
     * @private
     */
    onLoadingSuspiciouslyLong_: function() {
      if (this != Oobe.getInstance().currentScreen)
        return;
      chrome.send('showLoadingTimeoutError');
      this.loadingTimer_ = setTimeout(
          this.onLoadingTimeOut_.bind(this),
          (MAX_GAIA_LOADING_TIME_SEC - GAIA_LOADING_PORTAL_SUSSPECT_TIME_SEC) *
          1000);
    },

    /**
     * Handler for Gaia loading timeout.
     * @private
     */
    onLoadingTimeOut_: function() {
      this.loadingTimer_ = undefined;
      chrome.send('showLoadingTimeoutError');
    },

    /**
     * Clears loading timer.
     * @private
     */
    clearLoadingTimer_: function() {
      if (this.loadingTimer_) {
        clearTimeout(this.loadingTimer_);
        this.loadingTimer_ = undefined;
      }
    },

    /**
     * Sets up loading timer.
     * @private
     */
    startLoadingTimer_: function() {
      this.clearLoadingTimer_();
      this.loadingTimer_ = setTimeout(
          this.onLoadingSuspiciouslyLong_.bind(this),
          GAIA_LOADING_PORTAL_SUSSPECT_TIME_SEC * 1000);
    },

    /**
     * Handler for GAIA animation guard timer.
     * @private
     */
    onLoadAnimationGuardTimer_: function() {
      this.loadAnimationGuardTimer_ = undefined;
      this.onShowView_();
    },

    /**
     * Clears GAIA animation guard timer.
     * @private
     */
    clearLoadAnimationGuardTimer_: function() {
      if (this.loadAnimationGuardTimer_) {
        clearTimeout(this.loadAnimationGuardTimer_);
        this.loadAnimationGuardTimer_ = undefined;
      }
    },

    /**
     * Sets up GAIA animation guard timer.
     * @private
     */
    startLoadAnimationGuardTimer_: function() {
      this.clearLoadAnimationGuardTimer_();
      this.loadAnimationGuardTimer_ = setTimeout(
          this.onLoadAnimationGuardTimer_.bind(this),
          GAIA_ANIMATION_GUARD_MILLISEC);
    },

    /**
     * Whether Gaia is loading.
     * @type {boolean}
     */
    get loading() {
      return !$('gaia-loading').hidden;
    },
    set loading(loading) {
      if (loading == this.loading)
        return;

      this.showLoadingUI_(loading);
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {string} data Screen init payload. Url of auth extension start
     *                      page.
     */
    onBeforeShow: function(data) {
      chrome.send('loginUIStateChanged', ['gaia-signin', true]);
      $('login-header-bar').signinUIState =
          this.isEnrollingConsumerManagement_ ?
              SIGNIN_UI_STATE.CONSUMER_MANAGEMENT_ENROLLMENT :
              SIGNIN_UI_STATE.GAIA_SIGNIN;

      // Ensure that GAIA signin (or loading UI) is actually visible.
      window.requestAnimationFrame(function() {
        chrome.send('loginVisible', ['gaia-loading']);
      });
      $('back-button-item').disabled = false;
      $('back-button-item').hidden = true;
      $('close-button-item').disabled = false;
      this.classList.toggle('loading', this.loading);

      // Button header is always visible when sign in is presented.
      // Header is hidden once GAIA reports on successful sign in.
      Oobe.getInstance().headerHidden = false;
    },

    onAfterShow: function(data) {
      if (!this.loading && this.isWebviewSignin) {
        if (this.isLocal)
          $('offline-gaia').focus();
        else
          $('signin-frame').focus();
      }
    },

    /**
     * Event handler that is invoked just before the screen is hidden.
     */
    onBeforeHide: function() {
      chrome.send('loginUIStateChanged', ['gaia-signin', false]);
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.HIDDEN;
    },

    /**
     * Loads the authentication extension into the iframe.
     * @param {Object} data Extension parameters bag.
     * @private
     */
    loadAuthExtension: function(data) {
      this.isNewGaiaFlow = data.useNewGaiaFlow;
      this.isLocal = data.isLocal;
      this.email = '';

      // Reset SAML
      this.classList.toggle('full-width', false);
      this.samlPasswordConfirmAttempt_ = 0;

      this.updateAuthExtension(data);

      var params = {};
      for (var i in cr.login.GaiaAuthHost.SUPPORTED_PARAMS) {
        var name = cr.login.GaiaAuthHost.SUPPORTED_PARAMS[i];
        if (data[name])
          params[name] = data[name];
      }

      if (data.localizedStrings)
        params.localizedStrings = data.localizedStrings;

      if (this.isNewGaiaFlow) {
        $('inner-container').classList.add('new-gaia-flow');
        params.chromeType = data.chromeType;
        params.isNewGaiaFlowChromeOS = true;
      }

      if (data.gaiaEndpoint)
        params.gaiaPath = data.gaiaEndpoint;

      $('login-header-bar').newGaiaFlow = this.isNewGaiaFlow;

      // Screen size could have been changed because of 'new-gaia-flow' or
      // 'full-width' classes.
      if (Oobe.getInstance().currentScreen === this)
        Oobe.getInstance().updateScreenSize(this);

      if (data.forceReload ||
          JSON.stringify(this.gaiaAuthParams_) != JSON.stringify(params)) {
        this.error_ = 0;

        var authMode = cr.login.GaiaAuthHost.AuthMode.DEFAULT;
        if (data.useOffline)
          authMode = cr.login.GaiaAuthHost.AuthMode.OFFLINE;

        this.gaiaAuthParams_ = params;
        this.loading = true;
        this.startLoadingTimer_();

        if (this.isLocal && this.isNewGaiaFlow) {
          this.loadOffline(params);
          this.onAuthReady_();
        } else {
          this.gaiaAuthHost_.load(authMode,
                                  params,
                                  this.onAuthCompleted_.bind(this));
        }
      } else if (this.loading && this.error_) {
        // An error has occurred, so trying to reload.
        this.doReload();
      }
    },

    /**
     * Updates the authentication extension with new parameters, if needed.
     * @param {Object} data New extension parameters bag.
     * @private
     */
    updateAuthExtension: function(data) {
      var reasonLabel = $('gaia-signin-reason');
      if (data.passwordChanged) {
        reasonLabel.textContent =
            loadTimeData.getString('signinScreenPasswordChanged');
        reasonLabel.hidden = false;
      } else {
        reasonLabel.hidden = true;
      }

      if (this.isNewGaiaFlow) {
        $('login-header-bar').showCreateSupervisedButton =
            data.supervisedUsersCanCreate;
        $('login-header-bar').showGuestButton = data.guestSignin;
      } else {
        $('createAccount').hidden = !data.createAccount;
        $('guestSignin').hidden = !data.guestSignin;
        $('createSupervisedUserPane').hidden = !data.supervisedUsersEnabled;

        $('createSupervisedUserLinkPlaceholder').hidden =
            !data.supervisedUsersCanCreate;
        $('createSupervisedUserNoManagerText').hidden =
            data.supervisedUsersCanCreate;
        $('createSupervisedUserNoManagerText').textContent =
            data.supervisedUsersRestrictionReason;
      }

      var isEnrollingConsumerManagement = data.isEnrollingConsumerManagement;
      $('consumerManagementEnrollment').hidden = !isEnrollingConsumerManagement;

      this.isShowUsers_ = data.isShowUsers;
      this.updateCancelButtonState();

      this.isEnrollingConsumerManagement_ = isEnrollingConsumerManagement;

      // Sign-in right panel is hidden if all of its items are hidden.
      var noRightPanel = $('gaia-signin-reason').hidden &&
                         $('createAccount').hidden &&
                         $('guestSignin').hidden &&
                         $('createSupervisedUserPane').hidden &&
                         $('consumerManagementEnrollment').hidden;
      this.classList.toggle('no-right-panel', noRightPanel);
      this.classList.toggle('full-width', false);
      if (Oobe.getInstance().currentScreen === this)
        Oobe.getInstance().updateScreenSize(this);
    },

    /**
     * Updates [Cancel] button state. Allow cancellation of screen only when
     * user pods can be displayed.
     */
    updateCancelButtonState: function() {
      this.cancelAllowed_ = this.isLocal ||
                            (this.isShowUsers_ && $('pod-row').pods.length);
      $('login-header-bar').allowCancel = this.cancelAllowed_;
      if (this.isNewGaiaFlow)
        $('close-button-item').hidden = !this.cancelAllowed_;
    },

    /**
     * Whether the current auth flow is SAML.
     */
    isSAML: function() {
       return this.gaiaAuthHost_.authFlow ==
           cr.login.GaiaAuthHost.AuthFlow.SAML;
    },

    /**
     * Invoked when the authDomain property is changed on the GAIA host.
     */
    onAuthDomainChange_: function() {
      $('saml-notice-message').textContent = loadTimeData.getStringF(
          'samlNotice',
          this.gaiaAuthHost_.authDomain);
    },

    /**
     * Invoked when the authFlow property is changed on the GAIA host.
     * @param {Event} e Property change event.
     */
    onAuthFlowChange_: function(e) {
      var isSAML = this.isSAML();

      this.classList.toggle('no-right-panel', isSAML);
      this.classList.toggle('full-width', isSAML);
      $('saml-notice-container').hidden = !isSAML;

      if (Oobe.getInstance().currentScreen === this) {
        Oobe.getInstance().updateScreenSize(this);
        $('login-header-bar').allowCancel = isSAML || this.cancelAllowed_;
        if (this.isNewGaiaFlow)
          $('close-button-item').hidden = !(isSAML || this.cancelAllowed_);
      }
    },

    /**
     * Invoked when the auth host emits 'ready' event.
     * @private
     */
    onAuthReady_: function() {
      showViewProcessed_ = false;
      if (this.isNewGaiaFlow)
        this.startLoadAnimationGuardTimer_();

      this.clearLoadingTimer_();
      this.loading = false;

      if (!this.isNewGaiaFlow)
        this.onLoginUIVisible_();

      // Warm up the user images screen.
      Oobe.getInstance().preloadScreen({id: SCREEN_USER_IMAGE_PICKER});
    },

    /**
     * Invoked when the auth host emits 'dialogShown' event.
     * @private
     */
    onDialogShown_: function() {
      $('back-button-item').disabled = true;
      $('close-button-item').disabled = true;
    },

    /**
     * Invoked when the auth host emits 'dialogHidden' event.
     * @private
     */
    onDialogHidden_: function() {
      $('back-button-item').disabled = false;
      $('close-button-item').disabled = false;
    },

    /**
     * Invoked when the auth host emits 'backButton' event.
     * @private
     */
    onBackButton_: function(e) {
      $('back-button-item').hidden = !e.detail;
      $('login-header-bar').updateUI_();
    },

    /**
     * Invoked when the auth host emits 'showView' event or when corresponding
     * guard time fires.
     * @private
     */
    onShowView_: function(e) {
      if (showViewProcessed_)
        return;

      showViewProcessed_ = true;
      this.clearLoadAnimationGuardTimer_();
      $('signin-frame').classList.add('show');
      this.onLoginUIVisible_();
    },

    /**
     * Called when UI is shown.
     * @private
     */
    onLoginUIVisible_: function() {
      // Show deferred error bubble.
      if (this.errorBubble_) {
        this.showErrorBubble(this.errorBubble_[0], this.errorBubble_[1]);
        this.errorBubble_ = undefined;
      }

      chrome.send('loginWebuiReady');
      chrome.send('loginVisible', ['gaia-signin']);
    },

    /**
     * Invoked when the user has successfully authenticated via SAML, the
     * principals API was not used and the auth host needs the user to confirm
     * the scraped password.
     * @param {string} email The authenticated user's e-mail.
     * @param {number} passwordCount The number of passwords that were scraped.
     * @private
     */
    onAuthConfirmPassword_: function(email, passwordCount) {
      this.loading = true;
      Oobe.getInstance().headerHidden = false;

      if (this.samlPasswordConfirmAttempt_ == 0)
        chrome.send('scrapedPasswordCount', [passwordCount]);

      if (this.samlPasswordConfirmAttempt_ < 2) {
        login.ConfirmPasswordScreen.show(
            email,
            this.samlPasswordConfirmAttempt_,
            this.onConfirmPasswordCollected_.bind(this));
      } else {
        chrome.send('scrapedPasswordVerificationFailed');
        this.showFatalAuthError(
            loadTimeData.getString('fatalErrorMessageVerificationFailed'),
            loadTimeData.getString('fatalErrorTryAgainButton'));
      }
      if (this.isNewGaiaFlow) {
        this.classList.toggle('no-right-panel', false);
        this.classList.toggle('full-width', false);
      }
    },

    /**
     * Invoked when the confirm password screen is dismissed.
     * @private
     */
    onConfirmPasswordCollected_: function(password) {
      this.samlPasswordConfirmAttempt_++;
      this.gaiaAuthHost_.verifyConfirmedPassword(password);

      // Shows signin UI again without changing states.
      Oobe.showScreen({id: SCREEN_GAIA_SIGNIN});
    },

    /**
     * Inovked when the user has successfully authenticated via SAML, the
     * principals API was not used and no passwords could be scraped.
     * @param {string} email The authenticated user's e-mail.
     */
    onAuthNoPassword_: function(email) {
      this.showFatalAuthError(
          loadTimeData.getString('fatalErrorMessageNoPassword'),
          loadTimeData.getString('fatalErrorTryAgainButton'));
      chrome.send('scrapedPasswordCount', [0]);
    },

    /**
     * Invoked when the authentication flow had to be aborted because content
     * served over an unencrypted connection was detected. Shows a fatal error.
     * This method is only called on Chrome OS, where the entire authentication
     * flow is required to be encrypted.
     * @param {string} url The URL that was blocked.
     */
    onInsecureContentBlocked_: function(url) {
      this.showFatalAuthError(
          loadTimeData.getStringF('fatalErrorMessageInsecureURL', url),
          loadTimeData.getString('fatalErrorDoneButton'));
    },

    /**
     * Shows the fatal auth error.
     * @param {string} message The error message to show.
     * @param {string} buttonLabel The label to display on dismiss button.
     */
    showFatalAuthError: function(message, buttonLabel) {
      login.FatalErrorScreen.show(message, buttonLabel, Oobe.showSigninUI);
    },

    /**
     * Show fatal auth error when information is missing from GAIA.
     */
    missingGaiaInfo_: function() {
      this.showFatalAuthError(
          loadTimeData.getString('fatalErrorMessageNoAccountDetails'),
          loadTimeData.getString('fatalErrorTryAgainButton'));
    },

    /**
     * Record that SAML API was used during sign-in.
     */
    samlApiUsed_: function() {
      chrome.send('usingSAMLAPI');
    },

    /**
     * Invoked when auth is completed successfully.
     * @param {!Object} credentials Credentials of the completed authentication.
     * @private
     */
    onAuthCompleted_: function(credentials) {
      if (credentials.useOffline) {
        this.email = credentials.email;
        chrome.send('authenticateUser',
                    [credentials.email,
                     credentials.password]);
      } else if (credentials.authCode) {
        if (credentials.hasOwnProperty('authCodeOnly') &&
            credentials.authCodeOnly) {
          chrome.send('completeAuthenticationAuthCodeOnly',
                      [credentials.authCode]);
        } else {
          chrome.send('completeAuthentication', [
            credentials.gaiaId,
            credentials.email,
            credentials.password,
            credentials.authCode,
            credentials.usingSAML,
            credentials.gapsCookie
          ]);
        }
      } else {
        chrome.send('completeLogin',
                    [credentials.gaiaId,
                     credentials.email,
                     credentials.password,
                     credentials.usingSAML]);
      }

      this.loading = true;
      this.classList.add('auth-completed');
      // Now that we're in logged in state header should be hidden.
      Oobe.getInstance().headerHidden = true;
      // Clear any error messages that were shown before login.
      Oobe.clearErrors();
    },

    /**
     * Invoked when onAuthCompleted message received.
     * @param {!Object} e Payload of the received HTML5 message.
     * @private
     */
    onAuthCompletedMessage_: function(e) {
      this.onAuthCompleted_(e.detail);
    },

    /**
     * Invoked when onLoadAbort message received.
     * @param {!Object} e Payload of the received HTML5 message.
     * @private
     */
    onLoadAbortMessage_: function(e) {
      this.onWebviewError(e.detail);
    },

    /**
     * Invoked when identifierEntered message received.
     * @param {!Object} e Payload of the received HTML5 message.
     * @private
     */
    onIdentifierEnteredMessage_: function(e) {
      this.onIdentifierEntered(e.detail);
    },

    /**
     * Clears input fields and switches to input mode.
     * @param {boolean} takeFocus True to take focus.
     * @param {boolean} forceOnline Whether online sign-in should be forced.
     * If |forceOnline| is false previously used sign-in type will be used.
     */
    reset: function(takeFocus, forceOnline) {
      // Reload and show the sign-in UI if needed.
      if (takeFocus) {
        if (!forceOnline && this.isLocal) {
          // Show 'Cancel' button to allow user to return to the main screen
          // (e.g. this makes sense when connection is back).
          Oobe.getInstance().headerHidden = false;
          $('login-header-bar').signinUIState = SIGNIN_UI_STATE.GAIA_SIGNIN;
          // Do nothing, since offline version is reloaded after an error comes.
        } else {
          Oobe.showSigninUI();
        }
      }
    },

    /**
     * Reloads extension frame.
     */
    doReload: function() {
      if (this.isLocal)
        return;
      this.error_ = 0;
      this.gaiaAuthHost_.reload();
      this.loading = true;
      this.startLoadingTimer_();
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      $('createAccount').innerHTML = loadTimeData.getStringF(
          'createAccount',
          '<a id="createAccountLink" class="signin-link" href="#">',
          '</a>');
      $('guestSignin').innerHTML = loadTimeData.getStringF(
          'guestSignin',
          '<a id="guestSigninLink" class="signin-link" href="#">',
          '</a>');
      $('createSupervisedUserLinkPlaceholder').innerHTML =
          loadTimeData.getStringF(
              'createSupervisedUser',
              '<a id="createSupervisedUserLink" class="signin-link" href="#">',
              '</a>');
      $('consumerManagementEnrollment').innerHTML = loadTimeData.getString(
          'consumerManagementEnrollmentSigninMessage');
      $('createAccountLink').addEventListener('click', function(e) {
        chrome.send('createAccount');
        e.preventDefault();
      });
      $('guestSigninLink').addEventListener('click', function(e) {
        chrome.send('launchIncognito');
        e.preventDefault();
      });
      $('createSupervisedUserLink').addEventListener('click', function(e) {
        chrome.send('showSupervisedUserCreationScreen');
        e.preventDefault();
      });
    },

    /**
     * Shows sign-in error bubble.
     * @param {number} loginAttempts Number of login attemps tried.
     * @param {HTMLElement} content Content to show in bubble.
     */
    showErrorBubble: function(loginAttempts, error) {
      if (this.isLocal) {
        $('add-user-button').hidden = true;
        $('cancel-add-user-button').hidden = false;
        // Reload offline version of the sign-in extension, which will show
        // error itself.
        chrome.send('offlineLogin', [this.email]);
      } else if (!this.loading) {
        // We want to show bubble near "Email" field, but we can't calculate
        // it's position because it is located inside iframe. So we only
        // can hardcode some constants.
        /** @const */ var ERROR_BUBBLE_OFFSET = 84;
        /** @const */ var ERROR_BUBBLE_PADDING = 0;
        $('bubble').showContentForElement($('login-box'),
                                          cr.ui.Bubble.Attachment.LEFT,
                                          error,
                                          ERROR_BUBBLE_OFFSET,
                                          ERROR_BUBBLE_PADDING);
      } else {
        // Defer the bubble until the frame has been loaded.
        this.errorBubble_ = [loginAttempts, error];
      }
    },

    /**
     * Called when user canceled signin.
     */
    cancel: function() {
      if (!this.cancelAllowed_) {
        // In OOBE signin screen, cancel is not allowed because there is
        // no other screen to show. If user is in middle of a saml flow,
        // reset signin screen to get out of the saml flow.
        if (this.isSAML())
          Oobe.resetSigninUI(true);

        return;
      }

      $('offline-gaia').switchToEmailCard();

      this.classList.remove('whitelist-error');
      Oobe.showUserPods();
    },

    /**
     * Handler for iframe's error notification coming from the outside.
     * For more info see C++ class 'WebUILoginView' which calls this method.
     * @param {number} error Error code.
     * @param {string} url The URL that failed to load.
     */
    onFrameError: function(error, url) {
      this.error_ = error;
      chrome.send('frameLoadingCompleted', [this.error_]);
    },

    /**
     * Handler for webview error handling.
     * @param {!Object} data Additional information about error event like:
     * {string} error Error code such as "ERR_INTERNET_DISCONNECTED".
     * {string} url The URL that failed to load.
     */
    onWebviewError: function(data) {
      chrome.send('webviewLoadAborted', [data.error]);
    },

    /**
     * Handler for identifierEntered event.
     * @param {!Object} data The identifier entered by user:
     * {string} accountIdentifier User identifier.
     */
    onIdentifierEntered: function(data) {
      chrome.send('identifierEntered', [data.accountIdentifier]);
    },

    /**
     * Sets welcome and enterpriseinfo strings for offline gaia.
     * Also sets callback and sends message whether we already have email and
     * should switch to the password screen with error.
     */
    loadOffline: function(params) {
      var offlineLogin = $('offline-gaia');
      var strings = params.localizedStrings;
      if ('stringEnterpriseInfo' in strings)
        offlineLogin.enterpriseInfo = strings['stringEnterpriseInfo'];
      if ('emailDomain' in params)
        offlineLogin.emailDomain = '@' + params['emailDomain'];
      offlineLogin.setEmail(params.email);
    },

    /**
     * Show/Hide error when user is not in whitelist. When UI is hidden
     * GAIA is reloaded.
     * @param {boolean} show Show/hide error UI.
     * @param {!Object} opt_data Optional additional information.
     */
    showWhitelistCheckFailedError: function(show, opt_data) {
      if (show) {
        var isManaged = opt_data && opt_data.enterpriseManaged;
        $('gaia-whitelist-error').textContent =
            loadTimeData.getValue(isManaged ? 'whitelistErrorEnterprise' :
                                              'whitelistErrorConsumer');
      }

      this.classList.toggle('whitelist-error', show);
      this.loading = !show;

      if (!show)
        Oobe.showSigninUI();
    }
  };
});
