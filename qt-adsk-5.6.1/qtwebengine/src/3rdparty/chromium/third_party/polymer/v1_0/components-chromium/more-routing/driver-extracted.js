
(function(scope) {
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};
MoreRouting.Driver = Driver;

/**
 * TODO(nevir): Docs.
 */
function Driver(opt_config) {
  var config = opt_config || {};
  if (config.prefix) this.prefix = config.prefix;

  this._activeRoutes = [];

  this._rootRoutes = [];
}

Driver.prototype.manageRoute = function manageRoute(route) {
  route.driver = this;
  this._appendRoute(route);

  if (route.parent) {
    if (route.parent.active) {
      // Remember: `processPathParts` takes just the path parts relative to that
      // route; not the full set.
      route.processPathParts(this.currentPathParts.slice(route.parent.depth));
    }
  } else {
    route.processPathParts(this.currentPathParts);
  }

  if (route.active) this._activeRoutes.push(route);
};

Driver.prototype.urlForParts = function urlForParts(parts) {
  return this.prefix + parts.join(this.separator);
};

Driver.prototype.navigateToParts = function(parts) {
  return this.navigateToUrl(this.urlForParts(parts));
};

Driver.prototype.navigateToUrl = function navigateToUrl(url) {
  throw new Error(this.constructor.name + '#navigateToUrl not implemented');
};

// Subclass Interface

Driver.prototype.prefix = '/';
Driver.prototype.separator = '/';

Driver.prototype.setCurrentPath = function setCurrentPath(path) {
  this.currentPathParts = this.splitPath(path);
  var newRoutes = this._matchingRoutes(this.currentPathParts);

  // active -> inactive.
  for (var i = 0, route; route = this._activeRoutes[i]; i++) {
    if (newRoutes.indexOf(route) === -1) {
      route.processPathParts(null);
    }
  }

  this._activeRoutes = newRoutes;
}

Driver.prototype.splitPath = function splitPath(rawPath) {
  if (this.prefix && rawPath.indexOf(this.prefix) !== 0) {
    throw new Error(
        'Invalid path "' + rawPath + '"; ' +
        'expected it to be prefixed by "' + this.prefix + '"');
  }
  var path  = rawPath.substr(this.prefix.length);
  var parts = path.split(this.separator);
  // Ignore trailing separators.
  if (!parts[parts.length - 1]) parts.pop();

  return parts;
};

// Internal Implementation
Driver.prototype._appendRoute = function _appendRoute(route) {
  if (route.parent) {
    // We only care about root routes.
    return;
  }
  this._rootRoutes.push(route);
};

Driver.prototype._matchingRoutes = function _matchingRoutes(parts, rootRoutes) {
  var routes = [];
  var candidates = rootRoutes || this._rootRoutes;
  var route;
  for (var i = 0; i < candidates.length; i++) {
    route = candidates[i];
    route.processPathParts(parts);
    if (route.active) {
      routes.push(route);
      routes = routes.concat(this._matchingRoutes(parts.slice(route.compiled.length), route.children));
    }
  }
  return routes;
}

})(window);
