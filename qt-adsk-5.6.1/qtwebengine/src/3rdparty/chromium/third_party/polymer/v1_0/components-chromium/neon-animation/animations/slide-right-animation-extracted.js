

  Polymer({

    is: 'slide-right-animation',

    behaviors: [
      Polymer.NeonAnimationBehavior
    ],

    configure: function(config) {
      var node = config.node;

      if (config.transformOrigin) {
        this.setPrefixedProperty(node, 'transformOrigin', config.transformOrigin);
      } else {
        this.setPrefixedProperty(node, 'transformOrigin', '0 50%');
      }

      this._effect = new KeyframeEffect(node, [
        {'transform': 'none'},
        {'transform': 'translateX(100%)'}
      ], this.timingFromConfig(config));

      return this._effect;
    }

  });

