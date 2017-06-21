

  (function() {

    'use strict';

   // this would be the only `paper-drawer-panel` in
   // the whole app that can be in `dragging` state
    var sharedPanel = null;

    function classNames(obj) {
      var classes = [];
      for (var key in obj) {
        if (obj.hasOwnProperty(key) && obj[key]) {
          classes.push(key);
        }
      }

      return classes.join(' ');
    }

    Polymer({

      is: 'paper-drawer-panel',

      /**
       * Fired when the narrow layout changes.
       *
       * @event paper-responsive-change {{narrow: boolean}} detail -
       *     narrow: true if the panel is in narrow layout.
       */

      /**
       * Fired when the selected panel changes.
       *
       * Listening for this event is an alternative to observing changes in the `selected` attribute.
       * This event is fired both when a panel is selected and deselected.
       * The `isSelected` detail property contains the selection state.
       *
       * @event paper-select {{isSelected: boolean, item: Object}} detail -
       *     isSelected: True for selection and false for deselection.
       *     item: The panel that the event refers to.
       */

      properties: {

        /**
         * The panel to be selected when `paper-drawer-panel` changes to narrow
         * layout.
         */
        defaultSelected: {
          type: String,
          value: 'main'
        },

        /**
         * If true, swipe from the edge is disable.
         */
        disableEdgeSwipe: {
          type: Boolean,
          value: false
        },

        /**
         * If true, swipe to open/close the drawer is disabled.
         */
        disableSwipe: {
          type: Boolean,
          value: false
        },

        /**
         * Whether the user is dragging the drawer interactively.
         */
        dragging: {
          type: Boolean,
          value: false,
          readOnly: true,
          notify: true
        },

        /**
         * Width of the drawer panel.
         */
        drawerWidth: {
          type: String,
          value: '256px'
        },

        /**
         * How many pixels on the side of the screen are sensitive to edge
         * swipes and peek.
         */
        edgeSwipeSensitivity: {
          type: Number,
          value: 30
        },

        /**
         * If true, ignore `responsiveWidth` setting and force the narrow layout.
         */
        forceNarrow: {
          type: Boolean,
          value: false
        },

        /**
         * Whether the browser has support for the transform CSS property.
         */
        hasTransform: {
          type: Boolean,
          value: function() {
            return 'transform' in this.style;
          }
        },

        /**
         * Whether the browser has support for the will-change CSS property.
         */
        hasWillChange: {
          type: Boolean,
          value: function() {
            return 'willChange' in this.style;
          }
        },

        /**
         * Returns true if the panel is in narrow layout.  This is useful if you
         * need to show/hide elements based on the layout.
         */
        narrow: {
          reflectToAttribute: true,
          type: Boolean,
          value: false,
          readOnly: true,
          notify: true
        },

        /**
         * Whether the drawer is peeking out from the edge.
         */
        peeking: {
          type: Boolean,
          value: false,
          readOnly: true,
          notify: true
        },

        /**
         * Max-width when the panel changes to narrow layout.
         */
        responsiveWidth: {
          type: String,
          value: '640px'
        },

        /**
         * If true, position the drawer to the right.
         */
        rightDrawer: {
          type: Boolean,
          value: false
        },

        /**
         * The panel that is being selected. `drawer` for the drawer panel and
         * `main` for the main panel.
         */
        selected: {
          reflectToAttribute: true,
          notify: true,
          type: String,
          value: null
        },

        /**
         * The attribute on elements that should toggle the drawer on tap, also elements will
         * automatically be hidden in wide layout.
         */
        drawerToggleAttribute: {
          type: String,
          value: 'paper-drawer-toggle'
        },

        /**
         * Whether the transition is enabled.
         */
        transition: {
          type: Boolean,
          value: false
        },

      },

      listeners: {
        tap: '_onTap',
        track: '_onTrack',
        down: '_downHandler',
        up: '_upHandler'
      },

      observers: [
        '_forceNarrowChanged(forceNarrow, defaultSelected)'
      ],

      /**
       * Toggles the panel open and closed.
       *
       * @method togglePanel
       */
      togglePanel: function() {
        if (this._isMainSelected()) {
          this.openDrawer();
        } else {
          this.closeDrawer();
        }
      },

      /**
       * Opens the drawer.
       *
       * @method openDrawer
       */
      openDrawer: function() {
        this.selected = 'drawer';
      },

      /**
       * Closes the drawer.
       *
       * @method closeDrawer
       */
      closeDrawer: function() {
        this.selected = 'main';
      },

      ready: function() {
        // Avoid transition at the beginning e.g. page loads and enable
        // transitions only after the element is rendered and ready.
        this.transition = true;
      },

      _computeIronSelectorClass: function(narrow, transition, dragging, rightDrawer) {
        return classNames({
          dragging: dragging,
          'narrow-layout': narrow,
          'right-drawer': rightDrawer,
          'left-drawer': !rightDrawer,
          transition: transition
        });
      },

      _computeDrawerStyle: function(drawerWidth) {
        return 'width:' + drawerWidth + ';';
      },

      _computeMainStyle: function(narrow, rightDrawer, drawerWidth) {
        var style = '';

        style += 'left:' + ((narrow || rightDrawer) ? '0' : drawerWidth) + ';';

        if (rightDrawer) {
          style += 'right:' + (narrow ? '' : drawerWidth) + ';';
        } else {
          style += 'right:;';
        }

        return style;
      },

      _computeMediaQuery: function(forceNarrow, responsiveWidth) {
        return forceNarrow ? '' : '(max-width: ' + responsiveWidth + ')';
      },

      _computeSwipeOverlayHidden: function(narrow, disableEdgeSwipe) {
        return !narrow || disableEdgeSwipe;
      },

      _onTrack: function(e) {
        if (sharedPanel && this !== sharedPanel) {
          return;
        }
        switch (e.detail.state) {
          case 'start':
            this._trackStart(e);
            break;
          case 'track':
            this._trackX(e);
            break;
          case 'end':
            this._trackEnd(e);
            break;
        }

      },

      _responsiveChange: function(narrow) {
        this._setNarrow(narrow);

        if (this.narrow) {
          this.selected = this.defaultSelected;
        }

        this.setScrollDirection(this._swipeAllowed() ? 'y' : 'all');
        this.fire('paper-responsive-change', {narrow: this.narrow});
      },

      _onQueryMatchesChanged: function(e) {
        this._responsiveChange(e.detail.value);
      },

      _forceNarrowChanged: function() {
        // set the narrow mode only if we reached the `responsiveWidth`
        this._responsiveChange(this.forceNarrow || this.$.mq.queryMatches);
      },

      _swipeAllowed: function() {
        return this.narrow && !this.disableSwipe;
      },

      _isMainSelected: function() {
        return this.selected === 'main';
      },

      _startEdgePeek: function() {
        this.width = this.$.drawer.offsetWidth;
        this._moveDrawer(this._translateXForDeltaX(this.rightDrawer ?
            -this.edgeSwipeSensitivity : this.edgeSwipeSensitivity));
        this._setPeeking(true);
      },

      _stopEdgePeek: function() {
        if (this.peeking) {
          this._setPeeking(false);
          this._moveDrawer(null);
        }
      },

      _downHandler: function(e) {
        if (!this.dragging && this._isMainSelected() && this._isEdgeTouch(e) && !sharedPanel) {
          this._startEdgePeek();
          // grab this panel
          sharedPanel = this;
        }
      },

      _upHandler: function() {
        this._stopEdgePeek();
        // release the panel
        sharedPanel = null;
      },

      _onTap: function(e) {
        var targetElement = Polymer.dom(e).localTarget;
        var isTargetToggleElement = targetElement &&
          this.drawerToggleAttribute &&
          targetElement.hasAttribute(this.drawerToggleAttribute);

        if (isTargetToggleElement) {
          this.togglePanel();
        }
      },

      _isEdgeTouch: function(e) {
        var x = e.detail.x;

        return !this.disableEdgeSwipe && this._swipeAllowed() &&
          (this.rightDrawer ?
            x >= this.offsetWidth - this.edgeSwipeSensitivity :
            x <= this.edgeSwipeSensitivity);
      },

      _trackStart: function(event) {
        if (this._swipeAllowed()) {
          sharedPanel = this;
          this._setDragging(true);

          if (this._isMainSelected()) {
            this._setDragging(this.peeking || this._isEdgeTouch(event));
          }

          if (this.dragging) {
            this.width = this.$.drawer.offsetWidth;
            this.transition = false;
          }
        }
      },

      _translateXForDeltaX: function(deltaX) {
        var isMain = this._isMainSelected();

        if (this.rightDrawer) {
          return Math.max(0, isMain ? this.width + deltaX : deltaX);
        } else {
          return Math.min(0, isMain ? deltaX - this.width : deltaX);
        }
      },

      _trackX: function(e) {
        if (this.dragging) {
          var dx = e.detail.dx;

          if (this.peeking) {
            if (Math.abs(dx) <= this.edgeSwipeSensitivity) {
              // Ignore trackx until we move past the edge peek.
              return;
            }
            this._setPeeking(false);
          }

          this._moveDrawer(this._translateXForDeltaX(dx));
        }
      },

      _trackEnd: function(e) {
        if (this.dragging) {
          var xDirection = e.detail.dx > 0;

          this._setDragging(false);
          this.transition = true;
          sharedPanel = null;
          this._moveDrawer(null);

          if (this.rightDrawer) {
            this[xDirection ? 'closeDrawer' : 'openDrawer']();
          } else {
            this[xDirection ? 'openDrawer' : 'closeDrawer']();
          }
        }
      },

      _transformForTranslateX: function(translateX) {
        if (translateX === null) {
          return '';
        }

        return this.hasWillChange ? 'translateX(' + translateX + 'px)' :
            'translate3d(' + translateX + 'px, 0, 0)';
      },

      _moveDrawer: function(translateX) {
        var s = this.$.drawer.style;

        if (this.hasTransform) {
          s.transform = this._transformForTranslateX(translateX);
        } else {
          s.webkitTransform = this._transformForTranslateX(translateX);
        }
      }

    });

  }());

