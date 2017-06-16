// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DIGIT_LENGTH = 0.6;

Polymer({
  is: 'viewer-page-selector',

  properties: {
    /**
     * The number of pages the document contains.
     */
    docLength: {
      type: Number,
      value: 1,
      observer: 'docLengthChanged'
    },

    /**
     * The current page being viewed (1-based).
     */
    pageNo: {
      type: String,
      value: '1'
    }
  },

  pageNoCommitted: function() {
    var page = parseInt(this.pageNo);
    if (!isNaN(page)) {
      this.fire('change-page', {page: page - 1});
    }
  },

  docLengthChanged: function() {
    var numDigits = this.docLength.toString().length;
    this.$.pageselector.style.width = (numDigits * DIGIT_LENGTH) + 'em';
  },

  select: function() {
    this.$.input.select();
  }
});
