// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="../../assert.js">

cr.exportPath('cr.ui');

/**
 * Enum for type of hide. Delayed is used when called by clicking on a
 * checkable menu item.
 * @enum {number}
 */
cr.ui.HideType = {
  INSTANT: 0,
  DELAYED: 1
};

cr.define('cr.ui', function() {
  /** @const */
  var Menu = cr.ui.Menu;

  /** @const */
  var HideType = cr.ui.HideType;

  /** @const */
  var positionPopupAroundElement = cr.ui.positionPopupAroundElement;

  /**
   * Creates a new menu button element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLButtonElement}
   * @implements {EventListener}
   */
  var MenuButton = cr.ui.define('button');

  MenuButton.prototype = {
    __proto__: HTMLButtonElement.prototype,

    /**
     * Initializes the menu button.
     */
    decorate: function() {
      this.addEventListener('mousedown', this);
      this.addEventListener('keydown', this);
      this.addEventListener('dblclick', this);

      // Adding the 'custom-appearance' class prevents widgets.css from changing
      // the appearance of this element.
      this.classList.add('custom-appearance');
      this.classList.add('menu-button');  // For styles in menu_button.css.

      var menu;
      if ((menu = this.getAttribute('menu')))
        this.menu = menu;

      // An event tracker for events we only connect to while the menu is
      // displayed.
      this.showingEvents_ = new EventTracker();

      this.anchorType = cr.ui.AnchorType.BELOW;
      this.invertLeftRight = false;
    },

    /**
     * The menu associated with the menu button.
     * @type {cr.ui.Menu}
     */
    get menu() {
      return this.menu_;
    },
    set menu(menu) {
      if (typeof menu == 'string' && menu[0] == '#') {
        menu = assert(this.ownerDocument.getElementById(menu.slice(1)));
        cr.ui.decorate(menu, Menu);
      }

      this.menu_ = menu;
      if (menu) {
        if (menu.id)
          this.setAttribute('menu', '#' + menu.id);
      }
    },

    /**
     * Whether to show the menu on press of the Up or Down arrow keys.
     */
    respondToArrowKeys: true,

    /**
     * Handles event callbacks.
     * @param {Event} e The event object.
     */
    handleEvent: function(e) {
      if (!this.menu)
        return;

      switch (e.type) {
        case 'mousedown':
          if (e.currentTarget == this.ownerDocument) {
            if (e.target instanceof Node && !this.contains(e.target) &&
                !this.menu.contains(e.target)) {
              this.hideMenu();
            } else {
              e.preventDefault();
            }
          } else {
            if (this.isMenuShown()) {
              this.hideMenu();
            } else if (e.button == 0) {  // Only show the menu when using left
                                         // mouse button.
              this.showMenu(false);

              // Prevent the button from stealing focus on mousedown.
              e.preventDefault();
            }
          }

          // Hide the focus ring on mouse click.
          this.classList.add('using-mouse');
          break;
        case 'keydown':
          this.handleKeyDown(e);
          // If the menu is visible we let it handle all the keyboard events.
          if (this.isMenuShown() && e.currentTarget == this.ownerDocument) {
            this.menu.handleKeyDown(e);
            e.preventDefault();
            e.stopPropagation();
          }

          // Show the focus ring on keypress.
          this.classList.remove('using-mouse');
          break;
        case 'focus':
          if (e.target instanceof Node && !this.contains(e.target) &&
              !this.menu.contains(e.target)) {
            this.hideMenu();
            // Show the focus ring on focus - if it's come from a mouse event,
            // the focus ring will be hidden in the mousedown event handler,
            // executed after this.
            this.classList.remove('using-mouse');
          }
          break;
        case 'activate':
          var hideDelayed = e.target instanceof cr.ui.MenuItem &&
              e.target.checkable;
          this.hideMenu(hideDelayed ? HideType.DELAYED : HideType.INSTANT);
          break;
        case 'scroll':
          if (!(e.target == this.menu || this.menu.contains(e.target)))
            this.hideMenu();
          break;
        case 'popstate':
        case 'resize':
          this.hideMenu();
          break;
        case 'contextmenu':
          if ((!this.menu || !this.menu.contains(e.target)) &&
              (!this.hideTimestamp_ || Date.now() - this.hideTimestamp_ > 50))
            this.showMenu(true);
          e.preventDefault();
          // Don't allow elements further up in the DOM to show their menus.
          e.stopPropagation();
          break;
        case 'dblclick':
          // Don't allow double click events to propagate.
          e.preventDefault();
          e.stopPropagation();
          break;
      }
    },

    /**
     * Shows the menu.
     * @param {boolean} shouldSetFocus Whether to set focus on the
     *     selected menu item.
     */
    showMenu: function(shouldSetFocus) {
      this.hideMenu();

      this.menu.updateCommands(this);

      var event = new UIEvent('menushow',{
        bubbles: true,
        cancelable: true,
        view: window
      });
      if (!this.dispatchEvent(event))
        return;

      this.menu.hidden = false;

      this.setAttribute('menu-shown', '');

      // When the menu is shown we steal all keyboard events.
      var doc = this.ownerDocument;
      var win = doc.defaultView;
      this.showingEvents_.add(doc, 'keydown', this, true);
      this.showingEvents_.add(doc, 'mousedown', this, true);
      this.showingEvents_.add(doc, 'focus', this, true);
      this.showingEvents_.add(doc, 'scroll', this, true);
      this.showingEvents_.add(win, 'popstate', this);
      this.showingEvents_.add(win, 'resize', this);
      this.showingEvents_.add(this.menu, 'contextmenu', this);
      this.showingEvents_.add(this.menu, 'activate', this);
      this.positionMenu_();

      if (shouldSetFocus)
        this.menu.focusSelectedItem();
    },

    /**
     * Hides the menu. If your menu can go out of scope, make sure to call this
     * first.
     * @param {cr.ui.HideType=} opt_hideType Type of hide.
     *     default: cr.ui.HideType.INSTANT.
     */
    hideMenu: function(opt_hideType) {
      if (!this.isMenuShown())
        return;

      this.removeAttribute('menu-shown');
      if (opt_hideType == HideType.DELAYED)
        this.menu.classList.add('hide-delayed');
      else
        this.menu.classList.remove('hide-delayed');
      this.menu.hidden = true;

      this.showingEvents_.removeAll();
      this.focus();

      var event = new UIEvent('menuhide', {
        bubbles: true,
        cancelable: false,
        view: window
      });
      this.dispatchEvent(event);

      // On windows we might hide the menu in a right mouse button up and if
      // that is the case we wait some short period before we allow the menu
      // to be shown again.
      this.hideTimestamp_ = cr.isWindows ? Date.now() : 0;
    },

    /**
     * Whether the menu is shown.
     */
    isMenuShown: function() {
      return this.hasAttribute('menu-shown');
    },

    /**
     * Positions the menu below the menu button. At this point we do not use any
     * advanced positioning logic to ensure the menu fits in the viewport.
     * @private
     */
    positionMenu_: function() {
      positionPopupAroundElement(this, this.menu, this.anchorType,
                                 this.invertLeftRight);
    },

    /**
     * Handles the keydown event for the menu button.
     */
    handleKeyDown: function(e) {
      switch (e.keyIdentifier) {
        case 'Down':
        case 'Up':
          if (!this.respondToArrowKeys)
            break;
        case 'Enter':
        case 'U+0020': // Space
          if (!this.isMenuShown())
            this.showMenu(true);
          e.preventDefault();
          break;
        case 'Esc':
        case 'U+001B': // Maybe this is remote desktop playing a prank?
        case 'U+0009': // Tab
          this.hideMenu();
          break;
      }
    }
  };

  /**
   * Helper for styling a menu button with a drop-down arrow indicator.
   * Creates a new 2D canvas context and draws a downward-facing arrow into it.
   * @param {string} canvasName The name of the canvas. The canvas can be
   *     addressed from CSS using -webkit-canvas(<canvasName>).
   * @param {number} width The width of the canvas and the arrow.
   * @param {number} height The height of the canvas and the arrow.
   * @param {string} colorSpec The CSS color to use when drawing the arrow.
   */
  function createDropDownArrowCanvas(canvasName, width, height, colorSpec) {
    var ctx = document.getCSSCanvasContext('2d', canvasName, width, height);
    ctx.fillStyle = ctx.strokeStyle = colorSpec;
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(width, 0);
    ctx.lineTo(height, height);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();
  };

  /** @const */ var ARROW_WIDTH = 6;
  /** @const */ var ARROW_HEIGHT = 3;

  /**
   * Create the images used to style drop-down-style MenuButtons.
   * This should be called before creating any MenuButtons that will have the
   * CSS class 'drop-down'. If no colors are specified, defaults will be used.
   * @param {string=} opt_normalColor CSS color for the default button state.
   * @param {string=} opt_hoverColor CSS color for the hover button state.
   * @param {string=} opt_activeColor CSS color for the active button state.
   */
  MenuButton.createDropDownArrows = function(
      opt_normalColor, opt_hoverColor, opt_activeColor) {
    opt_normalColor = opt_normalColor || 'rgb(192, 195, 198)';
    opt_hoverColor = opt_hoverColor || 'rgb(48, 57, 66)';
    opt_activeColor = opt_activeColor || 'white';

    createDropDownArrowCanvas(
        'drop-down-arrow', ARROW_WIDTH, ARROW_HEIGHT, opt_normalColor);
    createDropDownArrowCanvas(
        'drop-down-arrow-hover', ARROW_WIDTH, ARROW_HEIGHT, opt_hoverColor);
    createDropDownArrowCanvas(
        'drop-down-arrow-active', ARROW_WIDTH, ARROW_HEIGHT, opt_activeColor);
  };

  // Export
  return {
    MenuButton: MenuButton,
  };
});
