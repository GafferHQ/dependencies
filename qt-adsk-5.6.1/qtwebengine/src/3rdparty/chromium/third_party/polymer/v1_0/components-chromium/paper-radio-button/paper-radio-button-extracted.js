
    Polymer({
      is: 'paper-radio-button',

      behaviors: [
        Polymer.PaperInkyFocusBehavior
      ],

      hostAttributes: {
        role: 'radio',
        'aria-checked': false,
        tabindex: 0
      },

      properties: {
        /**
         * Fired when the checked state changes due to user interaction.
         *
         * @event change
         */

        /**
         * Fired when the checked state changes.
         *
         * @event iron-change
         */

        /**
         * Gets or sets the state, `true` is checked and `false` is unchecked.
         */
        checked: {
          type: Boolean,
          value: false,
          reflectToAttribute: true,
          notify: true,
          observer: '_checkedChanged'
        },

        /**
         * If true, the button toggles the active state with each tap or press
         * of the spacebar.
         */
        toggles: {
          type: Boolean,
          value: true,
          reflectToAttribute: true
        }
      },

      ready: function() {
        if (Polymer.dom(this).textContent == '') {
          this.$.radioLabel.hidden = true;
        } else {
          this.setAttribute('aria-label', Polymer.dom(this).textContent);
        }
        this._isReady = true;
      },

      _buttonStateChanged: function() {
        if (this.disabled) {
          return;
        }
        if (this._isReady) {
          this.checked = this.active;
        }
      },

      _checkedChanged: function() {
        this.setAttribute('aria-checked', this.checked ? 'true' : 'false');
        this.active = this.checked;
        this.fire('iron-change');
      }
    })
  