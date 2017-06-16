// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'viewer-progress-bar',

  properties: {
    progress: {
      type: Number,
      observer: 'progressChanged'
    },

    text: {
      type: String,
      value: 'Loading'
    },

    numSegments: {
      type: Number,
      value: 8,
      observer: 'numSegmentsChanged'
    }
  },

  segments: [],

  ready: function() {
    this.numSegmentsChanged();
  },

  progressChanged: function() {
    var numVisible = this.progress * this.segments.length / 100.0;
    for (var i = 0; i < this.segments.length; i++) {
      this.segments[i].style.visibility =
          i < numVisible ? 'inherit' : 'hidden';
    }

    if (this.progress >= 100 || this.progress < 0)
      this.style.opacity = 0;
  },

  numSegmentsChanged: function() {
    // Clear the existing segments.
    this.segments = [];
    var segmentsElement = this.$.segments;
    segmentsElement.innerHTML = '';

    // Create the new segments.
    var segment = document.createElement('li');
    segment.classList.add('segment');
    var angle = 360 / this.numSegments;
    for (var i = 0; i < this.numSegments; ++i) {
      var segmentCopy = segment.cloneNode(true);
      segmentCopy.style.webkitTransform =
          'rotate(' + (i * angle) + 'deg) skewY(' +
          -1 * (90 - angle) + 'deg)';
      segmentsElement.appendChild(segmentCopy);
      this.segments.push(segmentCopy);
    }
    this.progressChanged();
  }
});
