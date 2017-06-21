// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides output services for ChromeVox.
 */

goog.provide('Output');
goog.provide('Output.EventType');

goog.require('AutomationUtil.Dir');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('cursors.Unit');
goog.require('cvox.AbstractEarcons');
goog.require('cvox.NavBraille');
goog.require('cvox.Spannable');
goog.require('cvox.ValueSelectionSpan');
goog.require('cvox.ValueSpan');

goog.scope(function() {
var Dir = AutomationUtil.Dir;

/**
 * An Output object formats a cursors.Range into speech, braille, or both
 * representations. This is typically a cvox.Spannable.
 *
 * The translation from Range to these output representations rely upon format
 * rules which specify how to convert AutomationNode objects into annotated
 * strings.
 * The format of these rules is as follows.
 *
 * $ prefix: used to substitute either an attribute or a specialized value from
 *     an AutomationNode. Specialized values include role and state.
 *     For example, $value $role $enabled
 * @ prefix: used to substitute a message. Note the ability to specify params to
 *     the message.  For example, '@tag_html' '@selected_index($text_sel_start,
 *     $text_sel_end').
 * = suffix: used to specify substitution only if not previously appended.
 *     For example, $name= would insert the name attribute only if no name
 * attribute had been inserted previously.
 * @constructor
 */
Output = function() {
  // TODO(dtseng): Include braille specific rules.
  /** @type {!Array<cvox.Spannable>} */
  this.buffer_ = [];
  /** @type {!Array<cvox.Spannable>} */
  this.brailleBuffer_ = [];
  /** @type {!Array<Object>} */
  this.locations_ = [];
  /** @type {function(?)} */
  this.speechEndCallback_;

  /**
   * Current global options.
   * @type {{speech: boolean, braille: boolean, location: boolean}}
   */
  this.formatOptions_ = {speech: true, braille: false, location: true};

  /**
   * Speech properties to apply to the entire output.
   * @type {!Object<*>}
   */
  this.speechProperties_ = {};
};

/**
 * Delimiter to use between output values.
 * @type {string}
 */
Output.SPACE = ' ';

/**
 * Metadata about supported automation roles.
 * @const {Object<{msgId: string,
 *                 earconId: (string|undefined),
 *                 inherits: (string|undefined)}>}
 * msgId: the message id of the role.
 * earconId: an optional earcon to play when encountering the role.
 * inherits: inherits rules from this role.
 * @private
 */
Output.ROLE_INFO_ = {
  alert: {
    msgId: 'aria_role_alert',
    earconId: 'ALERT_NONMODAL',
  },
  alertDialog: {
    msgId: 'aria_role_alertdialog'
  },
  article: {
    msgId: 'aria_role_article',
    inherits: 'abstractContainer'
  },
  application: {
    msgId: 'aria_role_application',
    inherits: 'abstractContainer'
  },
  banner: {
    msgId: 'aria_role_banner',
    inherits: 'abstractContainer'
  },
  button: {
    msgId: 'tag_button',
    earconId: 'BUTTON'
  },
  buttonDropDown: {
    msgId: 'tag_button',
    earconId: 'BUTTON'
  },
  cell: {
    msgId: 'aria_role_gridcell'
  },
  checkBox: {
    msgId: 'input_type_checkbox'
  },
  columnHeader: {
    msgId: 'aria_role_columnheader',
    inherits: 'abstractContainer'
  },
  comboBox: {
    msgId: 'aria_role_combobox'
  },
  complementary: {
    msgId: 'aria_role_complementary',
    inherits: 'abstractContainer'
  },
  contentInfo: {
    msgId: 'aria_role_contentinfo',
    inherits: 'abstractContainer'
  },
  date: {
    msgId: 'input_type_date',
    inherits: 'abstractContainer'
  },
  definition: {
    msgId: 'aria_role_definition',
    inherits: 'abstractContainer'
  },
  dialog: {
    msgId: 'dialog'
  },
  directory: {
    msgId: 'aria_role_directory',
    inherits: 'abstractContainer'
  },
  document: {
    msgId: 'aria_role_document',
    inherits: 'abstractContainer'
  },
  form: {
    msgId: 'aria_role_form',
    inherits: 'abstractContainer'
  },
  grid: {
    msgId: 'aria_role_grid'
  },
  group: {
    msgId: 'aria_role_group'
  },
  heading: {
    msgId: 'aria_role_heading',
  },
  image: {
    msgId: 'aria_role_img',
  },
  link: {
    msgId: 'tag_link',
    earconId: 'LINK'
  },
  listBox: {
    msgId: 'aria_role_listbox',
    earconId: 'LISTBOX'
  },
  listBoxOption: {
    msgId: 'aria_role_listitem',
    earconId: 'LIST_ITEM'
  },
  listItem: {
    msgId: 'aria_role_listitem',
    earconId: 'LIST_ITEM'
  },
  log: {
    msgId: 'aria_role_log',
  },
  main: {
    msgId: 'aria_role_main',
    inherits: 'abstractContainer'
  },
  marquee: {
    msgId: 'aria_role_marquee',
  },
  math: {
    msgId: 'aria_role_math',
    inherits: 'abstractContainer'
  },
  menu: {
    msgId: 'aria_role_menu'
  },
  menuBar: {
    msgId: 'aria_role_menubar',
  },
  menuItem: {
    msgId: 'aria_role_menuitem'
  },
  menuItemCheckBox: {
    msgId: 'aria_role_menuitemcheckbox'
  },
  menuItemRadio: {
    msgId: 'aria_role_menuitemradio'
  },
  menuListOption: {
    msgId: 'aria_role_menuitem'
  },
  menuListPopup: {
    msgId: 'aria_role_menu'
  },
  navigation: {
    msgId: 'aria_role_navigation',
    inherits: 'abstractContainer'
  },
  note: {
    msgId: 'aria_role_note',
    inherits: 'abstractContainer'
  },
  popUpButton: {
    msgId: 'tag_button',
    earcon: 'LISTBOX'
  },
  radioButton: {
    msgId: 'input_type_radio'
  },
  radioGroup: {
    msgId: 'aria_role_radiogroup',
  },
  region: {
    msgId: 'aria_role_region',
    inherits: 'abstractContainer'
  },
  rowHeader: {
    msgId: 'aria_role_rowheader',
    inherits: 'abstractContainer'
  },
  scrollBar: {
    msgId: 'aria_role_scrollbar',
  },
  search: {
    msgId: 'aria_role_search',
    inherits: 'abstractContainer'
  },
  separator: {
    msgId: 'aria_role_separator',
    inherits: 'abstractContainer'
  },
  spinButton: {
    msgId: 'aria_role_spinbutton',
    earconId: 'LISTBOX'
  },
  status: {
    msgId: 'aria_role_status'
  },
  tab: {
    msgId: 'aria_role_tab'
  },
  tabList: {
    msgId: 'aria_role_tablist'
  },
  tabPanel: {
    msgId: 'aria_role_tabpanel'
  },
  textBox: {
    msgId: 'input_type_text',
    earconId: 'EDITABLE_TEXT'
  },
  textField: {
    msgId: 'input_type_text',
    earconId: 'EDITABLE_TEXT'
  },
  time: {
    msgId: 'tag_time',
    inherits: 'abstractContainer'
  },
  timer: {
    msgId: 'aria_role_timer'
  },
  toolbar: {
    msgId: 'aria_role_toolbar'
  },
  tree: {
    msgId: 'aria_role_tree'
  },
  treeItem: {
    msgId: 'aria_role_treeitem'
  }
};

/**
 * Metadata about supported automation states.
 * @const {!Object<{on: {msgId: string, earconId: string},
 *                  off: {msgId: string, earconId: string},
 *                  omitted: {msgId: string, earconId: string}}>}
 *     on: info used to describe a state that is set to true.
 *     off: info used to describe a state that is set to false.
 *     omitted: info used to describe a state that is undefined.
 * @private
 */
Output.STATE_INFO_ = {
  checked: {
    on: {
      earconId: 'CHECK_ON',
      msgId: 'checkbox_checked_state'
    },
    off: {
      earconId: 'CHECK_OFF',
      msgId: 'checkbox_unchecked_state'
    },
    omitted: {
      earconId: 'CHECK_OFF',
      msgId: 'checkbox_unchecked_state'
    }
  },
  collapsed: {
    on: {
      msgId: 'aria_expanded_false'
    },
    off: {
      msgId: 'aria_expanded_true'
    }
  },
  expanded: {
    on: {
      msgId: 'aria_expanded_true'
    },
    off: {
      msgId: 'aria_expanded_false'
    }
  },
  visited: {
    on: {
      msgId: 'visited_state'
    }
  }
};

/**
 * Rules specifying format of AutomationNodes for output.
 * @type {!Object<Object<Object<string>>>}
 */
Output.RULES = {
  navigate: {
    'default': {
      speak: '$name $value $description $help $role',
      braille: ''
    },
    abstractContainer: {
      enter: '$name $role',
      leave: '@exited_container($role)'
    },
    alert: {
      speak: '!doNotInterrupt $role $descendants'
    },
    alertDialog: {
      enter: '$name $role $descendants'
    },
    cell: {
      enter: '@column_granularity $tableCellColumnIndex'
    },
    checkBox: {
      speak: '$name $role $checked'
    },
    dialog: {
      enter: '$name $role'
    },
    grid: {
      enter: '$name $role'
    },
    heading: {
      enter: '@tag_h+$hierarchicalLevel',
      speak: '@tag_h+$hierarchicalLevel $nameOrDescendants='
    },
    inlineTextBox: {
      speak: '$value='
    },
    link: {
      enter: '$name $visited $role',
      stay: '$name= $visited $role',
      speak: '$name= $visited $role'
    },
    list: {
      enter: '$role @list_with_items($countChildren(listItem))'
    },
    listBox: {
      enter: '$name $role @list_with_items($countChildren(listBoxOption))'
    },
    listBoxOption: {
      speak: '$name $role @describe_index($indexInParent, $parentChildCount)'
    },
    listItem: {
      enter: '$role'
    },
    menu: {
      enter: '$name $role @list_with_items($countChildren(menuItem))'
    },
    menuItem: {
      speak: '$if($haspopup, @describe_menu_item_with_submenu($name), ' +
          '@describe_menu_item($name)) ' +
          '@describe_index($indexInParent, $parentChildCount)'
    },
    menuListOption: {
      speak: '$name $value @aria_role_menuitem ' +
          '@describe_index($indexInParent, $parentChildCount)'
    },
    paragraph: {
      speak: '$value'
    },
    popUpButton: {
      speak: '$value $name $role @aria_has_popup ' +
          '$if($collapsed, @aria_expanded_false, @aria_expanded_true)'
    },
    radioButton: {
      speak: '$if($checked, @describe_radio_selected($name), ' +
          '@describe_radio_unselected($name))'
    },
    radioGroup: {
      enter: '$name $role'
    },
    row: {
      enter: '@row_granularity $tableRowIndex'
    },
    slider: {
      speak: '@describe_slider($value, $name) $help'
    },
    staticText: {
      speak: '$value $name'
    },
    tab: {
      speak: '@describe_tab($name)'
    },
    textField: {
      speak: '$name $value $if(' +
          '$type, @input_type_+$type, @input_type_text)',
      braille: ''
    },
    toolbar: {
      enter: '$name $role'
    },
    tree: {
      enter: '$name $role @list_with_items($countChildren(treeItem))'
    },
    treeItem: {
        enter: '$role $expanded $collapsed ' +
            '@describe_index($indexInParent, $parentChildCount) ' +
            '@describe_depth($hierarchicalLevel)'
    },
    window: {
      enter: '$name',
      speak: '@describe_window($name) $earcon(OBJECT_OPEN)'
    }
  },
  menuStart: {
    'default': {
      speak: '@chrome_menu_opened($name)  $earcon(OBJECT_OPEN)'
    }
  },
  menuEnd: {
    'default': {
      speak: '@chrome_menu_closed $earcon(OBJECT_CLOSE)'
    }
  },
  menuListValueChanged: {
    'default': {
      speak: '$value $name ' +
          '$find({"state": {"selected": true, "invisible": false}}, ' +
              '@describe_index($indexInParent, $parentChildCount)) '
    }
  },
  alert: {
    default: {
      speak: '!doNotInterrupt ' +
          '@aria_role_alert $name $earcon(ALERT_NONMODAL) $descendants'
    }
  }
};

/**
 * Custom actions performed while rendering an output string.
 * @param {function()} action
 * @constructor
 */
Output.Action = function(action) {
  this.action_ = action;
};

Output.Action.prototype = {
  run: function() {
    this.action_();
  }
};

/**
 * Action to play a earcon.
 * @param {string} earconId
 * @constructor
 * @extends {Output.Action}
 */
Output.EarconAction = function(earconId) {
  Output.Action.call(this, function() {
    cvox.ChromeVox.earcons.playEarcon(
        cvox.AbstractEarcons[earconId]);
  });
};

Output.EarconAction.prototype = {
  __proto__: Output.Action.prototype
};

/**
 * Annotation for selection.
 * @param {number} startIndex
 * @param {number} endIndex
 * @constructor
 */
Output.SelectionSpan = function(startIndex, endIndex) {
  // TODO(dtseng): Direction lost below; should preserve for braille panning.
  this.startIndex = startIndex < endIndex ? startIndex : endIndex;
  this.endIndex = endIndex > startIndex ? endIndex : startIndex;
};

/**
 * Possible events handled by ChromeVox internally.
 * @enum {string}
 */
Output.EventType = {
  NAVIGATE: 'navigate'
};

Output.prototype = {
  /**
   * Gets the output buffer for speech.
   * @param {string=} opt_separator Used to join components of the output.
   * @return {!cvox.Spannable}
   */
  toSpannable: function(opt_separator) {
    opt_separator = opt_separator || '';
    return this.buffer_.reduce(function(prev, cur) {
      if (prev === null)
        return cur;
      prev.append(opt_separator);
      prev.append(cur);
      return prev;
    }, null);
  },

  /**
   * Gets the output buffer for speech with separator '|'.
   * @return {!cvox.Spannable}
   */
  toSpannableForTest: function() {
    return this.toSpannable('|');
  },

  /**
   * Specify ranges for speech.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|Output.EventType} type
   * @return {!Output}
   */
  withSpeech: function(range, prevRange, type) {
    this.formatOptions_ = {speech: true, braille: false, location: true};
    this.render_(range, prevRange, type, this.buffer_);
    return this;
  },

  /**
   * Specify ranges for braille.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|Output.EventType} type
   * @return {!Output}
   */
  withBraille: function(range, prevRange, type) {
    this.formatOptions_ = {speech: false, braille: true, location: false};
    this.render_(range, prevRange, type, this.brailleBuffer_);
    return this;
  },

  /**
   * Specify the same ranges for speech and braille.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|Output.EventType} type
   * @return {!Output}
   */
  withSpeechAndBraille: function(range, prevRange, type) {
    this.withSpeech(range, prevRange, type);
    this.withBraille(range, prevRange, type);
    return this;
  },

  /**
   * Apply a format string directly to the output buffer. This lets you
   * output a message directly to the buffer using the format syntax.
   * @param {string} formatStr
   * @return {!Output}
   */
  format: function(formatStr) {
    this.formatOptions_ = {speech: true, braille: false, location: true};
    this.format_(null, formatStr, this.buffer_);

    this.formatOptions_ = {speech: false, braille: true, location: false};
    this.format_(null, formatStr, this.brailleBuffer_);

    return this;
  },

  /**
   * Triggers callback for a speech event.
   * @param {function()} callback
   */
  onSpeechEnd: function(callback) {
    this.speechEndCallback_ = function(opt_cleanupOnly) {
      if (!opt_cleanupOnly)
        callback();
    }.bind(this);
    return this;
  },

  /**
   * Executes all specified output.
   */
  go: function() {
    // Speech.
    var queueMode = cvox.QueueMode.FLUSH;
    this.buffer_.forEach(function(buff, i, a) {
      if (buff.toString()) {
        (function() {
          var scopedBuff = buff;
          this.speechProperties_['startCallback'] = function() {
            var actions = scopedBuff.getSpansInstanceOf(Output.Action);
            if (actions) {
              actions.forEach(function(a) {
                a.run();
              });
            }
          };
        }.bind(this)());

        if (this.speechEndCallback_ && i == a.length - 1)
          this.speechProperties_['endCallback'] = this.speechEndCallback_;
        else
          this.speechProperties_['endCallback'] = null;
        cvox.ChromeVox.tts.speak(
            buff.toString(), queueMode, this.speechProperties_);
        queueMode = cvox.QueueMode.QUEUE;
      }
    }.bind(this));

    // Braille.
    var buff = this.brailleBuffer_.reduce(function(prev, cur) {
      if (prev.getLength() > 0 && cur.getLength() > 0)
        prev.append(Output.SPACE);
      prev.append(cur);
      return prev;
    }, new cvox.Spannable());

    var selSpan =
        buff.getSpanInstanceOf(Output.SelectionSpan);
    var startIndex = -1, endIndex = -1;
    if (selSpan) {
      // Casts ok, since the span is known to be in the spannable.
      var valueStart =
          /** @type {number} */ (buff.getSpanStart(selSpan));
      var valueEnd =
          /** @type {number} */ (buff.getSpanEnd(selSpan));
      startIndex = valueStart + selSpan.startIndex;
      endIndex = valueStart + selSpan.endIndex;
        buff.setSpan(new cvox.ValueSpan(0),
                                  valueStart, valueEnd);
      buff.setSpan(new cvox.ValueSelectionSpan(),
                                  startIndex, endIndex);
    }

    var output = new cvox.NavBraille({
      text: buff,
      startIndex: startIndex,
      endIndex: endIndex
    });

    if (this.brailleBuffer_)
      cvox.ChromeVox.braille.write(output);

    // Display.
    chrome.accessibilityPrivate.setFocusRing(this.locations_);
  },

  /**
   * Renders the given range using optional context previous range and event
   * type.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|string} type
   * @param {!Array<cvox.Spannable>} buff Buffer to receive rendered output.
   * @private
   */
  render_: function(range, prevRange, type, buff) {
    if (range.isSubNode())
      this.subNode_(range, prevRange, type, buff);
    else
      this.range_(range, prevRange, type, buff);
  },

  /**
   * Format the node given the format specifier.
   * @param {chrome.automation.AutomationNode} node
   * @param {string|!Object} format The output format either specified as an
   * output template string or a parsed output format tree.
   * @param {!Array<cvox.Spannable>} buff Buffer to receive rendered output.
   * @param {!Object=} opt_exclude A set of attributes to exclude.
   * @private
   */
  format_: function(node, format, buff, opt_exclude) {
    opt_exclude = opt_exclude || {};
    var tokens = [];
    var args = null;

    // Hacky way to support args.
    if (typeof(format) == 'string') {
      format = format.replace(/([,:])\W/g, '$1');
      tokens = format.split(' ');
    } else {
      tokens = [format];
    }

    tokens.forEach(function(token) {
      // Ignore empty tokens.
      if (!token)
        return;

      // Parse the token.
      var tree;
      if (typeof(token) == 'string')
        tree = this.createParseTree(token);
      else
        tree = token;

      // Obtain the operator token.
      token = tree.value;

      // Set suffix options.
      var options = {};
      options.annotation = [];
      options.isUnique = token[token.length - 1] == '=';
      if (options.isUnique)
        token = token.substring(0, token.length - 1);

      // Process token based on prefix.
      var prefix = token[0];
      token = token.slice(1);

      if (opt_exclude[token])
        return;

      // All possible tokens based on prefix.
      if (prefix == '$') {
        if (token == 'value') {
          var text = node.value;
          if (text !== undefined) {
            if (node.textSelStart !== undefined) {
              options.annotation.push(new Output.SelectionSpan(
                  node.textSelStart,
                  node.textSelEnd));
            }
          }
          // Annotate this as a name so we don't duplicate names from ancestors.
          if (node.role == chrome.automation.RoleType.inlineTextBox)
            token = 'name';
          options.annotation.push(token);
          this.append_(buff, text, options);
        } else if (token == 'name') {
          options.annotation.push(token);
          var earconFinder = node;
          while (earconFinder) {
            var info = Output.ROLE_INFO_[earconFinder.role];
            if (info && info.earconId) {
              options.annotation.push(
                  new Output.EarconAction(info.earconId));
              break;
            }
            earconFinder = earconFinder.parent;
          }

          // Pending finalization of name calculation; we must use the
          // attributes property to access aria-label. See crbug.com/473220.
          node.attributes = node.attributes || {};
          var resolvedName = node.name || node.attributes['aria-label'];
          this.append_(buff, resolvedName, options);
        } else if (token == 'nameOrDescendants') {
          options.annotation.push(token);
          if (node.name)
            this.append_(buff, node.name, options);
          else
            this.format_(node, '$descendants', buff);
        } else if (token == 'indexInParent') {
          options.annotation.push(token);
          this.append_(buff, String(node.indexInParent + 1));
        } else if (token == 'parentChildCount') {
          options.annotation.push(token);
          if (node.parent)
          this.append_(buff, String(node.parent.children.length));
        } else if (token == 'state') {
          options.annotation.push(token);
          Object.getOwnPropertyNames(node.state).forEach(function(s) {
            this.append_(buff, s, options);
          }.bind(this));
        } else if (token == 'find') {
          // Find takes two arguments: JSON query string and format string.
          if (tree.firstChild) {
            var jsonQuery = tree.firstChild.value;
            node = node.find(
                /** @type {Object}*/(JSON.parse(jsonQuery)));
            var formatString = tree.firstChild.nextSibling;
            if (node)
              this.format_(node, formatString, buff);
          }
        } else if (token == 'descendants') {
          if (AutomationPredicate.leaf(node))
            return;

          // Construct a range to the leftmost and rightmost leaves.
          var leftmost = AutomationUtil.findNodePre(
              node, Dir.FORWARD, AutomationPredicate.leaf);
          var rightmost = AutomationUtil.findNodePre(
              node, Dir.BACKWARD, AutomationPredicate.leaf);
          if (!leftmost || !rightmost)
            return;

          var subrange = new cursors.Range(
              new cursors.Cursor(leftmost, 0),
              new cursors.Cursor(rightmost, 0));
          var prev = null;
          if (node)
            prev = cursors.Range.fromNode(node);
          this.range_(subrange, prev, 'navigate', buff);
        } else if (token == 'role') {
          options.annotation.push(token);
          var msg = node.role;
          var info = Output.ROLE_INFO_[node.role];
          if (info) {
            if (this.formatOptions_.braille)
              msg = cvox.ChromeVox.msgs.getMsg(info.msgId + '_brl');
            else
              msg = cvox.ChromeVox.msgs.getMsg(info.msgId);
          } else {
            console.error('Missing role info for ' + node.role);
          }
          this.append_(buff, msg, options);
        } else if (token == 'tableRowIndex' ||
            token == 'tableCellColumnIndex') {
          var value = node[token];
          if (!value)
            return;
          value = String(value + 1);
          options.annotation.push(token);
          this.append_(buff, value, options);
        } else if (node[token] !== undefined) {
          options.annotation.push(token);
          var value = node[token];
          if (typeof value == 'number')
            value = String(value);
          this.append_(buff, value, options);
        } else if (Output.STATE_INFO_[token]) {
          options.annotation.push('state');
          var stateInfo = Output.STATE_INFO_[token];
          var resolvedInfo = {};
          if (node.state[token] === undefined)
            resolvedInfo = stateInfo.omitted;
          else
            resolvedInfo = node.state[token] ? stateInfo.on : stateInfo.off;
          if (!resolvedInfo)
            return;
          if (resolvedInfo.earconId) {
            options.annotation.push(
                new Output.EarconAction(resolvedInfo.earconId));
          }
          var msgId =
              this.formatOptions_.braille ? resolvedInfo.msgId + '_brl' :
              resolvedInfo.msgId;
          var msg = cvox.ChromeVox.msgs.getMsg(msgId);
          this.append_(buff, msg, options);
        } else if (tree.firstChild) {
          // Custom functions.
          if (token == 'if') {
            var cond = tree.firstChild;
            var attrib = cond.value.slice(1);
            if (node[attrib] || node.state[attrib])
              this.format_(node, cond.nextSibling, buff);
            else
              this.format_(node, cond.nextSibling.nextSibling, buff);
          } else if (token == 'earcon') {
            // Assumes there's existing output in our buffer.
            var lastBuff = buff[buff.length - 1];
            if (!lastBuff)
              return;

            lastBuff.setSpan(
                new Output.EarconAction(tree.firstChild.value), 0, 0);
          } else if (token == 'countChildren') {
            var role = tree.firstChild.value;
            var count = node.children.filter(function(e) {
              return e.role == role;
            }).length;
            this.append_(buff, String(count));
          }
        }
      } else if (prefix == '@') {
        // Tokens can have substitutions.
        var pieces = token.split('+');
        token = pieces.reduce(function(prev, cur) {
          var lookup = cur;
          if (cur[0] == '$')
            lookup = node[cur.slice(1)];
          return prev + lookup;
        }.bind(this), '');
        var msgId = token;
        var msgArgs = [];
        var curMsg = tree.firstChild;

        while (curMsg) {
          var arg = curMsg.value;
          if (arg[0] != '$') {
            console.error('Unexpected value: ' + arg);
            return;
          }
          var msgBuff = [];
          this.format_(node, curMsg, msgBuff);
          msgArgs = msgArgs.concat(msgBuff);
          curMsg = curMsg.nextSibling;
        }
          var msg = cvox.ChromeVox.msgs.getMsg(msgId, msgArgs);
        try {
          if (this.formatOptions_.braille)
            msg = cvox.ChromeVox.msgs.getMsg(msgId + '_brl', msgArgs) || msg;
        } catch(e) {}

        if (msg) {
          this.append_(buff, msg, options);
        }
      } else if (prefix == '!') {
        this.speechProperties_[token] = true;
      }
    }.bind(this));
  },

  /**
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|string} type
   * @param {!Array<cvox.Spannable>} rangeBuff
   * @private
   */
  range_: function(range, prevRange, type, rangeBuff) {
    if (!prevRange)
      prevRange = cursors.Range.fromNode(range.getStart().getNode().root);

    var cursor = range.getStart();
    var prevNode = prevRange.getStart().getNode();

    var formatNodeAndAncestors = function(node, prevNode) {
      var buff = [];
      this.ancestry_(node, prevNode, type, buff);
      this.node_(node, prevNode, type, buff);
      if (this.formatOptions_.location)
        this.locations_.push(node.location);
      return buff;
    }.bind(this);

    while (cursor.getNode() != range.getEnd().getNode()) {
      var node = cursor.getNode();
        rangeBuff.push.apply(rangeBuff, formatNodeAndAncestors(node, prevNode));
      prevNode = node;
      cursor = cursor.move(cursors.Unit.NODE,
                           cursors.Movement.DIRECTIONAL,
                           Dir.FORWARD);

      // Reached a boundary.
      if (cursor.getNode() == prevNode)
        break;
    }
    var lastNode = range.getEnd().getNode();
    rangeBuff.push.apply(rangeBuff, formatNodeAndAncestors(lastNode, prevNode));
  },

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.AutomationNode} prevNode
   * @param {chrome.automation.EventType|string} type
   * @param {!Array<cvox.Spannable>} buff
   * @param {!Object=} opt_exclude A list of attributes to exclude from
   * processing.
   * @private
   */
  ancestry_: function(node, prevNode, type, buff, opt_exclude) {
    opt_exclude = opt_exclude || {};
    var prevUniqueAncestors =
        AutomationUtil.getUniqueAncestors(node, prevNode);
    var uniqueAncestors = AutomationUtil.getUniqueAncestors(prevNode, node);

    // First, look up the event type's format block.
    // Navigate is the default event.
    var eventBlock = Output.RULES[type] || Output.RULES['navigate'];

    var getMergedRoleBlock = function(role) {
      var parentRole = (Output.ROLE_INFO_[role] || {}).inherits;
      var roleBlock = eventBlock[role] || eventBlock['default'];
      var parentRoleBlock = parentRole ? eventBlock[parentRole] : {};
      var mergedRoleBlock = {};
      for (var key in parentRoleBlock)
        mergedRoleBlock[key] = parentRoleBlock[key];
      for (var key in roleBlock)
        mergedRoleBlock[key] = roleBlock[key];
      return mergedRoleBlock;
    };

    for (var i = 0, formatPrevNode;
         (formatPrevNode = prevUniqueAncestors[i]);
         i++) {
      var roleBlock = getMergedRoleBlock(formatPrevNode.role);
      if (roleBlock.leave)
        this.format_(formatPrevNode, roleBlock.leave, buff, opt_exclude);
    }

    var enterOutputs = [];
    var enterRole = {};
    for (var j = uniqueAncestors.length - 2, formatNode;
         (formatNode = uniqueAncestors[j]);
         j--) {
      var roleBlock = getMergedRoleBlock(formatNode.role);
      if (roleBlock.enter) {
        if (enterRole[formatNode.role])
          continue;
        enterRole[formatNode.role] = true;
        var tempBuff = [];
        this.format_(formatNode, roleBlock.enter, tempBuff, opt_exclude);
        enterOutputs.unshift(tempBuff);
      }
      if (formatNode.role == 'window')
        break;
    }
    enterOutputs.forEach(function(b) {
      buff.push.apply(buff, b);
    });

    if (!opt_exclude.stay) {
      var commonFormatNode = uniqueAncestors[0];
      while (commonFormatNode && commonFormatNode.parent) {
        commonFormatNode = commonFormatNode.parent;
        var roleBlock =
            eventBlock[commonFormatNode.role] || eventBlock['default'];
        if (roleBlock.stay)
          this.format_(commonFormatNode, roleBlock.stay, buff, opt_exclude);
      }
    }
  },

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.AutomationNode} prevNode
   * @param {chrome.automation.EventType|string} type
   * @param {!Array<cvox.Spannable>} buff
   * @private
   */
  node_: function(node, prevNode, type, buff) {
    // Navigate is the default event.
    var eventBlock = Output.RULES[type] || Output.RULES['navigate'];
    var roleBlock = eventBlock[node.role] || eventBlock['default'];
    var speakFormat = roleBlock.speak || eventBlock['default'].speak;
    this.format_(node, speakFormat, buff);
  },

  /**
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|string} type
   * @param {!Array<cvox.Spannable>} buff
   * @private
   */
  subNode_: function(range, prevRange, type, buff) {
    if (!prevRange)
      prevRange = range;
    var dir = cursors.Range.getDirection(prevRange, range);
    var prevNode = prevRange.getBound(dir).getNode();
    this.ancestry_(
        range.getStart().getNode(), prevNode, type, buff,
        {stay: true, name: true, value: true});
    var startIndex = range.getStart().getIndex();
    var endIndex = range.getEnd().getIndex();
    if (startIndex === endIndex)
      endIndex++;
    this.append_(
        buff, range.getStart().getText().substring(startIndex, endIndex));
  },

  /**
   * Appends output to the |buff|.
   * @param {!Array<cvox.Spannable>} buff
   * @param {string|!cvox.Spannable} value
   * @param {{isUnique: (boolean|undefined),
   *      annotation: !Array<*>}=} opt_options
   */
  append_: function(buff, value, opt_options) {
    opt_options = opt_options || {isUnique: false, annotation: []};

    // Reject empty values without annotations.
    if ((!value || value.length == 0) && opt_options.annotation.length == 0)
      return;

    var spannableToAdd = new cvox.Spannable(value);
    opt_options.annotation.forEach(function(a) {
      spannableToAdd.setSpan(a, 0, spannableToAdd.getLength());
    });

    // Early return if the buffer is empty.
    if (buff.length == 0) {
      buff.push(spannableToAdd);
      return;
    }

    // |isUnique| specifies an annotation that cannot be duplicated.
    if (opt_options.isUnique) {
      var alreadyAnnotated = buff.some(function(s) {
        return opt_options.annotation.some(function(annotation) {
          return s.getSpanStart(annotation) != undefined;
        });
      });
      if (alreadyAnnotated)
        return;
    }

    buff.push(spannableToAdd);
  },

  /**
   * Parses the token containing a custom function and returns a tree.
   * @param {string} inputStr
   * @return {Object}
   */
  createParseTree: function(inputStr) {
    var root = {value: ''};
    var currentNode = root;
    var index = 0;
    var braceNesting = 0;
    while (index < inputStr.length) {
      if (inputStr[index] == '(') {
        currentNode.firstChild = {value: ''};
        currentNode.firstChild.parent = currentNode;
        currentNode = currentNode.firstChild;
      } else if (inputStr[index] == ')') {
        currentNode = currentNode.parent;
      } else if (inputStr[index] == '{') {
        braceNesting++;
        currentNode.value += inputStr[index];
      } else if (inputStr[index] == '}') {
        braceNesting--;
        currentNode.value += inputStr[index];
      } else if (inputStr[index] == ',' && braceNesting === 0) {
        currentNode.nextSibling = {value: ''};
        currentNode.nextSibling.parent = currentNode.parent;
        currentNode = currentNode.nextSibling;
      } else {
        currentNode.value += inputStr[index];
      }
      index++;
    }

    if (currentNode != root)
      throw 'Unbalanced parenthesis.';

    return root;
  }
};

});  // goog.scope
