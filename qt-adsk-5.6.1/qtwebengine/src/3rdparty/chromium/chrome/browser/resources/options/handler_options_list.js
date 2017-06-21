// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var List = cr.ui.List;
  /** @const */ var ListItem = cr.ui.ListItem;
  /** @const */ var DeletableItem = options.DeletableItem;
  /** @const */ var DeletableItemList = options.DeletableItemList;

  /**
   * Creates a new ignored protocol / content handler list item.
   *
   * Accepts values in the form
   *   ['mailto', 'http://www.thesite.com/%s', 'www.thesite.com'],
   * @param {Object} entry A dictionary describing the handlers for a given
   *     protocol.
   * @constructor
   * @extends {options.DeletableItem}
   */
  function IgnoredHandlersListItem(entry) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.__proto__ = IgnoredHandlersListItem.prototype;
    el.decorate();
    return el;
  }

  IgnoredHandlersListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @override */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      // Protocol.
      var protocolElement = document.createElement('div');
      protocolElement.textContent = this.dataItem[0];
      protocolElement.className = 'handlers-type-column';
      this.contentElement_.appendChild(protocolElement);

      // Host name.
      var hostElement = document.createElement('div');
      hostElement.textContent = this.dataItem[2];
      hostElement.className = 'handlers-site-column';
      hostElement.title = this.dataItem[1];
      this.contentElement_.appendChild(hostElement);
    },
  };

  /**
   * @constructor
   * @extends {options.DeletableItemList}
   */
  var IgnoredHandlersList = cr.ui.define('list');

  IgnoredHandlersList.prototype = {
    __proto__: DeletableItemList.prototype,

    /**
     * @override
     * @param {Object} entry
     */
    createItem: function(entry) {
      return new IgnoredHandlersListItem(entry);
    },

    deleteItemAtIndex: function(index) {
      chrome.send('removeIgnoredHandler', [this.dataModel.item(index)]);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },

    /**
     * Set the protocol handlers displayed by this list.  See
     * IgnoredHandlersListItem for an example of the format the list should
     * take.
     *
     * @param {!Array} list A list of ignored protocol handlers.
     */
    setHandlers: function(list) {
      this.dataModel = new ArrayDataModel(list);
    },
  };

  /**
   * Creates a new protocol / content handler list item.
   *
   * Accepts values in the form
   * { protocol: 'mailto',
   *   handlers: [
   *     ['mailto', 'http://www.thesite.com/%s', 'www.thesite.com'],
   *     ...,
   *   ],
   * }
   * @param {Object} entry A dictionary describing the handlers for a given
   *     protocol.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function HandlerListItem(entry) {
    var el = cr.doc.createElement('div');
    el.dataItem = entry;
    el.__proto__ = HandlerListItem.prototype;
    el.decorate();
    return el;
  }

  HandlerListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * @param {Handlers} data
     * @param {{removeHandler: Function, setDefault: Function,
     *          clearDefault: Function}} delegate
     */
    buildWidget_: function(data, delegate) {
      // Protocol.
      var protocolElement = document.createElement('div');
      protocolElement.textContent = data.protocol;
      protocolElement.className = 'handlers-type-column';
      this.appendChild(protocolElement);

      // Handler selection.
      var handlerElement = document.createElement('div');
      var selectElement = document.createElement('select');
      var defaultOptionElement = document.createElement('option');
      defaultOptionElement.selected = data.default_handler == -1;
      defaultOptionElement.textContent =
          loadTimeData.getString('handlersNoneHandler');
      defaultOptionElement.value = -1;
      selectElement.appendChild(defaultOptionElement);

      for (var i = 0; i < data.handlers.length; ++i) {
        var optionElement = document.createElement('option');
        optionElement.selected = i == data.default_handler;
        optionElement.textContent = data.handlers[i][2];
        optionElement.value = i;
        selectElement.appendChild(optionElement);
      }

      selectElement.addEventListener('change', function(e) {
        var index = e.target.value;
        if (index == -1) {
          this.classList.add('none');
          delegate.clearDefault(data.protocol);
        } else {
          handlerElement.classList.remove('none');
          delegate.setDefault(data.handlers[index]);
        }
      });
      handlerElement.appendChild(selectElement);
      handlerElement.className = 'handlers-site-column';
      if (data.default_handler == -1)
        this.classList.add('none');
      this.appendChild(handlerElement);

      if (data.has_policy_recommendations) {
        // Create an indicator to show that the handler has policy
        // recommendations.
        var indicator = new options.ControlledSettingIndicator();
        if (data.is_default_handler_set_by_user || data.default_handler == -1) {
          // The default handler is registered by the user or set to none, which
          // indicates that the user setting has overridden a policy
          // recommendation. Show the appropriate bubble.
          indicator.controlledBy = 'hasRecommendation';
          indicator.resetHandler = function() {
            // If there is a policy recommendation, data.handlers.length >= 1.
            // Setting the default handler to 0 ensures that it won't be 'none',
            // and there *is* a user registered handler created by setDefault,
            // which is required for a change notification.
            // The user-registered handlers are removed in a loop. Note that if
            // a handler is installed by policy, removeHandler does nothing.
            delegate.setDefault(data.handlers[0]);
            for (var i = 0; i < data.handlers.length; ++i) {
              delegate.removeHandler(i, data.handlers[i]);
            }
          };
        } else {
          indicator.controlledBy = 'recommended';
        }
        this.appendChild(indicator);
      }

      if (data.is_default_handler_set_by_user) {
        // Remove link.
        var removeElement = document.createElement('div');
        removeElement.textContent =
            loadTimeData.getString('handlersRemoveLink');
        removeElement.addEventListener('click', function(e) {
          var value = selectElement ? selectElement.value : 0;
          delegate.removeHandler(value, data.handlers[value]);
        });
        removeElement.className =
            'handlers-remove-column handlers-remove-link';
        this.appendChild(removeElement);
      }
    },

    /** @override */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var delegate = {
        removeHandler: function(index, handler) {
          chrome.send('removeHandler', [handler]);
        },
        setDefault: function(handler) {
          chrome.send('setDefault', [handler]);
        },
        clearDefault: function(protocol) {
          chrome.send('clearDefault', [protocol]);
        },
      };

      this.buildWidget_(this.dataItem, delegate);
    },
  };

  /**
   * Create a new passwords list.
   * @constructor
   * @extends {cr.ui.List}
   */
  var HandlersList = cr.ui.define('list');

  HandlersList.prototype = {
    __proto__: List.prototype,

    /**
     * @override
     * @param {Object} entry
     */
    createItem: function(entry) {
      return new HandlerListItem(entry);
    },

    /**
     * The length of the list.
     */
    get length() {
      return this.dataModel.length;
    },

    /**
     * Set the protocol handlers displayed by this list.
     * See HandlerListItem for an example of the format the list should take.
     *
     * @param {!Array} list A list of protocols with their registered handlers.
     */
    setHandlers: function(list) {
      this.dataModel = new ArrayDataModel(list);
    },
  };

  return {
    IgnoredHandlersListItem: IgnoredHandlersListItem,
    IgnoredHandlersList: IgnoredHandlersList,
    HandlerListItem: HandlerListItem,
    HandlersList: HandlersList,
  };
});
