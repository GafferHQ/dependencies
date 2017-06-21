// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-settings-search-engines-page' is the settings page
 * containing search engines settings.
 *
 * Example:
 *
 *    <core-animated-pages>
 *      <cr-settings-search-engines-page prefs="{{prefs}}">
 *      </cr-settings-search-engines-page>
 *      ... other pages ...
 *    </core-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-search-engines-page
 */
Polymer({
  is: 'cr-settings-search-engines-page',

  properties: {
    /**
     * Route for the page.
     */
    route: {
      type: String,
      value: ''
    },

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: true,
      readOnly: true
    },

    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'search_engines',
      readOnly: true
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: loadTimeData.getString('searchEnginesPageTitle'),
      readOnly: true
    },

    /**
     * Name of the 'core-icon' to be shown in the settings-page-header.
     */
    icon: {
      type: String,
      value: 'search',
      readOnly: true
    },

    /** @type {!Array<!SearchEngine>} */
    defaultEngines: {
      type: Array,
      value: function() { return []; }
    },

    /** @type {!Array<!SearchEngine>} */
    otherEngines: {
      type: Array,
      value: function() { return []; }
    },
  },

  /** @override */
  ready: function() {
    chrome.searchEnginesPrivate.onSearchEnginesChanged.addListener(
        this.enginesChanged_.bind(this));
    this.enginesChanged_();
  },

  /** @private */
  enginesChanged_: function() {
    chrome.searchEnginesPrivate.getSearchEngines(function(engines) {
      this.defaultEngines = engines.filter(function(engine) {
        return engine.type ==
            chrome.searchEnginesPrivate.SearchEngineType.DEFAULT;
      }, this);

      this.otherEngines = engines.filter(function(engine) {
        return engine.type ==
            chrome.searchEnginesPrivate.SearchEngineType.OTHER;
      }, this);
    }.bind(this));
  }
});
