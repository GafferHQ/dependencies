
(function(scope) {
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};
MoreRouting.Route = Route;

// Note that this can differ from the part separator defined by the driver. The
// driver's separator is used when parsing/generating URLs given to the client,
// whereas this one is for route definitions.
var PART_SEPARATOR    = '/';
var PARAM_SENTINEL    = ':';
var SEPARATOR_CLEANER = /\/\/+/g;

/**
 * TODO(nevir): Docs.
 */
function Route(path, parent) {
  // For `MoreRouting.Emitter`; Emits changes for `active`.
  this.__listeners = [];

  this.path     = path;
  this.parent   = parent;
  this.fullPath = path;
  this.compiled = this._compile(this.path);
  this.active   = false;
  this.driver   = null;

  var params = MoreRouting.Params(namedParams(this.compiled), this.parent && this.parent.params);
  params.__subscribe(this._navigateToParams.bind(this));
  Object.defineProperty(this, 'params', {
    get: function() { return params; },
    set: function() { throw new Error('Route#params cannot be overwritten'); },
  });

  this.parts    = [];
  this.children = [];

  // Param values matching the current URL, or an empty object if not `active`.
  //
  // To make data "binding" easy, `Route` guarantees that `params` will always
  // be the same object; just make a reference to it.
  if (this.parent) {
    this.parent.children.push(this);
    this.fullPath  = this.parent.fullPath + this.fullPath;
    this.depth     = this.parent.depth + this.compiled.length;
    this.numParams = this.parent.numParams + countParams(this.compiled);
  } else {
    this.depth     = this.compiled.length;
    this.numParams = countParams(this.compiled);
  }
}
Route.prototype = Object.create(MoreRouting.Emitter);

Object.defineProperty(Route.prototype, 'active', {
  get: function() {
    return this._active;
  },
  set: function(value) {
    if (value !== this._active);
    this._active = value;
    this.__notify('active', value);
  },
});

Route.isPath = function isPath(pathOrName) {
  return pathOrName.indexOf(PART_SEPARATOR) === 0;
};

Route.joinPath = function joinPath(paths) {
  var joined = Array.prototype.join.call(arguments, PART_SEPARATOR);
  joined = joined.replace(SEPARATOR_CLEANER, PART_SEPARATOR);

  var minLength = joined.length - PART_SEPARATOR.length;
  if (joined.substr(minLength) === PART_SEPARATOR) {
    joined = joined.substr(0, minLength);
  }

  return joined;
};

Route.prototype.urlFor = function urlFor(params) {
  return this.driver.urlForParts(this.partsForParams(params));
};

Route.prototype.navigateTo = function navigateTo(params) {
  return this.driver.navigateToParts(this.partsForParams(params));
}

Route.prototype.isCurrentUrl = function isCurrentUrl(params) {
  if (!this.active) return false;
  var currentKeys = Object.keys(this.params);
  for (var i = 0, key; key = currentKeys[i]; i++) {
    if (this.params[key] !== String(params[key])) {
      return false;
    }
  }
  return true;
};

// Driver Interface

Route.prototype.partsForParams = function partsForParams(params, silent) {
  var parts = this.parent && this.parent.partsForParams(params, silent) || [];
  for (var i = 0, config; config = this.compiled[i]; i++) {
    if (config.type === 'static') {
      parts.push(config.part);
    } else if (config.type === 'param') {
      var value
      if (params && config.name in params) {
        value = params[config.name];
      } else {
        value = this.params[config.name];
      }
      if (value === undefined) {
        if (silent) {
          return null;
        } else {
          throw new Error('Missing param "' + config.name + '" for route ' + this);
        }
      }
      parts.push(value);
    }
  }
  return parts;
};

/**
 * Called by the driver whenever it has detected a change to the URL.
 *
 * @param {Array.<String>|null} parts The parts of the URL, or null if the
 *     route should be disabled.
 */
Route.prototype.processPathParts = function processPathParts(parts) {
  this.parts  = parts;
  this.active = this.matchesPathParts(parts);

  // We don't want to notify of these changes; they'd be no-op noise.
  this.params.__silent = true;

  if (this.active) {
    var keys = Object.keys(this.params);
    for (var i = 0; i < keys.length; i++) {
      delete this.params[keys[i]];
    }
    for (var i = 0, config; config = this.compiled[i]; i++) {
      if (config.type === 'param') {
        this.params[config.name] = parts[i];
      }
    }
  } else {
    for (key in this.params) {
      this.params[key] = undefined;
    }
  }

  delete this.params.__silent;
};

Route.prototype.matchesPathParts = function matchesPathParts(parts) {
  if (!parts) return false;
  if (parts.length < this.compiled.length) return false;
  for (var i = 0, config; config = this.compiled[i]; i++) {
    if (config.type === 'static' && parts[i] !== config.part) {
      return false;
    }
  }
  return true;
};

Route.prototype.toString = function toString() {
  return this.path;
};

// Internal Implementation

Route.prototype._compile = function _compile(rawPath) {
  // Not strictly required, but helps us stay consistent w/ `getRoute`, etc.
  if (rawPath.indexOf(PART_SEPARATOR) !== 0) {
    throw new Error('Route paths must begin with a path separator; got: "' + rawPath + '"');
  }
  var path = rawPath.substr(PART_SEPARATOR.length);
  if (path === '') return [];

  return path.split(PART_SEPARATOR).map(function(part) {
    // raw fragment.
    if (part.substr(0, 1) == PARAM_SENTINEL) {
      return {type: 'param', name: part.substr(1)};
    } else {
      return {type: 'static', part: part};
    }
  });
};

Route.prototype._navigateToParams = function _navigateToParams() {
  var parts = this.partsForParams(this.params, true);
  if (!parts) return;
  this.driver.navigateToParts(parts);
};

function countParams(compiled) {
  return compiled.reduce(function(count, part) {
    return count + (part.type === 'param' ? 1 : 0);
  }, 0);
}

function namedParams(compiled) {
  var result = [];
  compiled.forEach(function(part) {
    if (part.type === 'static') return;
    result.push(part.name);
  });
  return result;
}

})(window);
