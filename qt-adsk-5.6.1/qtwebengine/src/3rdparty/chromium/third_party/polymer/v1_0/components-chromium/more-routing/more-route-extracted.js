

  Polymer({

    is: 'more-route',

    behaviors: [
      MoreRouting.ContextAware,
    ],

    properties: {

      /**
       * The name of this route. Behavior differs based on the presence of
       * `path` during _declaration_.
       *
       * If `path` is present during declaration, it is registered via `name`.
       *
       * Otherwise, this `more-route` becomes a `reference` to the route with
       * `name`. Changing `name` will update which route is referenced.
       */
      name: {
        type:     String,
        observer: '_nameChanged',
      },

      /**
       * A path expression used to parse parameters from the window's URL.
       */
      path: {
        type:     String,
        obserer: '_pathChanged',
      },

      /**
       * Whether this route should become a context for the element that
       * contains it.
       */
      context: {
        type: Boolean,
      },

      /**
       * The underlying `MoreRouting.Route` object that is being wrapped.
       *
       * @type {MoreRouting.Route}
       */
      route: {
        type:     Object,
        readOnly: true,
        notify:   true,
        observer: '_routeChanged',
      },

      /**
       * The full path expression for this route, including routes this is
       * nested within.
       */
      fullPath: {
        type:     String,
        readOnly: true,
        notify:   true,
      },

      /**
       * Param values matching the current URL, or an empty object if not
       * `active`.
       */
      params: {
        type:     Object,
        // readOnly: true,
        notify:   true,
      },

      /**
       * Whether the route matches the current URL.
       */
      active: {
        type:     Boolean,
        readOnly: true,
        notify:   true,
      },

    },

    routingReady: function() {
      this._identityChanged();
    },

    _nameChanged: function(newName, oldName) {
      if (oldName) {
        console.error('Changing the `name` property is not supported for', this);
        return;
      }
      this._identityChanged();
    },

    _pathChanged: function(newPath, oldPath) {
      if (oldPath) {
        console.error('Changing the `path` property is not supported for', this);
        return;
      }
      this._identityChanged();
    },

    _identityChanged: function() {
      if (!this.routingIsReady) return;

      if (this.name && this.path) {
        this._setRoute(MoreRouting.registerNamedRoute(this.name, this.path, this.parentRoute));
      } else if (this.name) {
        this._setRoute(MoreRouting.getRouteByName(this.name));
      } else if (this.path) {
        this._setRoute(MoreRouting.getRouteByPath(this.path, this.parentRoute));
      } else {
        this._setRoute(null);
      }
    },

    _routeChanged: function() {
      this._observeRoute();
      this._setFullPath(this.route.fullPath);
      // this._setParams(this.route.params);
      this.params = this.route.params;
      this._setActive(this.route.active);

      // @see MoreRouting.ContextAware
      this.moreRouteContext = this.route;

      if (this.context) {
        var parent = Polymer.dom(this).parentNode;
        if (parent.nodeType !== Node.ELEMENT_NODE) {
          parent = parent.host;
        }

        if (parent.nodeType === Node.ELEMENT_NODE) {
          parent.moreRouteContext = this.route;
        } else {
          console.warn('Unable to determine parent element for', this, '- not setting a context');
        }
      }
    },

    _observeRoute: function() {
      // TODO(nevir) https://github.com/Polymore/more-routing/issues/24
      if (this._routeListener) {
        this._routeListener.close();
        this._routeListener = null;
      }
      if (this._paramListener) {
        this._paramListener.close();
        this._paramListener = null;
      }
      if (!this.route) return;

      this._routeListener = this.route.__subscribe(function() {
        this._setActive(this.route && this.route.active);
      }.bind(this));

      this._paramListener = this.route.params.__subscribe(function(key, value) {
        // https://github.com/Polymer/polymer/issues/1505
        this.notifyPath('params.' + key, value);
      }.bind(this));
    },

    urlFor: function(params) {
      return this.route.urlFor(params);
    },

    navigateTo: function(params) {
      return this.route.navigateTo(params);
    },

    isCurrentUrl: function(params) {
      return this.route.isCurrentUrl(params);
    },

  });

