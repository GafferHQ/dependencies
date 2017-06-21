/**
 * @fileoverview Closure compiler externs for the Polymer library.
 *
 * @externs
 * @license
 * Copyright (c) 2015 The Polymer Project Authors. All rights reserved.
 * This code may only be used under the BSD style license found at
 * http://polymer.github.io/LICENSE.txt. The complete set of authors may be
 * found at http://polymer.github.io/AUTHORS.txt. The complete set of
 * contributors may be found at http://polymer.github.io/CONTRIBUTORS.txt. Code
 * distributed by Google as part of the polymer project is also subject to an
 * additional IP rights grant found at http://polymer.github.io/PATENTS.txt.
 */

/**
 * @param {!{is: string}} descriptor The Polymer descriptor of the element.
 * @see https://github.com/Polymer/polymer/blob/0.8-preview/PRIMER.md#custom-element-registration
 */
var Polymer = function(descriptor) {};


/** @constructor @extends {HTMLElement} */
var PolymerElement = function() {};

/**
 * A mapping from ID to element in this Polymer Element's local DOM.
 * @type {!Object}
 */
PolymerElement.prototype.$;

/**
 * True if the element has been attached to the DOM.
 * @type {boolean}
 */
PolymerElement.prototype.isAttached;

/**
 * The root node of the element.
 * @type {!Node}
 */
PolymerElement.prototype.root;

/**
 * Returns the first node in this element’s local DOM that matches selector.
 * @param {string} selector
 */
PolymerElement.prototype.$$ = function(selector) {};

/** @type {string} The Custom element tag name. */
PolymerElement.prototype.is;

/** @type {string} The native element this element extends. */
PolymerElement.prototype.extends;

/**
 * An array of objects whose properties get added to this element.
 * @see https://www.polymer-project.org/1.0/docs/devguide/behaviors.html
 * @type {!Array<!Object>|undefined}
 */
PolymerElement.prototype.behaviors;

/**
 * A string-separated list of dependent properties that should result in a
 * change function being called. These observers differ from single-property
 * observers in that the change handler is called asynchronously.
 *
 * @type {!Object<string, string>|undefined}
 */
PolymerElement.prototype.observers;

/** On create callback. */
PolymerElement.prototype.created = function() {};
/** On ready callback. */
PolymerElement.prototype.ready = function() {};
/** On attached to the DOM callback. */
PolymerElement.prototype.attached = function() {};
/** On detached from the DOM callback. */
PolymerElement.prototype.detached = function() {};

/**
 * Callback fired when an attribute on the element has been changed.
 *
 * @param {string} name The name of the attribute that changed.
 */
PolymerElement.prototype.attributeChanged = function(name) {};

/** @typedef {!{
 *    type: !Function,
 *    reflectToAttribute: (boolean|undefined),
 *    readOnly: (boolean|undefined),
 *    notify: (boolean|undefined),
 *    value: *,
 *    computed: (string|undefined),
 *    observer: (string|undefined)
 *  }} */
PolymerElement.PropertyConfig;

/** @typedef {!Object<string, (!Function|!PolymerElement.PropertyConfig)>} */
PolymerElement.Properties;

/** @type {!PolymerElement.Properties} */
PolymerElement.prototype.properties;

/** @type {!Object<string, *>} */
PolymerElement.prototype.hostAttributes;

/**
 * An object that maps events to event handler function names.
 * @type {!Object<string, string>}
 */
PolymerElement.prototype.listeners;

/**
 * Return the element whose local dom within which this element is contained.
 * @type {?Element}
 */
PolymerElement.prototype.domHost;

/**
 * Notifies the event binding system of a change to a property.
 * @param  {string} path  The path to set.
 * @param  {*}      value The value to send in the update notification.
 */
PolymerElement.prototype.notifyPath = function(path, value) {};

/**
 * Convienence method for setting a value to a path and notifying any
 * elements bound to the same path.
 *
 * Note, if any part in the path except for the last is undefined,
 * this method does nothing (this method does not throw when
 * dereferencing undefined paths).
 *
 * @param {(string|Array<(string|number)>)} path Path to the value
 *   to read.  The path may be specified as a string (e.g. `foo.bar.baz`)
 *   or an array of path parts (e.g. `['foo.bar', 'baz']`).  Note that
 *   bracketed expressions are not supported; string-based path parts
 *   *must* be separated by dots.  Note that when dereferencing array
 *   indicies, the index may be used as a dotted part directly
 *   (e.g. `users.12.name` or `['users', 12, 'name']`).
 * @param {*} value Value to set at the specified path.
 * @param {Object=} root Root object from which the path is evaluated.
*/
PolymerElement.prototype.set = function(path, value, root) {};

/**
 * Convienence method for reading a value from a path.
 *
 * Note, if any part in the path is undefined, this method returns
 * `undefined` (this method does not throw when dereferencing undefined
 * paths).
 *
 * @param {(string|Array<(string|number)>)} path Path to the value
 *   to read.  The path may be specified as a string (e.g. `foo.bar.baz`)
 *   or an array of path parts (e.g. `['foo.bar', 'baz']`).  Note that
 *   bracketed expressions are not supported; string-based path parts
 *   *must* be separated by dots.  Note that when dereferencing array
 *   indicies, the index may be used as a dotted part directly
 *   (e.g. `users.12.name` or `['users', 12, 'name']`).
 * @param {Object=} root Root object from which the path is evaluated.
 * @return {*} Value at the path, or `undefined` if any part of the path
 *   is undefined.
 */
PolymerElement.prototype.get = function(path, root) {};

/**
 * Fire an event.
 *
 * @param {string} type An event name.
 * @param {Object=} detail
 * @param {{
 *   bubbles: (boolean|undefined),
 *   cancelable: (boolean|undefined),
 *   node: (!HTMLElement|undefined)}=} options
 * @return {Object} event
 */
PolymerElement.prototype.fire = function(type, detail, options) {};

/**
 * Toggles the named boolean class on the host element, adding the class if
 * bool is truthy and removing it if bool is falsey. If node is specified, sets
 * the class on node instead of the host element.
 * @param {string} name
 * @param {boolean} bool
 * @param {HTMLElement=} node
 */
PolymerElement.prototype.toggleClass = function(name, bool, node) {};

/**
 * Toggles the named boolean attribute on the host element, adding the attribute
 * if bool is truthy and removing it if bool is falsey. If node is specified,
 * sets the attribute on node instead of the host element.
 * @param {string} name
 * @param {boolean} bool
 * @param {HTMLElement=} node
 */
PolymerElement.prototype.toggleAttribute = function(name, bool, node) {};

/**
 * Moves a boolean attribute from oldNode to newNode, unsetting the attribute
 * (if set) on oldNode and setting it on newNode.
 * @param {string} name
 * @param {!HTMLElement} newNode
 * @param {!HTMLElement} oldNode
 */
PolymerElement.prototype.attributeFollows = function(name, newNode, oldNode) {};

/**
 * Convenience method to add an event listener on a given element, late bound to
 * a named method on this element.
 * @param {!Element} node Element to add event listener to.
 * @param {string} eventName Name of event to listen for.
 * @param {string} methodName Name of handler method on this to call.
 */
PolymerElement.prototype.listen = function(node, eventName, methodName) {};

/**
 * Override scrolling behavior to all direction, one direction, or none.
 *
 * Valid scroll directions:
 * 'all': scroll in any direction
 * 'x': scroll only in the 'x' direction
 * 'y': scroll only in the 'y' direction
 * 'none': disable scrolling for this node
 *
 * @param {string=} direction Direction to allow scrolling Defaults to all.
 * @param {HTMLElement=} node Element to apply scroll direction setting.
 *     Defaults to this.
 */
PolymerElement.prototype.setScrollDirection = function(direction, node) {};

/**
 * @param {!Function} method
 * @param {number=} wait
 * @return {number} A handle which can be used to cancel the job.
 */
PolymerElement.prototype.async = function(method, wait) {};

Polymer.Base;

/**
 * Used by the promise-polyfill on its own.
 *
 * @param {!Function} method
 * @param {number=} wait
 * @return {number} A handle which can be used to cancel the job.
 */
Polymer.Base.async = function(method, wait) {};

/**
 * @param {number} handle
 */
PolymerElement.prototype.cancelAsync = function(handle) {};

/**
 * Call debounce to collapse multiple requests for a named task into one
 * invocation, which is made after the wait time has elapsed with no new
 * request. If no wait time is given, the callback is called at microtask timing
 * (guaranteed to be before paint).
 * @param {string} jobName
 * @param {!Function} callback
 * @param {number=} wait
 */
PolymerElement.prototype.debounce = function(jobName, callback, wait) {};

/**
 * Cancels an active debouncer without calling the callback.
 * @param {string} jobName
 */
PolymerElement.prototype.cancelDebouncer = function(jobName) {};

/**
 * Calls the debounced callback immediately and cancels the debouncer.
 * @param {string} jobName
 */
PolymerElement.prototype.flushDebouncer = function(jobName) {};

/**
 * @param {string} jobName
 * @return {boolean} True if the named debounce task is waiting to run.
 */
PolymerElement.prototype.isDebouncerActive = function(jobName) {};


/**
 * Applies a CSS transform to the specified node, or this element if no node is
 * specified. transform is specified as a string.
 * @param {string} transform
 * @param {HTMLElement=} node
 */
PolymerElement.prototype.transform = function(transform, node) {};

/**
 * Transforms the specified node, or this element if no node is specified.
 * @param {number|string} x
 * @param {number|string} y
 * @param {number|string} z
 * @param {HTMLElement=} node
 */
PolymerElement.prototype.translate3d = function(x, y, z, node) {};

/**
 * Dynamically imports an HTML document.
 * @param {string} href
 * @param {Function=} onload
 * @param {Function=} onerror
 */
PolymerElement.prototype.importHref = function(href, onload, onerror) {};

/**
 * Delete an element from an array.
 * @param {!Array} array
 * @param {*} item
 */
PolymerElement.prototype.arrayDelete = function(array, item) {};

/**
 * Resolve a url to make it relative to the current doc.
 * @param {string} url
 * @return {string}
 */
PolymerElement.prototype.resolveUrl = function(url) {};

/**
 * Logs a message to the console.
 *
 * @param {!Array} var_args
 * @protected
 */
PolymerElement.prototype._log = function(var_args) {};

/**
 * Logs a message to the console with a 'warn' level.
 *
 * @param {!Array} var_args
 * @protected
 */
PolymerElement.prototype._warn = function(var_args) {};

/**
 * Logs a message to the console with an 'error' level.
 *
 * @param {!Array} var_args
 * @protected
 */
PolymerElement.prototype._error = function(var_args) {};

/**
 * Formats string arguments together for a console log.
 *
 * @param {...*} var_args
 * @return {!Array} The formatted array of args to a log function.
 * @protected
 */
PolymerElement.prototype._logf = function(var_args) {};


/**
 * A Polymer DOM API for manipulating DOM such that local DOM and light DOM
 * trees are properly maintained.
 *
 * @constructor
 */
var PolymerDomApi = function() {};

/** @param {!Node} node */
PolymerDomApi.prototype.appendChild = function(node) {};

/**
 * @param {!Node} node
 * @param {!Node} beforeNode
 */
PolymerDomApi.prototype.insertBefore = function(node, beforeNode) {};

/** @param {!Node} node */
PolymerDomApi.prototype.removeChild = function(node) {};

/** @type {!Array<!Node>} */
PolymerDomApi.prototype.childNodes;

/** @type {?Node} */
PolymerDomApi.prototype.parentNode;

/** @type {?Node} */
PolymerDomApi.prototype.firstChild;

/** @type {?Node} */
PolymerDomApi.prototype.lastChild;

/** @type {?HTMLElement} */
PolymerDomApi.prototype.firstElementChild;

/** @type {?HTMLElement} */
PolymerDomApi.prototype.lastElementChild;

/** @type {?Node} */
PolymerDomApi.prototype.previousSibling;

/** @type {?Node} */
PolymerDomApi.prototype.nextSibling;

/** @type {string} */
PolymerDomApi.prototype.textContent;

/** @type {string} */
PolymerDomApi.prototype.innerHTML;

/**
 * @param {string} selector
 * @return {?HTMLElement}
 */
PolymerDomApi.prototype.querySelector = function(selector) {};

/**
 * @param {string} selector
 * @return {!Array<?HTMLElement>}
 */
PolymerDomApi.prototype.querySelectorAll = function(selector) {};

/** @return {!Array<!Node>} */
PolymerDomApi.prototype.getDistributedNodes = function() {};

/** @return {!Array<!Node>} */
PolymerDomApi.prototype.getDestinationInsertionPoints = function() {};

/** @return {?Node} */
PolymerDomApi.prototype.getOwnerRoot = function() {};

/**
 * @param {string} attribute
 * @param {string|number|boolean} value Values are converted to strings with
 *     ToString, so we accept number and boolean since both convert easily to
 *     strings.
 */
PolymerDomApi.prototype.setAttribute = function(attribute, value) {};

/** @param {string} attribute */
PolymerDomApi.prototype.removeAttribute = function(attribute) {};

/** @type {?DOMTokenList} */
PolymerDomApi.prototype.classList;

/**
 * A Polymer Event API.
 *
 * @constructor
 */
var PolymerEventApi = function() {};

/** @type {?EventTarget} */
PolymerEventApi.prototype.rootTarget;

/** @type {?EventTarget} */
PolymerEventApi.prototype.localTarget;

/** @type {?Array<!Element>|undefined} */
PolymerEventApi.prototype.path;

/**
 * Returns a Polymer-friendly API for manipulating DOM of a specified node or
 * an event API for a specified event..
 *
 * @param {?Node|?Event} nodeOrEvent
 * @return {!PolymerDomApi|!PolymerEventApi}
 */
Polymer.dom = function(nodeOrEvent) {};

Polymer.dom.flush = function() {};

Polymer.CaseMap;

/**
 * Convert a string from dash to camel-case.
 * @param {string} dash
 * @return {string} The string in camel-case.
 */
Polymer.CaseMap.dashToCamelCase = function(dash) {};

/**
 * Convert a string from camel-case to dash format.
 * @param {string} camel
 * @return {string} The string in dash format.
 */
Polymer.CaseMap.camelToDashCase = function(camel) {};


/**
 * An Event type fired when moving while finger/button is down.
 * state - a string indicating the tracking state:
 *     + start: fired when tracking is first detected (finger/button down and
 *              moved past a pre-set distance threshold)
 *     + track: fired while tracking
 *     + end: fired when tracking ends
 * x - clientX coordinate for event
 * y - clientY coordinate for event
 * dx - change in pixels horizontally since the first track event
 * dy - change in pixels vertically since the first track event
 * ddx - change in pixels horizontally since last track event
 * ddy - change in pixels vertically since last track event
 * hover() - a function that may be called to determine the element currently
 *           being hovered
 *
 * @typedef {{
 *   state: string,
 *   x: number,
 *   y: number,
 *   dx: number,
 *   dy: number,
 *   ddx: number,
 *   ddy: number,
 *   hover: (function(): Node)
 * }}
 */
var PolymerTrackEvent;

/**
 * An Event type fired when a finger does down, up, or taps.
 * x - clientX coordinate for event
 * y - clientY coordinate for event
 * sourceEvent - the original DOM event that caused the down action
 *
 * @typedef {{
 *   x: number,
 *   y: number,
 *   sourceEvent: Event
 * }}
 */
var PolymerTouchEvent;
