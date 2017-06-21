// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the IP Config properties for
 * a network state. TODO(stevenjb): Allow editing of static IP configurations
 * when 'editable' is true.
 */
Polymer({
  is: 'network-ip-config',

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
     * Whether or not the IP Address can be edited.
     * TODO(stevenjb): Implement editing.
     */
    editable: {
      type: Boolean,
      value: false
    },

    /**
     * State of 'Configure IP Addresses Automatically'.
     */
    automatic: {
      type: Boolean,
      value: false,
      observer: 'automaticChanged_'
    },

    /**
     * The currently visible IP Config property dictionary. The 'RoutingPrefix'
     * property is a human-readable mask instead of a prefix length.
     * @type {{
     *   ipv4: !CrOnc.IPConfigUIProperties,
     *   ipv6: !CrOnc.IPConfigUIProperties
     * }}
     */
    ipConfig: {
      type: Object,
      value: function() { return {ipv4: {}, ipv6: {}}; }
    },

    /**
     * Array of properties to pass to the property list.
     */
    ipConfigFields_: {
      type: Array,
      value: function() {
        return [
          'ipv4.IPAddress',
          'ipv4.RoutingPrefix',
          'ipv4.Gateway',
          'ipv6.IPAddress'
        ];
      },
      readOnly: true
    },
  },

  /**
   * Saved static IP configuration properties when switching to 'automatic'.
   * @type {?CrOnc.IPConfigUIProperties}
   */
  savedStaticIp_: null,

  /**
   * Polymer networkState changed method.
   */
  networkStateChanged_: function(newValue, oldValue) {
    if (this.networkState === undefined || this.ipConfig === undefined)
      return;

    if (newValue.GUID != (oldValue && oldValue.GUID))
      this.savedStaticIp_ = null;

    // Update the 'automatic' property.
    var ipConfigType =
        CrOnc.getActiveValue(this.networkState, 'IPAddressConfigType');
    this.automatic = (ipConfigType != CrOnc.IPConfigType.STATIC);

    // Update the 'ipConfig' property.
    var ipv4 = CrOnc.getIPConfigForType(this.networkState, CrOnc.IPType.IPV4);
    var ipv6 = CrOnc.getIPConfigForType(this.networkState, CrOnc.IPType.IPV6);
    this.ipConfig = {
      ipv4: this.getIPConfigUIProperties_(ipv4),
      ipv6: this.getIPConfigUIProperties_(ipv6)
    };
  },

  /**
   * Polymer automatic changed method.
   */
  automaticChanged_: function() {
    if (this.automatic === undefined || this.ipConfig === undefined)
      return;
    console.debug('IP.automaticChanged: ' + this.automatic);
    if (this.automatic || !this.savedStaticIp_) {
      // Save the static IP configuration when switching to automatic.
      this.savedStaticIp_ = this.ipConfig.ipv4;
      this.fire('changed', {
        field: 'IPAddressConfigType',
        value: this.automatic ? 'DHCP' : 'Static'
      });
    } else {
      // Restore the saved static IP configuration.
      var ipconfig = {
        Gateway: this.savedStaticIp_.Gateway,
        IPAddress: this.savedStaticIp_.IPAddress,
        RoutingPrefix: this.savedStaticIp_.RoutingPrefix,
        Type: this.savedStaticIp_.Type
      };
      this.fire('changed', {
        field: 'StaticIPConfig',
        value: this.getIPConfigProperties_(ipconfig)
      });
    }
  },

  /**
   * @param {?CrOnc.IPConfigProperties} ipconfig The IP Config properties.
   * @return {!CrOnc.IPConfigUIProperties} A new IPConfigUIProperties object
   *     with RoutingPrefix expressed as a string mask instead of a prefix
   *     length. Returns an empty object if |ipconfig| is undefined.
   * @private
   */
  getIPConfigUIProperties_: function(ipconfig) {
    var result = {};
    if (!ipconfig)
      return result;
    for (var key in ipconfig) {
      var value = ipconfig[key];
      if (key == 'RoutingPrefix')
        result.RoutingPrefix = CrOnc.getRoutingPrefixAsNetmask(value);
      else
        result[key] = value;
    }
    return result;
  },

  /**
   * @param {!CrOnc.IPConfigUIProperties} ipconfig The IP Config UI properties.
   * @return {!CrOnc.IPConfigProperties} A new IPConfigProperties object with
   *     RoutingPrefix expressed as a a prefix length.
   * @private
   */
  getIPConfigProperties_: function(ipconfig) {
    var result = {};
    for (var key in ipconfig) {
      var value = ipconfig[key];
      if (key == 'RoutingPrefix')
        result.RoutingPrefix = CrOnc.getRoutingPrefixAsLength(value);
      else
        result[key] = value;
    }
    return result;
  },

  /**
   * @param {!CrOnc.IPConfigUIProperties} ipConfig The IP Config properties.
   * @param {boolean} editable The editable property.
   * @param {boolean} automatic The automatic property.
   * @return {Object} An object with the edit type for each editable field.
   * @private
   */
  getIPEditFields_: function(ipConfig, editable, automatic) {
    if (!editable || automatic)
      return {};
    return {
      'ipv4.IPAddress': 'String',
      'ipv4.RoutingPrefix': 'String',
      'ipv4.Gateway': 'String'
    };
  },

  /**
   * Event triggered when the network property list changes.
   * @param {!{detail: { field: string, value: string}}} event The
   *     network-property-list changed event.
   * @private
   */
  onIPChanged_: function(event) {
    event.stopPropagation();

    var field = event.detail.field;
    var value = event.detail.value;
    console.debug('IP.onIPChanged: ' + field + ' -> ' + value);
    // Note: |field| includes the 'ipv4.' prefix.
    this.set('ipConfig.' + field, value);
    this.fire('changed', {
      field: 'StaticIPConfig',
      value: this.getIPConfigProperties_(this.ipConfig.ipv4)
    });
  },
});
