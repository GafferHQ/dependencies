
(function(scope) {
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};
MoreRouting.MockDriver = MockDriver;

/** A mock driver for use in your tests. */
function MockDriver() {
  MoreRouting.Driver.apply(this, arguments);
}
MockDriver.prototype = Object.create(MoreRouting.Driver.prototype);

MockDriver.prototype.navigateToUrl = function navigateToUrl(url) {
  this.setCurrentPath(url);
};

})(window);
