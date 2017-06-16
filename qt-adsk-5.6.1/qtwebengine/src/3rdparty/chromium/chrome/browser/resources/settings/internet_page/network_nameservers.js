// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network nameserver options.
 */
Polymer({
  is: 'network-nameservers',

  properties: {
    /**
     * The current state containing the IP Config properties to display and
     * modify.
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null,
      observer: 'networkStateChanged_'
    },

    /**
     * Whether or not the nameservers can be edited.
     */
    editable: {
      type: Boolean,
      value: false
    },

    /**
     * Array of nameserver addresses stored as strings.
     * @type {!Array<string>}
     */
    nameservers: {
      type: Array,
      value: function() { return []; }
    },

    /**
     * The selected nameserver type.
     */
    nameserversType: {
      type: String,
      value: 'automatic'
    },

    /**
     * Array of nameserver types.
     */
    nameserverTypeNames_: {
      type: Array,
      value: ['automatic', 'google', 'custom'],
      readOnly: true
    },
  },

  /** @const */ GoogleNameservers: ['8.8.4.4', '8.8.8.8'],

  /**
   * Saved nameservers when switching to 'automatic'.
   * @type {!Array<string>}
   */
  savedNameservers_: [],

  /**
   * Polymer networkState changed method.
   */
  networkStateChanged_: function(newValue, oldValue) {
    if (!this.networkState)
      return;

    if (!oldValue || newValue.GUID != oldValue.GUID)
      this.savedNameservers_ = [];

    // Update the 'nameservers' property.
    var nameservers = [];
    var ipv4 = CrOnc.getIPConfigForType(this.networkState, CrOnc.IPType.IPV4);
    if (ipv4 && ipv4.NameServers)
      nameservers = ipv4.NameServers;

    // Update the 'nameserversType' property.
    var configType =
        CrOnc.getActiveValue(this.networkState, 'NameServersConfigType');
    var type;
    if (configType == 'Static') {
      if (nameservers.join(',') == this.GoogleNameservers.join(','))
        type = 'google';
      else
        type = 'custom';
    } else {
      type = 'automatic';
    }
    this.nameserversType = type;
    this.$$('#type').selectedIndex = this.getSelectedIndex_(type);

    this.nameservers = nameservers;
  },

  /**
   * @param {string} nameserversType The nameservers type.
   * @return {number} The selected index for |nameserversType|.
   * @private
   */
  getSelectedIndex_: function(nameserversType) {
    var idx = this.nameserverTypeNames_.indexOf(nameserversType);
    if (idx != -1)
      return idx;
    console.error('Unexpected type: ' + nameserversType);
    return 0;
  },

  /**
   * @param {string} nameserversType The nameservers type.
   * @return {string} The description for |nameserversType|.
   * @private
   */
  nameserverTypeDesc_: function(nameserversType) {
    // TODO(stevenjb): Translate.
    if (nameserversType == 'custom')
      return 'Custom name servers';
    if (nameserversType == 'google')
      return 'Google name servers';
    return 'Automatic name servers';
  },

  /**
   * @param {boolean} editable The editable state.
   * @param {string} nameserversType The nameservers type.
   * @return {boolean} True if the nameservers are editable.
   * @private
   */
  canEdit_: function(editable, nameserversType) {
    return editable && nameserversType == 'custom';
  },

  /**
   * Event triggered when the selected type changes. Updates nameservers and
   * sends the changed value if necessary.
   * @param {Event} event The select node changed event.
   * @private
   */
  onTypeChanged_: function(event) {
    if (this.nameserversType == 'custom')
      this.savedNameservers_ = this.nameservers;
    var type = this.nameserverTypeNames_[event.target.selectedIndex];
    this.nameserversType = type;
    if (type == 'custom') {
      if (this.savedNameservers_.length == 0)
        return;  // Don't change nameservers until onValueChanged_().
      // Restore the saved nameservers and send them.
      this.nameservers = this.savedNameservers_;
    }
    this.sendNameServers_();
  },

  /**
   * Event triggered when a nameserver value changes.
   * @private
   */
  onValueChanged_: function() {
    if (this.nameserversType != 'custom') {
      // If a user inputs Google nameservers in the custom nameservers fields,
      // |nameserversType| will change to 'google' so don't send the values.
      return;
    }
    this.sendNameServers_();
  },

  /**
   * Sends the current nameservers type (for automatic) or value.
   * @private
   */
  sendNameServers_: function() {
    var type = this.nameserversType;
    console.debug('NameServers.sendNameServers: ' + type);

    var nameservers;
    if (type == 'custom') {
      nameservers = [];
      for (var i = 0; i < 4; ++i) {
        var id = 'nameserver' + i;
        var nameserver = this.$$('#' + id).value;
        if (nameserver)
          nameservers.push(nameserver);
      }
      this.fire('changed', {
        field: 'NameServers',
        value: nameservers
      });
    } else if (type == 'google') {
      nameservers = this.GoogleNameservers;
      this.fire('changed', {
        field: 'NameServers',
        value: nameservers
      });
    } else {
      // automatic
      this.fire('changed', {
        field: 'NameServersConfigType',
        value: 'DHCP'
      });
    }
  },
});
