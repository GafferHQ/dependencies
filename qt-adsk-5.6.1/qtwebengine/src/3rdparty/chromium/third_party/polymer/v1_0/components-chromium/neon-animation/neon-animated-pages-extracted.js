
(function() {

  Polymer({

    is: 'neon-animated-pages',

    behaviors: [
      Polymer.IronResizableBehavior,
      Polymer.IronSelectableBehavior,
      Polymer.NeonAnimationRunnerBehavior
    ],

    properties: {

      activateEvent: {
        type: String,
        value: ''
      },

      // if true, the initial page selection will also be animated according to its animation config.
      animateInitialSelection: {
        type: Boolean,
        value: false
      }

    },

    observers: [
      '_selectedChanged(selected)'
    ],

    listeners: {
      'neon-animation-finish': '_onNeonAnimationFinish'
    },

    _selectedChanged: function(selected) {

      var selectedPage = this.selectedItem;
      var oldPage = this._prevSelected || false;
      this._prevSelected = selectedPage;

      // on initial load and if animateInitialSelection is negated, simply display selectedPage.
      if (!oldPage && !this.animateInitialSelection) {
        this._completeSelectedChanged();
        return;
      }

      // insert safari fix.
      this.animationConfig = [{
        name: 'opaque-animation',
        node: selectedPage
      }];

      // configure selectedPage animations.
      if (this.entryAnimation) {
        this.animationConfig.push({
          name: this.entryAnimation,
          node: selectedPage
        });
      } else {
        if (selectedPage.getAnimationConfig) {
          this.animationConfig.push({
            animatable: selectedPage,
            type: 'entry'
          });
        }
      }

      // configure oldPage animations iff exists.
      if (oldPage) {

        // cancel the currently running animation if one is ongoing.
        if (oldPage.classList.contains('neon-animating')) {
          this._squelchNextFinishEvent = true;
          this.cancelAnimation();
          this._completeSelectedChanged();
        }

        // configure the animation.
        if (this.exitAnimation) {
          this.animationConfig.push({
            name: this.exitAnimation,
            node: oldPage
          });
        } else {
          if (oldPage.getAnimationConfig) {
            this.animationConfig.push({
              animatable: oldPage,
              type: 'exit'
            });
          }
        }

        // display the oldPage during the transition.
        oldPage.classList.add('neon-animating');
      }

      // display the selectedPage during the transition.
      selectedPage.classList.add('neon-animating');

      // actually run the animations.
      if (this.animationConfig.length > 1) {

        // on first load, ensure we run animations only after element is attached.
        if (!this.isAttached) {
          this.async(function () {
            this.playAnimation(null, {
              fromPage: null,
              toPage: selectedPage
            });
          });

        } else {
          this.playAnimation(null, {
            fromPage: oldPage,
            toPage: selectedPage
          });
        }

      } else {
        this._completeSelectedChanged(oldPage, selectedPage);
      }
    },

    _completeSelectedChanged: function(oldPage, selectedPage) {
      if (selectedPage) {
        selectedPage.classList.remove('neon-animating');
      }
      if (oldPage) {
        oldPage.classList.remove('neon-animating');
      }
      if (!selectedPage || !oldPage) {
        var nodes = Polymer.dom(this.$.content).getDistributedNodes();
        for (var node, index = 0; node = nodes[index]; index++) {
          node.classList && node.classList.remove('neon-animating');
        }
      }
      this.async(this.notifyResize);
    },

    _onNeonAnimationFinish: function(event) {
      if (this._squelchNextFinishEvent) {
        this._squelchNextFinishEvent = false;
        return;
      }
      this._completeSelectedChanged(event.detail.fromPage, event.detail.toPage);
    }

  })

})();
