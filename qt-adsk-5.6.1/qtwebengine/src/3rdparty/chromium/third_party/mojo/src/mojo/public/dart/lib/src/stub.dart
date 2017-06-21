// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

part of bindings;

abstract class Stub extends core.MojoEventStreamListener {
  int _outstandingResponseFutures = 0;
  bool _isClosing = false;
  Completer _closeCompleter;

  Stub.fromEndpoint(core.MojoMessagePipeEndpoint endpoint)
      : super.fromEndpoint(endpoint);

  Stub.fromHandle(core.MojoHandle handle) : super.fromHandle(handle);

  Stub.unbound() : super.unbound();

  Future<Message> handleMessage(ServiceMessage message);

  void handleRead() {
    // Query how many bytes are available.
    var result = endpoint.query();
    assert(result.status.isOk || result.status.isResourceExhausted);
    if (result.bytesRead == 0) {
      throw new MojoCodecError('Unexpected empty message.');
    }

    // Read the data and view as a message.
    var bytes = new ByteData(result.bytesRead);
    var handles = new List<core.MojoHandle>(result.handlesRead);
    result = endpoint.read(bytes, result.bytesRead, handles);
    assert(result.status.isOk || result.status.isResourceExhausted);

    // Prepare the response.
    var message;
    var responseFuture;
    try {
      message = new ServiceMessage.fromMessage(new Message(bytes, handles));
      responseFuture = _isClosing ? null : handleMessage(message);
    } catch (e, s) {
      handles.forEach((h) => h.close());
      rethrow;
    }

    // If there's a response, send it.
    if (responseFuture != null) {
      _outstandingResponseFutures++;
      responseFuture.then((response) {
        _outstandingResponseFutures--;
        if (isOpen) {
          endpoint.write(
              response.buffer, response.buffer.lengthInBytes, response.handles);
          // FailedPrecondition is only used to indicate that the other end of
          // the pipe has been closed. We can ignore the close here and wait for
          // the PeerClosed signal on the event stream.
          assert(endpoint.status.isOk || endpoint.status.isFailedPrecondition);
          if (_isClosing && (_outstandingResponseFutures == 0)) {
            // This was the final response future for which we needed to send
            // a response. It is safe to close.
            super.close().then((_) {
              if (_isClosing) {
                _isClosing = false;
                _closeCompleter.complete(null);
                _closeCompleter = null;
              }
            });
          }
        }
      });
    } else if (_isClosing && (_outstandingResponseFutures == 0)) {
      // We are closing, there is no response to send for this message, and
      // there are no outstanding response futures. Do the close now.
      super.close().then((_) {
        if (_isClosing) {
          _isClosing = false;
          _closeCompleter.complete(null);
          _closeCompleter = null;
        }
      });
    }
  }

  void handleWrite() {
    throw 'Unexpected write signal in client.';
  }

  // NB: |immediate| should only be true when calling close() while handling an
  // exception thrown from handleRead(), e.g. when we receive a malformed
  // message, or when we have received the PEER_CLOSED event.
  @override
  Future close({bool immediate: false}) {
    if (isOpen &&
        !immediate &&
        (isInHandler || (_outstandingResponseFutures > 0))) {
      // Either close() is being called from within handleRead() or
      // handleWrite(), or close() is being called while there are outstanding
      // response futures. Defer the actual close until all response futures
      // have been resolved.
      _isClosing = true;
      _closeCompleter = new Completer();
      return _closeCompleter.future;
    } else {
      return super.close(immediate: immediate).then((_) {
        if (_isClosing) {
          _isClosing = false;
          _closeCompleter.complete(null);
          _closeCompleter = null;
        }
      });
    }
  }

  Message buildResponse(Struct response, int name) {
    var header = new MessageHeader(name);
    return response.serializeWithHeader(header);
  }

  Message buildResponseWithId(Struct response, int name, int id, int flags) {
    var header = new MessageHeader.withRequestId(name, flags, id);
    return response.serializeWithHeader(header);
  }

  String toString() {
    var superString = super.toString();
    return "Stub(${superString})";
  }

  int get version;
}
