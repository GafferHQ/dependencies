// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Any strings used here will already be localized. Values such as
// CastMode.type or IDs will be defined elsewhere and determined later.
cr.define('media_router', function() {
  'use strict';

  /**
   * @enum {string}
   */
  var SinkStatus = {
    IDLE: 'idle',
    ACTIVE: 'active',
    REQUEST_PENDING: 'request_pending'
  };


  /**
   * @param {number} type The type of cast mode. This corresponds to the
   *   C++ MediaCastMode.
   * @param {string} title The title of the cast mode.
   * @param {string} description The description of the cast mode.
   * @constructor
   * @struct
   */
  var CastMode = function(type, title, description) {
    /** @type {number} */
    this.type = type;

    /** @type {string} */
    this.title = title;

    /** @type {string} */
    this.description = description;
  };


  /**
   * @param {string} id The ID of this issue.
   * @param {string} title The issue title.
   * @param {string} message The issue message.
   * @param {number} defaultActionType The type of default action.
   * @param {?number} secondaryActionType The type of optional action.
   * @param {?string} mediaRouteId The route ID to which this issue
   *                  pertains. If not set, this is a global issue.
   * @param {boolean} isBlocking True if this issue blocks other UI.
   * @param {?number} helpPageId The numeric help center ID.
   * @constructor
   * @struct
   */
  var Issue = function(id, title, message, defaultActionType,
                       secondaryActionType, mediaRouteId, isBlocking,
                       helpPageId) {
    /** @type {string} */
    this.id = id;

    /** @type {string} */
    this.title = title;

    /** @type {string} */
    this.message = message;

    /** @type {number} */
    this.defaultActionType = defaultActionType;

    /** @type {?number} */
    this.secondaryActionType = secondaryActionType;

    /** @type {?string} */
    this.mediaRouteId = mediaRouteId;

    /** @type {boolean} */
    this.isBlocking = isBlocking;

    /** @type {?number} */
    this.helpPageId = helpPageId;
  };


  /**
   * @param {string} id The media route ID.
   * @param {string} sinkId The ID of the media sink running this route.
   * @param {string} title The short description of this route.
   * @param {?number} tabId The ID of the tab in which web app is running and
   *                  accessing the route.
   * @param {boolean} isLocal True if this is a locally created route.
   * @constructor
   * @struct
   */
  var Route = function(id, sinkId, title, tabId, isLocal) {
    /** @type {string} */
    this.id = id;

    /** @type {string} */
    this.sinkId = sinkId;

    /** @type {string} */
    this.title = title;

    /** @type {?number} */
    this.tabId = tabId;

    /** @type {boolean} */
    this.isLocal = isLocal;
  };


  /**
   * @param {string} id The ID of the media sink.
   * @param {string} name The name of the sink.
   * @param {media_router.SinkStatus} status The readiness state of the sink.
   * @param {!Array<number>} castModes Cast modes compatible with the sink.
   * @constructor
   * @struct
   */
  var Sink = function(id, name, status, castModes) {
    /** @type {string} */
    this.id = id;

    /** @type {string} */
    this.name = name;

    /** @type {media_router.SinkStatus} */
    this.status = status;

    /** @type {!Array<number>} */
    this.castModes = castModes;
  };


  /**
   * @param {number} tabId The current tab ID.
   * @param {string} domain The domain of the current tab.
   * @constructor
   * @struct
   */
  var TabInfo = function(tabId, domain) {
    /** @type {number} */
    this.tabId = tabId;

    /** @type {string} */
    this.domain = domain;
  };

  return {
    SinkStatus: SinkStatus,
    CastMode: CastMode,
    Issue: Issue,
    Route: Route,
    Sink: Sink,
    TabInfo: TabInfo,
  };
});
