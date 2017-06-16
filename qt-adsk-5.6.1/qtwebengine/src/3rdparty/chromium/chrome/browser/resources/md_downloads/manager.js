// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloads', function() {
  /**
   * Class to own and manage download items.
   * @constructor
   */
  function Manager() {}

  cr.addSingletonGetter(Manager);

  Manager.prototype = {
    /** @private {string} */
    searchText_: '',

    /**
     * Sets the search text, updates related UIs, and tells the browser.
     * @param {string} searchText Text we're searching for.
     * @private
     */
    setSearchText_: function(searchText) {
      this.searchText_ = searchText;

      $('downloads-summary-text').textContent = this.searchText_ ?
          loadTimeData.getStringF('searchResultsFor', this.searchText_) : '';

      // Split quoted terms (e.g., 'The "lazy" dog' => ['The', 'lazy', 'dog']).
      function trim(s) { return s.trim(); }
      chrome.send('getDownloads', searchText.split(/"([^"]*)"/).map(trim));
    },

    /**
     * @return {number} A guess at how many items could be visible at once.
     * @private
     */
    guesstimateNumberOfVisibleItems_: function() {
      var toolbarHeight = $('downloads-toolbar').offsetHeight;
      var summaryHeight = $('downloads-summary').offsetHeight;
      var nonItemSpace = toolbarHeight + summaryHeight;
      return Math.floor((window.innerHeight - nonItemSpace) / 46) + 1;
    },

    /**
     * Called when all items need to be updated.
     * @param {!Array<!downloads.Data>} list A list of new download data.
     * @private
     */
    updateAll_: function(list) {
      var oldIdMap = this.idMap_ || {};

      /** @private {!Object<!downloads.ItemView>} */
      this.idMap_ = {};

      /** @private {!Array<!downloads.ItemView>} */
      this.items_ = [];

      if (!this.iconLoader_) {
        var guesstimate = Math.max(this.guesstimateNumberOfVisibleItems_(), 1);
        /** @private {downloads.ThrottledIconLoader} */
        this.iconLoader_ = new downloads.ThrottledIconLoader(guesstimate);
      }

      for (var i = 0; i < list.length; ++i) {
        var data = list[i];
        var id = data.id;

        // Re-use old items when possible (saves work, preserves focus).
        var item = oldIdMap[id] || new downloads.ItemView(this.iconLoader_);

        this.idMap_[id] = item;  // Associated by ID for fast lookup.
        this.items_.push(item);  // Add to sorted list for order.

        // Render |item| but don't actually add to the DOM yet. |this.items_|
        // must be fully created to be able to find the right spot to insert.
        item.update(data);

        // Collapse redundant dates.
        var prev = list[i - 1];
        item.hideDate = !!prev && prev.date_string == data.date_string;

        delete oldIdMap[id];
      }

      // Remove stale, previously rendered items from the DOM.
      for (var id in oldIdMap) {
        if (oldIdMap[id].parentNode)
          oldIdMap[id].parentNode.removeChild(oldIdMap[id]);
        delete oldIdMap[id];
      }

      for (var i = 0; i < this.items_.length; ++i) {
        var item = this.items_[i];
        if (item.parentNode)  // Already in the DOM; skip.
          continue;

        var before = null;
        // Find the next rendered item after this one, and insert before it.
        for (var j = i + 1; !before && j < this.items_.length; ++j) {
          if (this.items_[j].parentNode)
            before = this.items_[j];
        }
        // If |before| is null, |item| will just get added at the end.
        this.node_.insertBefore(item, before);
      }

      var noDownloadsOrResults = $('no-downloads-or-results');
      noDownloadsOrResults.textContent = loadTimeData.getString(
          this.searchText_ ? 'noSearchResults' : 'noDownloads');

      var hasDownloads = this.size_() > 0;
      this.node_.hidden = !hasDownloads;
      noDownloadsOrResults.hidden = hasDownloads;

      if (loadTimeData.getBoolean('allowDeletingHistory'))
        $('clear-all').hidden = !hasDownloads || this.searchText_.length > 0;
    },

    /**
     * @param {!downloads.Data} data
     * @private
     */
    updateItem_: function(data) {
      this.idMap_[data.id].update(data);
    },

    /**
     * @return {number} The number of downloads shown on the page.
     * @private
     */
    size_: function() {
      return this.items_.length;
    },

    /** @private */
    clearAll_: function() {
      if (loadTimeData.getBoolean('allowDeletingHistory')) {
        chrome.send('clearAll');
        this.setSearchText_('');
      }
    },

    /** @private */
    onLoad_: function() {
      this.node_ = $('downloads-display');

      $('clear-all').onclick = function() {
        this.clearAll_();
      }.bind(this);

      $('open-downloads-folder').onclick = function() {
        chrome.send('openDownloadsFolder');
      };

      $('search-button').onclick = function() {
        if (!$('search-term').hidden)
          return;
        $('clear-search').hidden = false;
        $('search-term').hidden = false;
      };

      $('clear-search').onclick = function() {
        $('clear-search').hidden = true;
        $('search-term').hidden = true;
        $('search-term').value = '';
        this.setSearchText_('');
      }.bind(this);

      // TODO(dbeam): this previously used onsearch, which batches keystrokes
      // together. This should probably be re-instated eventually.
      $('search-term').oninput = function(e) {
        this.setSearchText_($('search-term').value);
      }.bind(this);

      cr.ui.decorate('command', cr.ui.Command);
      document.addEventListener('canExecute', this.onCanExecute_.bind(this));
      document.addEventListener('command', this.onCommand_.bind(this));

      this.setSearchText_('');
    },

    /**
     * @param {Event} e
     * @private
     */
    onCanExecute_: function(e) {
      e = /** @type {cr.ui.CanExecuteEvent} */(e);
      switch (e.command.id) {
        case 'undo-command':
          e.canExecute = !$('search-term').contains(document.activeElement);
          break;
        case 'clear-all-command':
          e.canExecute = true;
          break;
      }
    },

    /**
     * @param {Event} e
     * @private
     */
    onCommand_: function(e) {
      if (e.command.id == 'undo-command')
        chrome.send('undo');
      else if (e.command.id == 'clear-all-command')
        this.clearAll_();
    },
  };

  Manager.updateAll = function(list) {
    Manager.getInstance().updateAll_(list);
  };

  Manager.updateItem = function(item) {
    Manager.getInstance().updateItem_(item);
  };

  Manager.setSearchText = function(searchText) {
    Manager.getInstance().setSearchText_(searchText);
  };

  Manager.onLoad = function() {
    Manager.getInstance().onLoad_();
  };

  Manager.size = function() {
    return Manager.getInstance().size_();
  };

  return {Manager: Manager};
});

window.addEventListener('load', downloads.Manager.onLoad);
