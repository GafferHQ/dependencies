// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for objects sent from C++ to JS for chrome://downloads.
 * @externs
 */

var downloads = {};

/**
 * The type of the download object. The definition is based on
 * chrome/browser/ui/webui/downloads_dom_handler.cc:CreateDownloadItemValue()
 * @typedef {{by_ext_id: (string|undefined),
 *            by_ext_name: (string|undefined),
 *            danger_type: (string|undefined),
 *            date_string: string,
 *            file_externally_removed: boolean,
 *            file_name: string,
 *            file_path: string,
 *            file_url: string,
 *            id: string,
 *            last_reason_text: (string|undefined),
 *            otr: boolean,
 *            percent: (number|undefined),
 *            progress_status_text: (string|undefined),
 *            received: (number|undefined),
 *            resume: boolean,
 *            retry: boolean,
 *            since_string: string,
 *            started: number,
 *            state: string,
 *            total: number,
 *            url: string}}
 */
downloads.Data;
