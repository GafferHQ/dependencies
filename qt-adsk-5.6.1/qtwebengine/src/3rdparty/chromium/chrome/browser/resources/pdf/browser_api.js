// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Returns a promise that will resolve to the default zoom factor.
 * @param {!Object} streamInfo The stream object pointing to the data contained
 *     in the PDF.
 * @return {Promise<number>} A promise that will resolve to the default zoom
 *     factor.
 */
function lookupDefaultZoom(streamInfo) {
  // Webviews don't run in tabs so |streamInfo.tabId| is -1 when running within
  // a webview.
  if (!chrome.tabs || streamInfo.tabId < 0)
    return Promise.resolve(1);

  return new Promise(function(resolve, reject) {
    chrome.tabs.getZoomSettings(streamInfo.tabId, function(zoomSettings) {
      resolve(zoomSettings.defaultZoomFactor);
    });
  });
}

/**
 * A class providing an interface to the browser.
 */
class BrowserApi {
  /**
   * @constructor
   * @param {!Object} streamInfo The stream object which points to the data
   *     contained in the PDF.
   * @param {number} defaultZoom The default browser zoom.
   * @param {boolean} manageZoom Whether to manage zoom.
   */
  constructor(streamInfo, defaultZoom, manageZoom) {
    this.streamInfo_ = streamInfo;
    this.defaultZoom_ = defaultZoom;
    this.manageZoom_ = manageZoom;
  }

  /**
   * Returns a promise to a BrowserApi.
   * @param {!Object} streamInfo The stream object pointing to the data
   *     contained in the PDF.
   * @param {boolean} manageZoom Whether to manage zoom.
   */
  static create(streamInfo, manageZoom) {
    return lookupDefaultZoom(streamInfo).then(function(defaultZoom) {
      return new BrowserApi(streamInfo, defaultZoom, manageZoom);
    });
  }

  /**
   * Returns the stream info pointing to the data contained in the PDF.
   * @return {Object} The stream info object.
   */
  getStreamInfo() {
    return this.streamInfo_;
  }

  /**
   * Aborts the stream.
   */
  abortStream() {
    if (chrome.mimeHandlerPrivate)
      chrome.mimeHandlerPrivate.abortStream();
  }

  /**
   * Sets the browser zoom.
   * @param {number} zoom The zoom factor to send to the browser.
   * @return {Promise} A promise that will be resolved when the browser zoom
   *     has been updated.
   */
  setZoom(zoom) {
    if (!this.manageZoom_)
      return Promise.resolve();
    return new Promise(function(resolve, reject) {
      chrome.tabs.setZoom(this.streamInfo_.tabId, zoom, resolve);
    }.bind(this));
  }

  /**
   * Returns the default browser zoom factor.
   * @return {number} The default browser zoom factor.
   */
  getDefaultZoom() {
    return this.defaultZoom_;
  }

  /**
   * Adds an event listener to be notified when the browser zoom changes.
   * @param {function} listener The listener to be called with the new zoom
   *     factor.
   */
  addZoomEventListener(listener) {
    if (!this.manageZoom_)
      return;

    chrome.tabs.onZoomChange.addListener(function(zoomChangeInfo) {
      if (zoomChangeInfo.tabId != this.streamInfo_.tabId)
        return;
      listener(zoomChangeInfo.newZoomFactor);
    }.bind(this));
  }
};

/**
 * Creates a BrowserApi for an extension running as a mime handler.
 * @return {Promise<BrowserApi>} A promise to a BrowserApi instance constructed
 *     using the mimeHandlerPrivate API.
 */
function createBrowserApiForMimeHandlerView() {
  return new Promise(function(resolve, reject) {
    chrome.mimeHandlerPrivate.getStreamInfo(resolve);
  }).then(function(streamInfo) {
    let manageZoom = !streamInfo.embedded && streamInfo.tabId != -1;
    return new Promise(function(resolve, reject) {
      if (!manageZoom) {
        resolve();
        return;
      }
      chrome.tabs.setZoomSettings(
          streamInfo.tabId, {mode: 'manual', scope: 'per-tab'}, resolve);
    }).then(function() { return BrowserApi.create(streamInfo, manageZoom); });
  });
}

/**
 * Creates a BrowserApi instance for an extension not running as a mime handler.
 * @return {Promise<BrowserApi>} A promise to a BrowserApi instance constructed
 *     from the URL.
 */
function createBrowserApiForStandaloneExtension() {
  let url = window.location.search.substring(1);
  let streamInfo = {
    streamUrl: url,
    originalUrl: url,
    responseHeaders: {},
    embedded: window.parent != window,
    tabId: -1,
  };
  return new Promise(function(resolve, reject) {
    if (!chrome.tabs) {
      resolve();
      return;
    }
    chrome.tabs.getCurrent(function(tab) {
      streamInfo.tabId = tab.id;
      resolve();
    });
  }).then(function() { return BrowserApi.create(streamInfo, false); });
}

/**
 * Returns a promise that will resolve to a BrowserApi instance.
 * @return {Promise<BrowserApi>} A promise to a BrowserApi instance for the
 *     current environment.
 */
function createBrowserApi() {
  if (window.location.search)
    return createBrowserApiForStandaloneExtension();

  return createBrowserApiForMimeHandlerView();
}
