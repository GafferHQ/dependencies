
(function(scope) {
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};

// Route singletons.
var routesByPath = {};
var pathsByName  = {};

// Route Management

/**
 * Retrieves (or builds) the singleton `Route` for the given path expression or
 * route name.
 *
 * Paths begin with `/`; anything else is considered a name.
 *
 * For convenience, `Route` objects can also be passed (and will be returned) -
 * this can be used as a route coercion function.
 *
 * @param {String|MoreRouting.Route} pathOrName
 * @param {MoreRouting.Route} parent
 * @return {MoreRouting.Route}
 */
MoreRouting.getRoute = function getRoute(pathOrName, parent) {
  if (typeof pathOrName !== 'string') return pathOrName;
  if (this.isPath(pathOrName)) {
    return this.getRouteByPath(pathOrName, parent);
  } else {
    return this.getRouteByName(pathOrName);
  }
}

/**
 * Retrieves (or builds) the singleton `Route` for the given path expression.
 *
 * @param {String} path
 * @param {MoreRouting.Route} parent
 * @return {MoreRouting.Route}
 */
MoreRouting.getRouteByPath = function getRouteByPath(path, parent) {
  var fullPath = (parent ? parent.fullPath : '') + path;
  if (!routesByPath[fullPath]) {
    routesByPath[fullPath] = new this.Route(path, parent);
    this.driver.manageRoute(routesByPath[fullPath]);
  }
  return routesByPath[fullPath];
}

/**
 * Retrieves the route registered via `name`.
 *
 * @param {String} name
 * @return {MoreRouting.Route}
 */
MoreRouting.getRouteByName = function getRouteByName(name) {
  var path = pathsByName[name];
  if (!path) {
    throw new Error('No route named "' + name + '" has been registered');
  }
  return this.getRouteByPath(path);
}

/**
 * @param {String} path
 * @return {MoreRouting.Route} The newly registered route.
 */
MoreRouting.registerNamedRoute = function registerNamedRoute(name, path, parent) {
  if (pathsByName[name]) {
    console.warn(
        'Overwriting route named "' + name + '" with path:', path,
        'previously:', pathsByName[name]);
  }
  var route = this.getRouteByPath(path, parent);
  pathsByName[name] = route.fullPath;
  return route;
};

// Route Shortcuts
MoreRouting.urlFor = function urlFor(pathOrName, params) {
  return this.getRoute(pathOrName).urlFor(params);
};

MoreRouting.navigateTo = function navigateTo(pathOrName, params) {
  return this.getRoute(pathOrName).navigateTo(params);
};

MoreRouting.isCurrentUrl = function isCurrentUrl(pathOrName, params) {
  return this.getRoute(pathOrName).isCurrentUrl(params);
};

// Utility

/**
 *
 */
MoreRouting.isPath = function isPath(pathOrName) {
  return this.Route.isPath(pathOrName);
}

/**
 * @param {...String} paths
 */
MoreRouting.joinPath = function joinPath(paths) {
  return this.Route.joinPath.apply(this.Route, arguments);
}

// Driver Management

var driver;
Object.defineProperty(MoreRouting, 'driver', {
  get: function getDriver() {
    if (!driver) {
      throw new Error('No routing driver configured. Did you forget <more-routing-config>?');
    }
    return driver;
  },
  set: function setDriver(newDriver) {
    if (driver) {
      console.warn('Changing routing drivers is not supported, ignoring. You should have only one <more-routing-config> on the page!');
      return;
    }
    driver = newDriver;
  }
});

})(window);
