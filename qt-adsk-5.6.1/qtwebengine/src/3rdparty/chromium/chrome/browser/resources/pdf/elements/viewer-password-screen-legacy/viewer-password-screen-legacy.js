// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-password-screen-legacy',

  properties: {
    text: {
      type: String,
      value: 'This document is password protected. Please enter a password.',
    },

    active: {
      type: Boolean,
      value: false,
      observer: 'activeChanged'
    }
  },

  timerId: undefined,

  ready: function() {
    this.activeChanged();
  },

  accept: function() {
    this.active = false;
  },

  deny: function() {
    this.$.password.disabled = false;
    this.$.submit.disabled = false;
    this.$.password.focus();
    this.$.password.select();
  },

  submit: function(e) {
    // Prevent the default form submission behavior.
    e.preventDefault();
    if (this.$.password.value.length == 0)
      return;
    this.$.password.disabled = true;
    this.$.submit.disabled = true;
    this.fire('password-submitted', {password: this.$.password.value});
  },

  activeChanged: function() {
    clearTimeout(this.timerId);
    this.timerId = undefined;
    if (this.active) {
      this.style.visibility = 'visible';
      this.style.opacity = 1;
      this.$.password.focus();
    } else {
      this.style.opacity = 0;
      this.timerId = setTimeout(function() {
        this.style.visibility = 'hidden';
      }.bind(this), 400);
    }
  }
});
