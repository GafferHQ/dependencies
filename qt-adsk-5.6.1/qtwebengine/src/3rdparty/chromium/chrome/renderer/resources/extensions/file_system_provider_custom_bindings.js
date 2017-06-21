// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the fileSystemProvider API.

var binding = require('binding').Binding.create('fileSystemProvider');
var fileSystemProviderInternal =
    require('binding').Binding.create('fileSystemProviderInternal').generate();
var eventBindings = require('event_bindings');
var fileSystemNatives = requireNative('file_system_natives');
var GetDOMError = fileSystemNatives.GetDOMError;

/**
 * Maximum size of the thumbnail in bytes.
 * @type {number}
 * @const
 */
var METADATA_THUMBNAIL_SIZE_LIMIT = 32 * 1024 * 1024;

/**
 * Regular expression to validate if the thumbnail URI is a valid data URI,
 * taking into account allowed formats.
 * @type {RegExp}
 * @const
 */
var METADATA_THUMBNAIL_FORMAT = new RegExp(
    '^data:image/(png|jpeg|webp);', 'i');

/**
 * Annotates a date with its serialized value.
 * @param {Date} date Input date.
 * @return {Date} Date with an extra <code>value</code> attribute.
 */
function annotateDate(date) {
  // Copy in case the input date is frozen.
  var result = new Date(date.getTime());
  result.value = result.toString();
  return result;
}

/**
 * Verifies if the passed image URI is valid.
 * @param {*} uri Image URI.
 * @return {boolean} True if valid, valse otherwise.
 */
function verifyImageURI(uri) {
  // The URI is specified by a user, so the type may be incorrect.
  if (typeof uri != 'string' && !(uri instanceof String))
    return false;

  return METADATA_THUMBNAIL_FORMAT.test(uri);
}

/**
 * Annotates an entry metadata by serializing its modifiedTime value.
 * @param {EntryMetadata} metadata Input metadata.
 * @return {EntryMetadata} metadata Annotated metadata, which can be passed
 *     back to the C++ layer.
 */
function annotateMetadata(metadata) {
  var result = {
    isDirectory: metadata.isDirectory,
    name: metadata.name,
    size: metadata.size,
    modificationTime: annotateDate(metadata.modificationTime)
  };
  if ('mimeType' in metadata)
    result.mimeType = metadata.mimeType;
  if ('thumbnail' in metadata)
    result.thumbnail = metadata.thumbnail;
  return result;
}

/**
 * Massages arguments of an event raised by the File System Provider API.
 * @param {Array<*>} args Input arguments.
 * @param {function(Array<*>)} dispatch Closure to be called with massaged
 *     arguments.
 */
function massageArgumentsDefault(args, dispatch) {
  var executionStart = Date.now();
  var options = args[0];
  var onSuccessCallback = function(hasNext) {
    fileSystemProviderInternal.operationRequestedSuccess(
        options.fileSystemId, options.requestId, Date.now() - executionStart);
  };
  var onErrorCallback = function(error) {
    fileSystemProviderInternal.operationRequestedError(
        options.fileSystemId, options.requestId, error,
        Date.now() - executionStart);
  }
  dispatch([options, onSuccessCallback, onErrorCallback]);
}

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onUnmountRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onGetMetadataRequested',
    function(args, dispatch) {
      var executionStart = Date.now();
      var options = args[0];
      var onSuccessCallback = function(metadata) {
        var error;
        // It is invalid to return a thumbnail when it's not requested. The
        // restriction is added in order to avoid fetching the thumbnail while
        // it's not needed.
        if (!options.thumbnail && metadata.thumbnail)
          error = 'Thumbnail data provided, but not requested.';

        // Check the format and size. Note, that in the C++ layer, there is
        // another sanity check to avoid passing any evil URL.
        if ('thumbnail' in metadata && !verifyImageURI(metadata.thumbnail))
          error = 'Thumbnail format invalid.';

        if ('thumbnail' in metadata &&
            metadata.thumbnail.length > METADATA_THUMBNAIL_SIZE_LIMIT) {
          error = 'Thumbnail data too large.';
        }

        if (error) {
          console.error(error);
          fileSystemProviderInternal.operationRequestedError(
              options.fileSystemId, options.requestId, 'FAILED',
              Date.now() - executionStart);
          return;
        }

        fileSystemProviderInternal.getMetadataRequestedSuccess(
            options.fileSystemId,
            options.requestId,
            annotateMetadata(metadata),
            Date.now() - executionStart);
      };

      var onErrorCallback = function(error) {
        fileSystemProviderInternal.operationRequestedError(
            options.fileSystemId, options.requestId, error,
            Date.now() - executionStart);
      }

      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onGetActionsRequested',
    function(args, dispatch) {
      var executionStart = Date.now();
      var options = args[0];
      var onSuccessCallback = function(actions) {
        fileSystemProviderInternal.getActionsRequestedSuccess(
            options.fileSystemId,
            options.requestId,
            actions,
            Date.now() - executionStart);
      };

      var onErrorCallback = function(error) {
        fileSystemProviderInternal.operationRequestedError(
            options.fileSystemId, options.requestId, error,
            Date.now() - executionStart);
      }

      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onReadDirectoryRequested',
    function(args, dispatch) {
      var executionStart = Date.now();
      var options = args[0];
      var onSuccessCallback = function(entries, hasNext) {
        var annotatedEntries = entries.map(annotateMetadata);
        // It is invalid to return a thumbnail when it's not requested.
        var error;
        annotatedEntries.forEach(function(metadata) {
          if (metadata.thumbnail) {
            var error =
                'Thumbnails must not be provided when reading a directory.';
            return;
          }
        });

        if (error) {
          console.error(error);
          fileSystemProviderInternal.operationRequestedError(
              options.fileSystemId, options.requestId, 'FAILED',
              Date.now() - executionStart);
          return;
        }

        fileSystemProviderInternal.readDirectoryRequestedSuccess(
            options.fileSystemId, options.requestId, annotatedEntries, hasNext,
            Date.now() - executionStart);
      };

      var onErrorCallback = function(error) {
        fileSystemProviderInternal.operationRequestedError(
            options.fileSystemId, options.requestId, error,
            Date.now() - executionStart);
      }
      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onOpenFileRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onCloseFileRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onReadFileRequested',
    function(args, dispatch) {
      var executionStart = Date.now();
      var options = args[0];
      var onSuccessCallback = function(data, hasNext) {
        fileSystemProviderInternal.readFileRequestedSuccess(
            options.fileSystemId, options.requestId, data, hasNext,
            Date.now() - executionStart);
      };
      var onErrorCallback = function(error) {
        fileSystemProviderInternal.operationRequestedError(
            options.fileSystemId, options.requestId, error,
            Date.now() - executionStart);
      }
      dispatch([options, onSuccessCallback, onErrorCallback]);
    });

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onCreateDirectoryRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onDeleteEntryRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onCreateFileRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onCopyEntryRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onMoveEntryRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onTruncateRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onWriteFileRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onAbortRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onObserveDirectoryRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onUnobserveEntryRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onAddWatcherRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onRemoveWatcherRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onConfigureRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onExecuteActionRequested',
    massageArgumentsDefault);

eventBindings.registerArgumentMassager(
    'fileSystemProvider.onMountRequested',
    function(args, dispatch) {
      var onSuccessCallback = function() {
        // TODO(mtomasz): To be implemented.
      };
      var onErrorCallback = function(error) {
        // TODO(mtomasz): To be implemented.
      }
      dispatch([onSuccessCallback, onErrorCallback]);
    });

exports.binding = binding.generate();
