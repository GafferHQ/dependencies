
(function(scope) {
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};
MoreRouting.Params = Params;

/**
 * A collection of route parameters and their values, with nofications.
 *
 * Params prefixed by `__` are reserved.
 *
 * @param {!Array<string>} params The keys of the params being managed.
 * @param {Params=} parent A parent route's params to inherit.
 * @return {Params}
 */
function Params(params, parent) {
  var model = Object.create(parent || Params.prototype);
  // We have a different set of listeners at every level of the hierarchy.
  Object.defineProperty(model, '__listeners', {value: []});

  // We keep all state enclosed within this closure so that inheritance stays
  // relatively straightfoward.
  var state = {};
  _compile(model, params, state);

  return model;
}
Params.prototype = Object.create(MoreRouting.Emitter);

// Utility

function _compile(model, params, state) {
  params.forEach(function(param) {
    Object.defineProperty(model, param, {
      get: function() {
        return state[param];
      },
      set: function(value) {
        if (state[param] === value) return;
        state[param] = value;
        model.__notify(param, value);
      },
    });
  });
}

})(window);
