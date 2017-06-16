// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element is used to show information about issues related
// to casting.
Polymer({
  is: 'issue-banner',

  properties: {
    /**
     * The issue to show.
     * @type {?media_router.Issue}
     */
    issue: {
      type: Object,
      value: null,
      observer: 'updateActionButtonText_',
    },

    /**
     * Maps an issue action type to the resource identifier of the text shown
     * in the action button.
     * This is a property of issue-banner because it is used in tests.
     * @type {!Array<string>}
     */
    issueActionTypeToButtonTextResource_: {
      type: Array,
      value: function() {
        return ['okButton', 'cancelButton', 'dismissButton',
            'learnMoreButton'];
      },
    },

    /**
     * The text shown in the default action button.
     * @private {string}
     */
    defaultActionButtonText_: {
      type: String,
      value: '',
    },

    /**
     * The text shown in the secondary action button.
     * @private {string}
     */
    secondaryActionButtonText_: {
      type: String,
      value: '',
    },
  },

  /**
   * @param {?media_router.Issue} issue
   * @return {boolean} Whether or not to hide the blocking issue UI.
   * @private
   */
  computeIsBlockingIssueHidden_: function(issue) {
    return !issue || !issue.isBlocking;
  },

  /**
   * Returns true to hide the non-blocking issue UI, false to show it.
   *
   * @param {?media_router.Issue} issue
   * @private
   */
  computeIsNonBlockingIssueHidden_: function(issue) {
    return !issue || issue.isBlocking;
  },

  /**
   * @param {?media_router.Issue} issue
   * @return {boolean} Whether or not to hide the non-blocking issue UI.
   * @private
   */
  computeOptionalActionHidden_: function(issue) {
    return !issue || !issue.secondaryActionType;
  },

  /**
   * Fires an issue-action-click event.
   *
   * @param {number} actionType The type of issue action.
   * @private
   */
  fireIssueActionClick_: function(actionType) {
    this.fire('issue-action-click', {
      id: this.issue.id,
      actionType: actionType,
      helpPageId: this.issue.helpPageId
    });
  },

  /**
   * Called when a default issue action is clicked.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onClickDefaultAction_: function(event) {
    this.fireIssueActionClick_(this.issue.defaultActionType);
  },

  /**
   * Called when an optional issue action is clicked.
   *
   * @param {!Event} event The event object.
   * @private
   */
  onClickOptAction_: function(event) {
    this.fireIssueActionClick_(this.issue.secondaryActionType);
  },

  /**
   * Called when |issue| is updated. This updates the default and secondary
   * action button text.
   *
   * @private
   */
  updateActionButtonText_: function() {
    var defaultText = '';
    var secondaryText = '';
    if (this.issue) {
      defaultText = loadTimeData.getString(
          this.issueActionTypeToButtonTextResource_[
          this.issue.defaultActionType]);

      if (this.issue.secondaryActionType) {
        secondaryText = loadTimeData.getString(
            this.issueActionTypeToButtonTextResource_[
            this.issue.secondaryActionType]);
      }
    }

    this.defaultActionButtonText_ = defaultText;
    this.secondaryActionButtonText_ = secondaryText;
  },
});
