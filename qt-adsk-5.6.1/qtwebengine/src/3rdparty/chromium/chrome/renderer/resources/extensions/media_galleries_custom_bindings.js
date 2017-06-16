// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Media Gallery API.

var binding = require('binding').Binding.create('mediaGalleries');
var blobNatives = requireNative('blob_natives');
var mediaGalleriesNatives = requireNative('mediaGalleries');

var blobsAwaitingMetadata = {};
var mediaGalleriesMetadata = {};

function createFileSystemObjectsAndUpdateMetadata(response) {
  var result = [];
  mediaGalleriesMetadata = {};  // Clear any previous metadata.
  if (response) {
    for (var i = 0; i < response.length; i++) {
      var filesystem = mediaGalleriesNatives.GetMediaFileSystemObject(
          response[i].fsid);
      $Array.push(result, filesystem);
      var metadata = response[i];
      delete metadata.fsid;
      mediaGalleriesMetadata[filesystem.name] = metadata;
    }
  }
  return result;
}

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // getMediaFileSystems, addUserSelectedFolder, and addScanResults use a
  // custom callback so that they can instantiate and return an array of file
  // system objects.
  apiFunctions.setCustomCallback('getMediaFileSystems',
                                 function(name, request, callback, response) {
    var result = createFileSystemObjectsAndUpdateMetadata(response);
    if (callback)
      callback(result);
  });

  apiFunctions.setCustomCallback('addScanResults',
      function(name, request, callback, response) {
    var result = createFileSystemObjectsAndUpdateMetadata(response);
    if (callback)
      callback(result);
  });

  apiFunctions.setCustomCallback('addUserSelectedFolder',
      function(name, request, callback, response) {
    var fileSystems = [];
    var selectedFileSystemName = "";
    if (response && 'mediaFileSystems' in response &&
        'selectedFileSystemIndex' in response) {
      fileSystems = createFileSystemObjectsAndUpdateMetadata(
          response['mediaFileSystems']);
      var selectedFileSystemIndex = response['selectedFileSystemIndex'];
      if (selectedFileSystemIndex >= 0) {
        selectedFileSystemName = fileSystems[selectedFileSystemIndex].name;
      }
    }
    if (callback)
      callback(fileSystems, selectedFileSystemName);
  });

  apiFunctions.setCustomCallback('dropPermissionForMediaFileSystem',
      function(name, request, callback, response) {
    var galleryId = response;

    if (galleryId) {
      for (var key in mediaGalleriesMetadata) {
        if (mediaGalleriesMetadata[key].galleryId == galleryId) {
          delete mediaGalleriesMetadata[key];
          break;
        }
      }
    }
    if (callback)
      callback();
  });

  apiFunctions.setHandleRequest('getMediaFileSystemMetadata',
                                function(filesystem) {
    if (filesystem && filesystem.name &&
        filesystem.name in mediaGalleriesMetadata) {
      return mediaGalleriesMetadata[filesystem.name];
    }
    return {
      'name': '',
      'galleryId': '',
      'isRemovable': false,
      'isMediaDevice': false,
      'isAvailable': false,
    };
  });

  apiFunctions.setUpdateArgumentsPostValidate('getMetadata',
      function(mediaFile, options, callback) {
    var blobUuid = blobNatives.GetBlobUuid(mediaFile)
    // Store the blob in a global object to keep its refcount nonzero -- this
    // prevents the object from being garbage collected before any metadata
    // parsing gets to occur (see crbug.com/415792).
    blobsAwaitingMetadata[blobUuid] = mediaFile;
    return [blobUuid, options, callback];
  });

  apiFunctions.setCustomCallback('getMetadata',
      function(name, request, callback, response) {
    if (response.attachedImagesBlobInfo) {
      for (var i = 0; i < response.attachedImagesBlobInfo.length; i++) {
        var blobInfo = response.attachedImagesBlobInfo[i];
        var blob = blobNatives.TakeBrowserProcessBlob(
            blobInfo.blobUUID, blobInfo.type, blobInfo.size);
        response.metadata.attachedImages.push(blob);
      }
    }

    if (callback)
      callback(response.metadata);

    // The UUID was in position 0 in the setUpdateArgumentsPostValidate
    // function.
    var uuid = request.args[0];
    delete blobsAwaitingMetadata[uuid];
  });
});

exports.binding = binding.generate();
