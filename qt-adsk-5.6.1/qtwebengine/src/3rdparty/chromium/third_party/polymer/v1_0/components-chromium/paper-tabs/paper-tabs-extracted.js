

  Polymer({

    is: 'paper-tabs',

    behaviors: [
      Polymer.IronResizableBehavior,
      Polymer.IronMenubarBehavior
    ],

    properties: {

      /**
       * If true, ink ripple effect is disabled.
       */
      noink: {
        type: Boolean,
        value: false
      },

      /**
       * If true, the bottom bar to indicate the selected tab will not be shown.
       */
      noBar: {
        type: Boolean,
        value: false
      },

      /**
       * If true, the slide effect for the bottom bar is disabled.
       */
      noSlide: {
        type: Boolean,
        value: false
      },

      /**
       * If true, tabs are scrollable and the tab width is based on the label width.
       */
      scrollable: {
        type: Boolean,
        value: false
      },

      /**
       * If true, dragging on the tabs to scroll is disabled.
       */
      disableDrag: {
        type: Boolean,
        value: false
      },

      /**
       * If true, scroll buttons (left/right arrow) will be hidden for scrollable tabs.
       */
      hideScrollButtons: {
        type: Boolean,
        value: false
      },

      /**
       * If true, the tabs are aligned to bottom (the selection bar appears at the top).
       */
      alignBottom: {
        type: Boolean,
        value: false
      },

      /**
       * Gets or sets the selected element. The default is to use the index of the item.
       */
      selected: {
        type: String,
        notify: true
      },

      selectable: {
        type: String,
        value: 'paper-tab'
      },

      _step: {
        type: Number,
        value: 10
      },

      _holdDelay: {
        type: Number,
        value: 1
      },

      _leftHidden: {
        type: Boolean,
        value: false
      },

      _rightHidden: {
        type: Boolean,
        value: false
      },

      _previousTab: {
        type: Object
      }
    },

    hostAttributes: {
      role: 'tablist'
    },

    listeners: {
      'iron-resize': '_onResize',
      'iron-select': '_onIronSelect',
      'iron-deselect': '_onIronDeselect'
    },

    _computeScrollButtonClass: function(hideThisButton, scrollable, hideScrollButtons) {
      if (!scrollable || hideScrollButtons) {
        return 'hidden';
      }

      if (hideThisButton) {
        return 'not-visible';
      }

      return '';
    },

    _computeTabsContentClass: function(scrollable) {
      return scrollable ? 'scrollable' : 'horizontal layout';
    },

    _computeSelectionBarClass: function(noBar, alignBottom) {
      if (noBar) {
        return 'hidden';
      } else if (alignBottom) {
        return 'align-bottom';
      }
    },

    // TODO(cdata): Add `track` response back in when gesture lands.

    _onResize: function() {
      this.debounce('_onResize', function() {
        this._scroll();
        this._tabChanged(this.selectedItem);
      }, 10);
    },

    _onIronSelect: function(event) {
      this._tabChanged(event.detail.item, this._previousTab);
      this._previousTab = event.detail.item;
      this.cancelDebouncer('tab-changed');
    },

    _onIronDeselect: function(event) {
      this.debounce('tab-changed', function() {
        this._tabChanged(null, this._previousTab);
      // See polymer/polymer#1305
      }, 1);
    },

    get _tabContainerScrollSize () {
      return Math.max(
        0,
        this.$.tabsContainer.scrollWidth -
          this.$.tabsContainer.offsetWidth
      );
    },

    _scroll: function() {
      var scrollLeft;

      if (!this.scrollable) {
        return;
      }

      scrollLeft = this.$.tabsContainer.scrollLeft;

      this._leftHidden = scrollLeft === 0;
      this._rightHidden = scrollLeft === this._tabContainerScrollSize;
    },

    _onLeftScrollButtonDown: function() {
      this._holdJob = setInterval(this._scrollToLeft.bind(this), this._holdDelay);
    },

    _onRightScrollButtonDown: function() {
      this._holdJob = setInterval(this._scrollToRight.bind(this), this._holdDelay);
    },

    _onScrollButtonUp: function() {
      clearInterval(this._holdJob);
      this._holdJob = null;
    },

    _scrollToLeft: function() {
      this.$.tabsContainer.scrollLeft -= this._step;
    },

    _scrollToRight: function() {
      this.$.tabsContainer.scrollLeft += this._step;
    },

    _tabChanged: function(tab, old) {
      if (!tab) {
        this._positionBar(0, 0);
        return;
      }

      var r = this.$.tabsContent.getBoundingClientRect();
      var w = r.width;
      var tabRect = tab.getBoundingClientRect();
      var tabOffsetLeft = tabRect.left - r.left;

      this._pos = {
        width: this._calcPercent(tabRect.width, w),
        left: this._calcPercent(tabOffsetLeft, w)
      };

      if (this.noSlide || old == null) {
        // position bar directly without animation
        this._positionBar(this._pos.width, this._pos.left);
        return;
      }

      var oldRect = old.getBoundingClientRect();
      var oldIndex = this.items.indexOf(old);
      var index = this.items.indexOf(tab);
      var m = 5;

      // bar animation: expand
      this.$.selectionBar.classList.add('expand');

      if (oldIndex < index) {
        this._positionBar(this._calcPercent(tabRect.left + tabRect.width - oldRect.left, w) - m,
            this._left);
      } else {
        this._positionBar(this._calcPercent(oldRect.left + oldRect.width - tabRect.left, w) - m,
            this._calcPercent(tabOffsetLeft, w) + m);
      }

      if (this.scrollable) {
        this._scrollToSelectedIfNeeded(tabRect.width, tabOffsetLeft);
      }
    },

    _scrollToSelectedIfNeeded: function(tabWidth, tabOffsetLeft) {
      var l = tabOffsetLeft - this.$.tabsContainer.scrollLeft;
      if (l < 0) {
        this.$.tabsContainer.scrollLeft += l;
      } else {
        l += (tabWidth - this.$.tabsContainer.offsetWidth);
        if (l > 0) {
          this.$.tabsContainer.scrollLeft += l;
        }
      }
    },

    _calcPercent: function(w, w0) {
      return 100 * w / w0;
    },

    _positionBar: function(width, left) {
      this._width = width;
      this._left = left;
      this.transform(
          'translate3d(' + left + '%, 0, 0) scaleX(' + (width / 100) + ')',
          this.$.selectionBar);
    },

    _onBarTransitionEnd: function(e) {
      var cl = this.$.selectionBar.classList;
      // bar animation: expand -> contract
      if (cl.contains('expand')) {
        cl.remove('expand');
        cl.add('contract');
        this._positionBar(this._pos.width, this._pos.left);
      // bar animation done
      } else if (cl.contains('contract')) {
        cl.remove('contract');
      }
    }

  });

