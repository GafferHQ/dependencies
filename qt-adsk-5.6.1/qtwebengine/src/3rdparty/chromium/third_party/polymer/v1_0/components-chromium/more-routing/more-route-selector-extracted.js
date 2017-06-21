
Polymer({

  is: 'more-route-selector',

  behaviors: [
    MoreRouting.ContextAware,
  ],

  properties: {

    /**
     * The attribute to read route expressions from (on children).
     */
    routeAttribute: {
      type: String,
      value: 'route',
    },

    /**
     * The routes managed by this element (inferred from the items that are
     * defined by the selector it targets).
     */
    routes: {
      type:     Array,
      readOnly: true,
      notify:   true,
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
   * @event more-route-selected fires when a new route is selected.
   * @param {{
   *   newRoute:  MoreRouting.Route, oldRoute: MoreRouting.Route,
   *   newIndex:  number,  oldIndex:  number,
   *   newPath:   ?string, oldPath:   ?string,
   *   newParams: Object,  oldParams: Object,
   * }}
   */

  attached: function() {
    this._managedSelector = this._findTargetSelector();
    if (!this._managedSelector) {
      console.warn(this, 'was built without a selector to manage. It will do nothing.');
      return;
    }

    this._managedSelector.addEventListener(
        'selected-item-changed', this._onSelectedItemChanged.bind(this));
    this._updateRoutes();
  },

  /**
   * Handle a change in selected item, driven by the targeted selector.
   *
   * Note that this will fail if a route is chosen that requires params not
   * defined by the current URL.
   */
  _onSelectedItemChanged: function(event) {
    if (this._settingSelection) return;
    var route = this._routeForItem(event.detail.value);
    if (!route) return;
    route.navigateTo();
  },

  _updateRoutes: function() {
    var routes = [];
    if (this._managedSelector) {
      routes = this._managedSelector.items.map(this._routeForItem.bind(this));
    }
    this._setRoutes(routes);
  },

  _onMoreRouteChange: function(event) {
    if (!this._managedSelector) return;

    var selected = '';

    var index = this.routes.indexOf(event.detail.newRoute);
    var attrForSelected = this._managedSelector.attrForSelected;
    if (!attrForSelected) {
      selected = index;
    } else {
      var item = this._managedSelector.items[index];
      if (item)
        selected = item[attrForSelected] || item.getAttribute(attrForSelected);
    }

    // Make sure that we don't turn around and re-navigate
    this._settingSelection = true;
    this._managedSelector.select(selected);
    this._settingSelection = false;
  },

  _findTargetSelector: function() {
    var children = Polymer.dom(this).children;
    if (children.length !== 1) {
      console.error(this, 'expects only a single selector child');
      return null;
    }

    var child = children[0];
    if ('selected' in child && 'items' in child) {
      return child;
    } else {
      console.error(this, 'can only manage children that are selectors');
      return null;
    }
  },

  _routeForItem: function(item) {
    if (!item) return null;
    if (item.moreRouteContext && item.moreRouteContext instanceof MoreRouting.Route) {
      return item.moreRouteContext;
    }

    if (!item.hasAttribute(this.routeAttribute)) {
      console.warn(item, 'is missing a context route or "' + this.routeAttribute + '" attribute');
      return null;
    }
    var expression = item.getAttribute(this.routeAttribute);
    var route      = MoreRouting.getRoute(expression, this.parentRoute);
    // Associate the route w/ its element while we're here.
    item.moreRouteContext = route;

    return route;
  },

});
