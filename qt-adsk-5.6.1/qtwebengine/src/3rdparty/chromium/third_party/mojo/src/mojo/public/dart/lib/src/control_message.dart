// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of bindings;

// Handles InterfaceControlMessages for a stub.
class ControlMessageHandler {
  static bool isControlMessage(ServiceMessage message) =>
      _isRun(message) || _isRunOrClose(message);

  static bool _isRun(ServiceMessage message) =>
      (message.header.type == icm.kRunMessageId);

  static bool _isRunOrClose(ServiceMessage message) =>
      (message.header.type == icm.kRunOrClosePipeMessageId);

  static Future<Message> handleMessage(Stub stub,
                                       int interface_version,
                                       ServiceMessage message) {
    assert(isControlMessage(message));
    if (_isRun(message)) {
      return _handleRun(stub, interface_version, message);
    } else {
      assert(_isRunOrClose(message));
      return _handleRunOrClose(stub, interface_version, message);
    }
  }

  static Future<Message> _handleRun(Stub stub,
                                    int interface_version,
                                    ServiceMessage message) {
    // Construct RunMessage response.
    var response = new icm.RunResponseMessageParams();
    response.reserved0 = 16;
    response.reserved1 = 0;
    response.queryVersionResult = new icm.QueryVersionResult();
    response.queryVersionResult.version = interface_version;
    // Return response.
    return new Future.value(
        stub.buildResponseWithId(response,
                                 icm.kRunMessageId,
                                 message.header.requestId,
                                 MessageHeader.kMessageIsResponse));
  }

  static Future<Message> _handleRunOrClose(Stub stub,
                                           int interface_version,
                                           ServiceMessage message) {
    // Deserialize message.
    var params = icm.RunOrClosePipeMessageParams.deserialize(message.payload);
    // Grab required version.
    var requiredVersion = params.requireVersion.version;
    if (interface_version >= requiredVersion) {
      // Stub meets the requirements.
      return;
    }
    // Stub does not implement required version. Close the pipe immediately.
    stub.close(immediate: true);
  }
}
