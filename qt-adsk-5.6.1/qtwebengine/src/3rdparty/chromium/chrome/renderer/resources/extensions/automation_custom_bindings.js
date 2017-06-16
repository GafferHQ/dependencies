// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the automation API.
var AutomationNode = require('automationNode').AutomationNode;
var AutomationRootNode = require('automationNode').AutomationRootNode;
var automation = require('binding').Binding.create('automation');
var automationInternal =
    require('binding').Binding.create('automationInternal').generate();
var eventBindings = require('event_bindings');
var Event = eventBindings.Event;
var forEach = require('utils').forEach;
var lastError = require('lastError');
var logging = requireNative('logging');
var nativeAutomationInternal = requireNative('automationInternal');
var GetRoutingID = nativeAutomationInternal.GetRoutingID;
var GetSchemaAdditions = nativeAutomationInternal.GetSchemaAdditions;
var schema = GetSchemaAdditions();

/**
 * A namespace to export utility functions to other files in automation.
 */
window.automationUtil = function() {};

// TODO(aboxhall): Look into using WeakMap
var idToAutomationRootNode = {};
var idToCallback = {};

var DESKTOP_TREE_ID = 0;

automationUtil.storeTreeCallback = function(id, callback) {
  if (!callback)
    return;

  var targetTree = idToAutomationRootNode[id];
  if (!targetTree) {
    // If we haven't cached the tree, hold the callback until the tree is
    // populated by the initial onAccessibilityEvent call.
    if (id in idToCallback)
      idToCallback[id].push(callback);
    else
      idToCallback[id] = [callback];
  } else {
    callback(targetTree);
  }
};

/**
 * Global list of tree change observers.
 * @type {Array<TreeChangeObserver>}
 */
automationUtil.treeChangeObservers = [];

automation.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // TODO(aboxhall, dtseng): Make this return the speced AutomationRootNode obj.
  apiFunctions.setHandleRequest('getTree', function getTree(tabID, callback) {
    var routingID = GetRoutingID();

    // enableTab() ensures the renderer for the active or specified tab has
    // accessibility enabled, and fetches its ax tree id to use as
    // a key in the idToAutomationRootNode map. The callback to
    // enableTab is bound to the callback passed in to getTree(), so that once
    // the tree is available (either due to having been cached earlier, or after
    // an accessibility event occurs which causes the tree to be populated), the
    // callback can be called.
    var params = { routingID: routingID, tabID: tabID };
    automationInternal.enableTab(params,
        function onEnable(id) {
          if (lastError.hasError(chrome)) {
            callback();
            return;
          }
          automationUtil.storeTreeCallback(id, callback);
        });
  });

  var desktopTree = null;
  apiFunctions.setHandleRequest('getDesktop', function(callback) {
    desktopTree =
        idToAutomationRootNode[DESKTOP_TREE_ID];
    if (!desktopTree) {
      if (DESKTOP_TREE_ID in idToCallback)
        idToCallback[DESKTOP_TREE_ID].push(callback);
      else
        idToCallback[DESKTOP_TREE_ID] = [callback];

      var routingID = GetRoutingID();

      // TODO(dtseng): Disable desktop tree once desktop object goes out of
      // scope.
      automationInternal.enableDesktop(routingID, function() {
        if (lastError.hasError(chrome)) {
          delete idToAutomationRootNode[
              DESKTOP_TREE_ID];
          callback();
          return;
        }
      });
    } else {
      callback(desktopTree);
    }
  });

  function removeTreeChangeObserver(observer) {
    var observers = automationUtil.treeChangeObservers;
    for (var i = 0; i < observers.length; i++) {
      if (observer == observers[i])
        observers.splice(i, 1);
    }
  }
  apiFunctions.setHandleRequest('removeTreeChangeObserver', function(observer) {
    removeTreeChangeObserver(observer);
  });

  function addTreeChangeObserver(observer) {
    removeTreeChangeObserver(observer);
    automationUtil.treeChangeObservers.push(observer);
  }
  apiFunctions.setHandleRequest('addTreeChangeObserver', function(observer) {
    addTreeChangeObserver(observer);
  });

});

// Listen to the automationInternal.onAccessibilityEvent event, which is
// essentially a proxy for the AccessibilityHostMsg_Events IPC from the
// renderer.
automationInternal.onAccessibilityEvent.addListener(function(data) {
  var id = data.treeID;
  var targetTree = idToAutomationRootNode[id];
  if (!targetTree) {
    // If this is the first time we've gotten data for this tree, it will
    // contain all of the tree's data, so create a new tree which will be
    // bootstrapped from |data|.
    targetTree = new AutomationRootNode(id);
    idToAutomationRootNode[id] = targetTree;
  }
  if (!privates(targetTree).impl.onAccessibilityEvent(data))
    return;

  // If we're not waiting on a callback to getTree(), we can early out here.
  if (!(id in idToCallback))
    return;

  // We usually get a 'placeholder' tree first, which doesn't have any url
  // attribute or child nodes. If we've got that, wait for the full tree before
  // calling the callback.
  // TODO(dmazzoni): Don't send down placeholder (crbug.com/397553)
  if (id != DESKTOP_TREE_ID && !targetTree.attributes.url &&
      targetTree.children.length == 0) {
    return;
  }

  // If the tree wasn't available when getTree() was called, the callback will
  // have been cached in idToCallback, so call and delete it now that we
  // have the complete tree.
  for (var i = 0; i < idToCallback[id].length; i++) {
    console.log('calling getTree() callback');
    var callback = idToCallback[id][i];
    callback(targetTree);
  }
  delete idToCallback[id];
});

automationInternal.onAccessibilityTreeDestroyed.addListener(function(id) {
  var targetTree = idToAutomationRootNode[id];
  if (targetTree) {
    privates(targetTree).impl.destroy();
    delete idToAutomationRootNode[id];
  } else {
    logging.WARNING('no targetTree to destroy');
  }
  delete idToAutomationRootNode[id];
});

exports.binding = automation.generate();

// Add additional accessibility bindings not specified in the automation IDL.
// Accessibility and automation share some APIs (see
// ui/accessibility/ax_enums.idl).
forEach(schema, function(k, v) {
  exports.binding[k] = v;
});
