// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {number}
 * @const
 */
var FEEDBACK_WIDTH = 500;
/**
 * @type {number}
 * @const
 */
var FEEDBACK_HEIGHT = 585;

var initialFeedbackInfo = null;

// To generate a hashed extension ID, use a sha-256 hash, all in lower case.
// Example:
//   echo -n 'abcdefghijklmnopqrstuvwxyzabcdef' | sha1sum | \
//       awk '{print toupper($1)}'
var whitelistedExtensionIds = [
  '12E618C3C6E97495AAECF2AC12DEB082353241C6', // QuickOffice
  '3727DD3E564B6055387425027AD74C58784ACC15', // QuickOffice
  '2FC374607C2DF285634B67C64A2E356C607091C3', // QuickOffice
  '2843C1E82A9B6C6FB49308FDDF4E157B6B44BC2B', // G+ Photos
  '5B5DA6D054D10DB917AF7D9EAE3C56044D1B0B03', // G+ Photos
  '986913085E3E3C3AFDE9B7A943149C4D3F4C937B', // Feedback Extension
  '7AE714FFD394E073F0294CFA134C9F91DB5FBAA4', // Connectivity Diagnostics
  'C7DA3A55C2355F994D3FDDAD120B426A0DF63843', // Connectivity Diagnostics
  '75E3CFFFC530582C583E4690EF97C70B9C8423B7', // Connectivity Diagnostics
  '32A1BA997F8AB8DE29ED1BA94AAF00CF2A3FEFA7', // Connectivity Diagnostics
  'A291B26E088FA6BA53FFD72F0916F06EBA7C585A', // Chrome OS Recovery Tool
  'D7986543275120831B39EF28D1327552FC343960', // Chrome OS Recovery Tool
  '8EBDF73405D0B84CEABB8C7513C9B9FA9F1DC2CE', // GetHelp app.
  '97B23E01B2AA064E8332EE43A7A85C628AADC3F2', // Chrome Remote Desktop Dev
  '9E527CDA9D7C50844E8A5DB964A54A640AE48F98', // Chrome Remote Desktop Stable
  'DF52618D0B040D8A054D8348D2E84DDEEE5974E7', // Chrome Remote Desktop QA
  '269D721F163E587BC53C6F83553BF9CE2BB143CD', // Chrome Remote Desktop QA backup
  'C449A798C495E6CF7D6AF10162113D564E67AD12', // Chrome Remote Desktop Apps V2
  '981974CD1832B87BE6B21BE78F7249BB501E0DE6', // Play Movies Dev
  '32FD7A816E47392C92D447707A89EB07EEDE6FF7', // Play Movies Nightly
  '3F3CEC4B9B2B5DC2F820CE917AABDF97DB2F5B49', // Play Movies Beta
  'F92FAC70AB68E1778BF62D9194C25979596AA0E6', // Play Movies Stable
  '0F585FB1D0FDFBEBCE1FEB5E9DFFB6DA476B8C9B', // Hangouts Extension
  '2D22CDB6583FD0A13758AEBE8B15E45208B4E9A7', // Hangouts Extension
  '49DA0B9CCEEA299186C6E7226FD66922D57543DC', // Hangouts Extension
  'E7E2461CE072DF036CF9592740196159E2D7C089', // Hangouts Extension
  'A74A4D44C7CFCD8844830E6140C8D763E12DD8F3', // Hangouts Extension
  '312745D9BF916161191143F6490085EEA0434997', // Hangouts Extension
  '53041A2FA309EECED01FFC751E7399186E860B2C', // Hangouts Extension
  '0F42756099D914A026DADFA182871C015735DD95', // Hangouts Extension
  '1B7734733E207CCE5C33BFAA544CA89634BF881F', // GLS nightly
  'E2ACA3D943A3C96310523BCDFD8C3AF68387E6B7', // GLS stable
  '11B478CEC461C766A2DC1E5BEEB7970AE06DC9C2', // http://crbug.com/463552
  '0EFB879311E9EFBB7C45251F89EC655711B1F6ED', // http://crbug.com/463552
  '9193D3A51E2FE33B496CDA53EA330423166E7F02', // http://crbug.com/463552
  'F9119B8B18C7C82B51E7BC6FF816B694F2EC3E89', // http://crbug.com/463552
  'BA007D8D52CC0E2632EFCA03ACD003B0F613FD71', // http://crbug.com/470411
  '5260FA31DE2007A837B7F7B0EB4A47CE477018C8', // http://crbug.com/470411
  '4F4A25F31413D9B9F80E61D096DEB09082515267', // http://crbug.com/470411
  'FBA0DE4D3EFB5485FC03760F01F821466907A743', // http://crbug.com/470411
  'E216473E4D15C5FB14522D32C5F8DEAAB2CECDC6', // http://crbug.com/470411
  '676A08383D875E51CE4C2308D875AE77199F1413', // http://crbug.com/473845
  '869A23E11B308AF45A68CC386C36AADA4BE44A01', // http://crbug.com/473845
  'E9CE07C7EDEFE70B9857B312E88F94EC49FCC30F', // http://crbug.com/473845
  'A4577D8C2AF4CF26F40CBCA83FFA4251D6F6C8F8', // http://crbug.com/478929
  'A8208CCC87F8261AFAEB6B85D5E8D47372DDEA6B', // http://crbug.com/478929
  'B620CF4203315F9F2E046EDED22C7571A935958D', // http://crbug.com/510270
  'B206D8716769728278D2D300349C6CB7D7DE2EF9', // http://crbug.com/510270
];


/**
 * Function to determine whether or not a given extension id is whitelisted to
 * invoke the feedback UI. If the extension is whitelisted, the callback to
 * start the Feedback UI will be called.
 * @param {string} id the id of the sender extension.
 * @param {Function} startFeedbackCallback The callback function that will
 *     will start the feedback UI.
 * @param {Object} feedbackInfo The feedback info object to pass to the
 *     start feedback UI callback.
 */
function senderWhitelisted(id, startFeedbackCallback, feedbackInfo) {
  crypto.subtle.digest('SHA-1', new TextEncoder().encode(id)).then(
      function(hashBuffer) {
    var hashString = '';
    var hashView = new Uint8Array(hashBuffer);
    for (var i = 0; i < hashView.length; ++i) {
      var n = hashView[i];
      hashString += n < 0x10 ? '0' : '';
      hashString += n.toString(16);
    }
    if (whitelistedExtensionIds.indexOf(hashString.toUpperCase()) != -1)
      startFeedbackCallback(feedbackInfo);
  });
}

/**
 * Callback which gets notified once our feedback UI has loaded and is ready to
 * receive its initial feedback info object.
 * @param {Object} request The message request object.
 * @param {Object} sender The sender of the message.
 * @param {function(Object)} sendResponse Callback for sending a response.
 */
function feedbackReadyHandler(request, sender, sendResponse) {
  if (request.ready) {
    chrome.runtime.sendMessage(
        {sentFromEventPage: true, data: initialFeedbackInfo});
  }
}


/**
 * Callback which gets notified if another extension is requesting feedback.
 * @param {Object} request The message request object.
 * @param {Object} sender The sender of the message.
 * @param {function(Object)} sendResponse Callback for sending a response.
 */
function requestFeedbackHandler(request, sender, sendResponse) {
  if (request.requestFeedback)
    senderWhitelisted(sender.id, startFeedbackUI, request.feedbackInfo);
}

/**
 * Callback which starts up the feedback UI.
 * @param {Object} feedbackInfo Object containing any initial feedback info.
 */
function startFeedbackUI(feedbackInfo) {
  initialFeedbackInfo = feedbackInfo;
  var win = chrome.app.window.get('default_window');
  if (win) {
    win.show();
    return;
  }
  chrome.app.window.create('html/default.html', {
      frame: 'none',
      id: 'default_window',
      width: FEEDBACK_WIDTH,
      height: FEEDBACK_HEIGHT,
      hidden: true,
      resizable: false },
      function(appWindow) {});
}

chrome.runtime.onMessage.addListener(feedbackReadyHandler);
chrome.runtime.onMessageExternal.addListener(requestFeedbackHandler);
chrome.feedbackPrivate.onFeedbackRequested.addListener(startFeedbackUI);
