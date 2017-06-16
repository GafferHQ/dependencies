
(function(scope) {
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};
MoreRouting.PathDriver = PathDriver;

/**
 * TODO(nevir): Docs.
 */
function PathDriver() {
  MoreRouting.Driver.apply(this, arguments);
  this._bindEvents();
  this._read();
}
PathDriver.prototype = Object.create(MoreRouting.Driver.prototype);

PathDriver.prototype.navigateToUrl = function navigateToUrl(url) {
  window.history.pushState({}, '', url);
  this._read();
};

PathDriver.prototype._bindEvents = function _bindEvents() {
  window.addEventListener('popstate', this._read.bind(this));
};

PathDriver.prototype._read = function _read() {
  this.setCurrentPath(window.location.pathname);
};

})(window);
