

  Polymer({

    is: 'iron-media-query',

    properties: {

      /**
       * The Boolean return value of the media query.
       *
       * @attribute queryMatches
       * @type Boolean
       * @default false
       */
      queryMatches: {
        type: Boolean,
        value: false,
        readOnly: true,
        notify: true
      },

      /**
       * The CSS media query to evaluate.
       *
       * @attribute query
       * @type String
       */
      query: {
        type: String,
        observer: 'queryChanged'
      }

    },

    created: function() {
      this._mqHandler = this.queryHandler.bind(this);
    },

    queryChanged: function(query) {
      if (this._mq) {
        this._mq.removeListener(this._mqHandler);
      }
      if (query[0] !== '(') {
        query = '(' + query + ')';
      }
      this._mq = window.matchMedia(query);
      this._mq.addListener(this._mqHandler);
      this.queryHandler(this._mq);
    },

    queryHandler: function(mq) {
      this._setQueryMatches(mq.matches);
    }

  });

