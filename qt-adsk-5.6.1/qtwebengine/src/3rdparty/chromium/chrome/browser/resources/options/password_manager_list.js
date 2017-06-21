// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.passwordManager', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var DeletableItemList = options.DeletableItemList;
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var List = cr.ui.List;

  /** @const */ var URL_DATA_INDEX = 0;
  /** @const */ var USERNAME_DATA_INDEX = 1;
  /** @const */ var PASSWORD_DATA_INDEX = 2;
  /** @const */ var FEDERATION_DATA_INDEX = 3;
  /** @const */ var ORIGINAL_DATA_INDEX = 4;

  /**
   * Creates a new passwords list item.
   * @param {cr.ui.ArrayDataModel} dataModel The data model that contains this
   *     item.
   * @param {Array} entry An array of the form [url, username, password,
   *     federation]. When the list has been filtered, a fifth element [index]
   *     may be present.
   * @param {boolean} showPasswords If true, add a button to the element to
   *     allow the user to reveal the saved password.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function PasswordListItem(dataModel, entry, showPasswords) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.dataModel = dataModel;
    el.__proto__ = PasswordListItem.prototype;
    el.showPasswords_ = showPasswords;
    el.decorate();

    return el;
  }

  PasswordListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // The URL of the site.
      var urlLabel = this.ownerDocument.createElement('div');
      urlLabel.classList.add('favicon-cell');
      urlLabel.classList.add('weakrtl');
      urlLabel.classList.add('url');
      urlLabel.setAttribute('title', this.url);
      urlLabel.textContent = this.url;

      // The favicon URL is prefixed with "origin/", which essentially removes
      // the URL path past the top-level domain and ensures that a scheme (e.g.,
      // http) is being used. This ensures that the favicon returned is the
      // default favicon for the domain and that the URL has a scheme if none
      // is present in the password manager.
      urlLabel.style.backgroundImage = getFaviconImageSet(
          'origin/' + this.url, 16);
      this.contentElement.appendChild(urlLabel);

      // The stored username.
      var usernameLabel = this.ownerDocument.createElement('div');
      usernameLabel.className = 'name';
      usernameLabel.textContent = this.username;
      usernameLabel.title = this.username;
      this.contentElement.appendChild(usernameLabel);

      if (this.federation) {
        // The federation.
        var federationDiv = this.ownerDocument.createElement('div');
        federationDiv.className = 'federation';
        federationDiv.textContent = this.federation;
        this.contentElement.appendChild(federationDiv);
      } else {
        // The stored password.
        var passwordInputDiv = this.ownerDocument.createElement('div');
        passwordInputDiv.className = 'password';

        // The password input field.
        var passwordInput = this.ownerDocument.createElement('input');
        passwordInput.type = 'password';
        passwordInput.className = 'inactive-password';
        passwordInput.readOnly = true;
        passwordInput.value = this.showPasswords_ ? this.password : '********';
        passwordInputDiv.appendChild(passwordInput);
        var deletableItem = this;
        passwordInput.addEventListener('focus', function() {
          deletableItem.handleFocus();
        });
        this.passwordField = passwordInput;
        this.setFocusable_(false);

        // The show/hide button.
        if (this.showPasswords_) {
          var button = this.ownerDocument.createElement('button');
          button.hidden = true;
          button.className = 'list-inline-button custom-appearance';
          button.textContent = loadTimeData.getString('passwordShowButton');
          button.addEventListener('click', this.onClick_.bind(this), true);
          button.addEventListener('mousedown', function(event) {
            // Don't focus on this button by mousedown.
            event.preventDefault();
            // Don't handle list item selection. It causes focus change.
            event.stopPropagation();
          }, false);
          button.addEventListener('focus', function() {
            deletableItem.handleFocus();
          });
          passwordInputDiv.appendChild(button);
          this.passwordShowButton = button;
        }
        this.contentElement.appendChild(passwordInputDiv);
      }

    },

    /** @override */
    selectionChanged: function() {
      var input = this.passwordField;
      var button = this.passwordShowButton;
      // The button doesn't exist when passwords can't be shown.
      if (!button)
        return;

      if (this.selected) {
        input.classList.remove('inactive-password');
        this.setFocusable_(true);
        button.hidden = false;
        input.focus();
      } else {
        input.classList.add('inactive-password');
        this.setFocusable_(false);
        button.hidden = true;
      }
    },

    /**
     * Set the focusability of this row.
     * @param {boolean} focusable
     * @private
     */
    setFocusable_: function(focusable) {
      var tabIndex = focusable ? 0 : -1;
      this.passwordField.tabIndex = this.closeButtonElement.tabIndex = tabIndex;
    },

    /**
     * Reveals the plain text password of this entry.
     */
    showPassword: function(password) {
      this.passwordField.value = password;
      this.passwordField.type = 'text';

      var button = this.passwordShowButton;
      if (button)
        button.textContent = loadTimeData.getString('passwordHideButton');
    },

    /**
     * Hides the plain text password of this entry.
     */
    hidePassword: function() {
      this.passwordField.type = 'password';

      var button = this.passwordShowButton;
      if (button)
        button.textContent = loadTimeData.getString('passwordShowButton');
    },

    /**
     * Get the original index of this item in the data model.
     * @return {number} The index.
     * @private
     */
    getOriginalIndex_: function() {
      var index = this.dataItem[ORIGINAL_DATA_INDEX];
      return index ? index : this.dataModel.indexOf(this.dataItem);
    },

    /**
     * On-click event handler. Swaps the type of the input field from password
     * to text and back.
     * @private
     */
    onClick_: function(event) {
      if (this.passwordField.type == 'password') {
        // After the user is authenticated, showPassword() will be called.
        PasswordManager.requestShowPassword(this.getOriginalIndex_());
      } else {
        this.hidePassword();
      }
    },

    /**
     * Get and set the URL for the entry.
     * @type {string}
     */
    get url() {
      return this.dataItem[URL_DATA_INDEX];
    },
    set url(url) {
      this.dataItem[URL_DATA_INDEX] = url;
    },

    /**
     * Get and set the username for the entry.
     * @type {string}
     */
    get username() {
      return this.dataItem[USERNAME_DATA_INDEX];
    },
    set username(username) {
      this.dataItem[USERNAME_DATA_INDEX] = username;
    },

    /**
     * Get and set the password for the entry.
     * @type {string}
     */
    get password() {
      return this.dataItem[PASSWORD_DATA_INDEX];
    },
    set password(password) {
      this.dataItem[PASSWORD_DATA_INDEX] = password;
    },

    /**
     * Get and set the federation for the entry.
     * @type {string}
     */
    get federation() {
      return this.dataItem[FEDERATION_DATA_INDEX];
    },
    set federation(federation) {
      this.dataItem[FEDERATION_DATA_INDEX] = federation;
    },
  };

  /**
   * Creates a new PasswordExceptions list item.
   * @param {Array} entry A pair of the form [url, username].
   * @constructor
   * @extends {options.DeletableItem}
   */
  function PasswordExceptionsListItem(entry) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.__proto__ = PasswordExceptionsListItem.prototype;
    el.decorate();

    return el;
  }

  PasswordExceptionsListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /**
     * Call when an element is decorated as a list item.
     */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // The URL of the site.
      var urlLabel = this.ownerDocument.createElement('div');
      urlLabel.className = 'url';
      urlLabel.classList.add('favicon-cell');
      urlLabel.classList.add('weakrtl');
      urlLabel.textContent = this.url;

      // The favicon URL is prefixed with "origin/", which essentially removes
      // the URL path past the top-level domain and ensures that a scheme (e.g.,
      // http) is being used. This ensures that the favicon returned is the
      // default favicon for the domain and that the URL has a scheme if none
      // is present in the password manager.
      urlLabel.style.backgroundImage = getFaviconImageSet(
          'origin/' + this.url, 16);
      this.contentElement.appendChild(urlLabel);
    },

    /**
     * Get the url for the entry.
     * @type {string}
     */
    get url() {
      return this.dataItem;
    },
    set url(url) {
      this.dataItem = url;
    },
  };

  /**
   * Create a new passwords list.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var PasswordsList = cr.ui.define('list');

  PasswordsList.prototype = {
    __proto__: DeletableItemList.prototype,

    /**
     * Whether passwords can be revealed or not.
     * @type {boolean}
     * @private
     */
    showPasswords_: true,

    /** @override */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      Preferences.getInstance().addEventListener(
          'profile.password_manager_allow_show_passwords',
          this.onPreferenceChanged_.bind(this));
      this.addEventListener('focus', this.onFocus_.bind(this));
    },

    /**
     * Listener for changes on the preference.
     * @param {Event} event The preference update event.
     * @private
     */
    onPreferenceChanged_: function(event) {
      this.showPasswords_ = event.value.value;
      this.redraw();
    },

    /**
     * @override
     * @param {Array} entry
     */
    createItem: function(entry) {
      var showPasswords = this.showPasswords_;

      if (loadTimeData.getBoolean('disableShowPasswords'))
        showPasswords = false;

      return new PasswordListItem(this.dataModel, entry, showPasswords);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      var item = this.dataModel.item(index);
      if (item && item[ORIGINAL_DATA_INDEX] != undefined) {
        // The fifth element, if present, is the original index to delete.
        index = item[ORIGINAL_DATA_INDEX];
      }
      PasswordManager.removeSavedPassword(index);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },

    /**
     * Will make to first row focusable if none are selected. This makes it
     * possible to tab into the rows without pressing up/down first.
     * @param {Event} e The focus event.
     * @private
     */
    onFocus_: function(e) {
      if (!this.selectedItem && this.items)
        this.items[0].setFocusable_(true);
    },
  };

  /**
   * Create a new passwords list.
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var PasswordExceptionsList = cr.ui.define('list');

  PasswordExceptionsList.prototype = {
    __proto__: DeletableItemList.prototype,

    /**
     * @override
     * @param {Array} entry
     */
    createItem: function(entry) {
      return new PasswordExceptionsListItem(entry);
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      PasswordManager.removePasswordException(index);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },
  };

  return {
    PasswordListItem: PasswordListItem,
    PasswordExceptionsListItem: PasswordExceptionsListItem,
    PasswordsList: PasswordsList,
    PasswordExceptionsList: PasswordExceptionsList,
  };
});
