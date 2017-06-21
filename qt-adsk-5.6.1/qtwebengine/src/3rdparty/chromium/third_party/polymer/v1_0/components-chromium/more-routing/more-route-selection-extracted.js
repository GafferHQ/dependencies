

  Polymer({

    is: 'more-route-selection',

    behaviors: [
      MoreRouting.ContextAware,
    ],

    properties: {

      /**
       * Routes to select from, as either a path expression or route name.
       *
       * You can either specify routes via this attribute, or as child nodes
       * to this element, but not both.
       *
       * @type {String|Array<string|MoreRouting.Route>}
       */
      routes: {
        type:     String,
        observer: '_routesChanged',
      },

      /**
       * The selected `MoreRouting.Route` object, or `null`.
       *
       * @type {MoreRouting.Route}
       */
      selectedRoute: {
        type:     Object,
        value:    null,
        readOnly: true,
        notify:   true,
      },

      /**
       * The index of the selected route (relative to `routes`). -1 when there
       * is no active route.
       */
      selectedIndex: {
        type:     Number,
        value:    -1,
        readOnly: true,
        notify:   true,
      },

      /**
       * The _full_ path expression of the selected route, or `null`.
       */
      selectedPath: {
        type:     String,
        readOnly: true,
        notify:   true,
      },

      /**
       * The params of the selected route, or an empty object if no route.
       */
      selectedParams: {
        type:     Object,
        readOnly: true,
        notify:   true,
      },

    },

    /**
     * @event more-route-change fires when a new route is selected.
     * @detail {{
     *   newRoute:  MoreRouting.Route, oldRoute: MoreRouting.Route,
     *   newIndex:  number,  oldIndex:  number,
     *   newPath:   ?string, oldPath:   ?string,
     *   newParams: Object,  oldParams: Object,
     * }}
     */

    routingReady: function() {
      this._routesChanged();
    },

    _routesChanged: function() {
      if (!this.routingIsReady) return;
      var routes = this.routes || [];
      if (typeof routes === 'string') {
        routes = routes.split(/\s+/);
      }
      this._routeInfo = this._sortIndexes(routes.map(function(route, index) {
        return {
          model: MoreRouting.getRoute(route, this.parentRoute),
          index: index,
        };
      }.bind(this)));

      this._observeRoutes();
      this._evaluate();
    },

    /**
     * Tracks changes to the routes.
     */
    _observeRoutes: function() {
      if (this._routeListeners) {
        for (var i = 0, listener; listener = this._routeListeners[i]; i++) {
          listener.close();
        }
      }

      this._routeListeners = this._routeInfo.map(function(routeInfo) {
        return routeInfo.model.__subscribe(this._evaluate.bind(this));
      }.bind(this));
    },

    _evaluate: function() {
      var newIndex = -1;
      var newRoute = null;
      var oldIndex = this.selectedIndex;

      for (var i = 0, routeInfo; routeInfo = this._routeInfo[i]; i++) {
        if (routeInfo.model && routeInfo.model.active) {
          newIndex = routeInfo.index;
          newRoute = routeInfo.model;
          break;
        }
      }
      if (newIndex === oldIndex) return;

      var oldRoute  = this.selectedRoute;
      var oldPath   = this.selectedPath;
      var oldParams = this.selectedParams;

      var newPath   = newRoute ? newRoute.fullPath : null;
      var newParams = newRoute ? newRoute.params   : {};

      this._setSelectedRoute(newRoute);
      this._setSelectedIndex(newIndex);
      this._setSelectedPath(newPath);
      this._setSelectedParams(newParams);

      this.fire('more-route-change', {
        newRoute:  newRoute,  oldRoute:  oldRoute,
        newIndex:  newIndex,  oldIndex:  oldIndex,
        newPath:   newPath,   oldPath:   oldPath,
        newParams: newParams, oldParams: oldParams,
      });
    },
    /**
     * We want the most specific routes to match first, so we must create a
     * mapping of indexes within `routes` that map
     */
    _sortIndexes: function(routeInfo) {
      return routeInfo.sort(function(a, b) {
        if (!a.model) {
          return 1;
        } else if (!b.model) {
          return -1;
        // Routes with more path parts are most definitely more specific.
        } else if (a.model.depth < b.model.depth) {
          return 1;
        } if (a.model.depth > b.model.depth) {
          return -1;
        } else {

          // Also, routes with fewer params are more specific. For example
          // `/users/foo` is more specific than `/users/:id`.
          if (a.model.numParams < b.model.numParams) {
            return -1;
          } else if (a.model.numParams > b.model.numParams) {
            return 1;
          } else {
            // Equally specific; we fall back to the default (and hopefully
            // stable) sort order.
            return 0;
          }
        }
      });
    },

  });
