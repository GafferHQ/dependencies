

  Polymer({

    is: 'iron-iconset',

    properties: {

      /**
       * The URL of the iconset image.
       *
       * @attribute src
       * @type string
       * @default ''
       */
      src: {
        type: String,
        observer: '_srcChanged'
      },

      /**
       * The name of the iconset.
       *
       * @attribute name
       * @type string
       * @default 'no-name'
       */
      name: {
        type: String,
        observer: '_nameChanged'
      },

      /**
       * The width of the iconset image. This must only be specified if the
       * icons are arranged into separate rows inside the image.
       *
       * @attribute width
       * @type number
       * @default 0
       */
      width: {
        type: Number,
        value: 0
      },

      /**
       * A space separated list of names corresponding to icons in the iconset
       * image file. This list must be ordered the same as the icon images
       * in the image file.
       *
       * @attribute icons
       * @type string
       * @default ''
       */
      icons: {
        type: String
      },

      /**
       * The size of an individual icon. Note that icons must be square.
       *
       * @attribute size
       * @type number
       * @default 24
       */
      size: {
        type: Number,
        value: 24
      },

      /**
       * The horizontal offset of the icon images in the inconset src image.
       * This is typically used if the image resource contains additional images
       * beside those intended for the iconset.
       *
       * @attribute offset-x
       * @type number
       * @default 0
       */
      _offsetX: {
        type: Number,
        value: 0
      },

      /**
       * The vertical offset of the icon images in the inconset src image.
       * This is typically used if the image resource contains additional images
       * beside those intended for the iconset.
       *
       * @attribute offset-y
       * @type number
       * @default 0
       */
      _offsetY: {
        type: Number,
        value: 0
      },

      /**
       * Array of fully-qualified names of icons in this set.
       */
      iconNames: {
        type: Array,
        notify: true
      }

    },

    hostAttributes: {
      // non-visual
      style: 'display: none;'
    },

    ready: function() {
      // theme data must exist at ready-time
      this._themes = this._mapThemes();
    },

    /**
     * Applies an icon to the given element as a css background image. This
     * method does not size the element, and it's usually necessary to set
     * the element's height and width so that the background image is visible.
     *
     * @method applyIcon
     * @param {Element} element The element to which the icon is applied.
     * @param {String|Number} icon The name or index of the icon to apply.
     * @param {String} theme (optional) The name or index of the icon to apply.
     * @param {Number} scale (optional, defaults to 1) Icon scaling factor.
     */
    applyIcon: function(element, icon, theme, scale) {
      this._validateIconMap();
      var offset = this._getThemedOffset(icon, theme);
      if (element && offset) {
        this._addIconStyles(element, this._srcUrl, offset, scale || 1,
          this.size, this.width);
      }
    },

    /**
     * Remove an icon from the given element by undoing the changes effected
     * by `applyIcon`.
     *
     * @param {Element} element The element from which the icon is removed.
     */
    removeIcon: function(element) {
      this._removeIconStyles(element.style);
    },

    _mapThemes: function() {
      var themes = Object.create(null);
      Polymer.dom(this).querySelectorAll('property[theme]')
        .forEach(function(property) {
          var offsetX = window.parseInt(
            property.getAttribute('offset-x'), 10
          ) || 0;
          var offsetY = window.parseInt(
            property.getAttribute('offset-y'), 10
          ) || 0;
          themes[property.getAttribute('theme')] = {
            offsetX: offsetX,
            offsetY: offsetY
          };
        });
      return themes;
    },

    _srcChanged: function(src) {
      // ensure `srcUrl` is always relative to the main document
      this._srcUrl = this.ownerDocument !== document
        ? this.resolveUrl(src) : src;
      this._prepareIconset();
    },

    _nameChanged: function(name) {
      this._prepareIconset();
    },

    _prepareIconset: function() {
      new Polymer.IronMeta({type: 'iconset', key: this.name, value: this});
    },

    _invalidateIconMap: function() {
      this._iconMapValid = false;
    },

    _validateIconMap: function() {
      if (!this._iconMapValid) {
        this._recomputeIconMap();
        this._iconMapValid = true;
      }
    },

    _recomputeIconMap: function() {
      this.iconNames = this._computeIconNames(this.icons);
      this.iconMap = this._computeIconMap(this._offsetX, this._offsetY,
        this.size, this.width, this.iconNames);
    },

    _computeIconNames: function(icons) {
      return icons.split(/\s+/g);
    },

    _computeIconMap: function(offsetX, offsetY, size, width, iconNames) {
      var iconMap = {};
      if (offsetX !== undefined && offsetY !== undefined) {
        var x0 = offsetX;
        iconNames.forEach(function(iconName) {
          iconMap[iconName] = {
            offsetX: offsetX,
            offsetY: offsetY
          };
          if ((offsetX + size) < width) {
            offsetX += size;
          } else {
            offsetX = x0;
            offsetY += size;
          }
        }, this);
      }
      return iconMap;
    },

    /**
     * Returns an object containing `offsetX` and `offsetY` properties which
     * specify the pixel location in the iconset's src file for the given
     * `icon` and `theme`. It's uncommon to call this method. It is useful,
     * for example, to manually position a css backgroundImage to the proper
     * offset. It's more common to use the `applyIcon` method.
     *
     * @method getThemedOffset
     * @param {String|Number} identifier The name of the icon or the index of
     * the icon within in the icon image.
     * @param {String} theme The name of the theme.
     * @returns {Object} An object specifying the offset of the given icon
     * within the icon resource file; `offsetX` is the horizontal offset and
     * `offsetY` is the vertical offset. Both values are in pixel units.
     */
    _getThemedOffset: function(identifier, theme) {
      var iconOffset = this._getIconOffset(identifier);
      var themeOffset = this._themes[theme];
      if (iconOffset && themeOffset) {
        return {
          offsetX: iconOffset.offsetX + themeOffset.offsetX,
          offsetY: iconOffset.offsetY + themeOffset.offsetY
        };
      }
      return iconOffset;
    },

    _getIconOffset: function(identifier) {
      // TODO(sjmiles): consider creating offsetArray (indexed by Number)
      // and having iconMap map names to indices, then and index is just
      // iconMap[identifier] || identifier (be careful of zero, store indices
      // as 1-based)
      return this.iconMap[identifier] ||
             this.iconMap[this.iconNames[Number(identifier)]];
    },

    _addIconStyles: function(element, url, offset, scale, size, width) {
      var style = element.style;
      style.backgroundImage = 'url(' + url + ')';
      style.backgroundPosition =
        (-offset.offsetX * scale + 'px') + ' ' +
        (-offset.offsetY * scale + 'px');
      style.backgroundSize = (scale === 1) ? 'auto' : width * scale + 'px';
      style.width = size + 'px';
      style.height = size + 'px';
      element.setAttribute('role', 'img');
    },

    _removeIconStyles: function(style) {
      style.background = '';
    }

  });

