// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/http_connection_impl.h"

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "mojo/common/handle_watcher.h"
#include "mojo/services/network/http_server_impl.h"
#include "mojo/services/network/net_adapters.h"
#include "mojo/services/network/public/cpp/web_socket_read_queue.h"
#include "mojo/services/network/public/cpp/web_socket_write_queue.h"
#include "mojo/services/network/public/interfaces/web_socket.mojom.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/type_converter.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"

namespace mojo {

// SimpleDataPipeReader reads till end-of-file, stores the data in a string and
// notifies completion.
class HttpConnectionImpl::SimpleDataPipeReader {
 public:
  using CompletionCallback =
      base::Callback<void(SimpleDataPipeReader*, scoped_ptr<std::string>)>;

  SimpleDataPipeReader() {}
  ~SimpleDataPipeReader() {}

  void Start(ScopedDataPipeConsumerHandle consumer,
             const CompletionCallback& completion_callback) {
    DCHECK(consumer.is_valid() && !consumer_.is_valid());
    consumer_ = consumer.Pass();
    completion_callback_ = completion_callback;
    buffer_.reset(new std::string);
    ReadMore();
  }

 private:
  void ReadMore() {
    const void* buf;
    uint32_t buf_size;
    MojoResult rv = BeginReadDataRaw(consumer_.get(), &buf, &buf_size,
                                     MOJO_READ_DATA_FLAG_NONE);
    if (rv == MOJO_RESULT_OK) {
      buffer_->append(static_cast<const char*>(buf), buf_size);
      EndReadDataRaw(consumer_.get(), buf_size);
      WaitToReadMore();
    } else if (rv == MOJO_RESULT_SHOULD_WAIT) {
      WaitToReadMore();
    } else if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
      // We reached end-of-file.
      completion_callback_.Run(this, buffer_.Pass());
      // Note: This object may have been destroyed in the callback.
    } else {
      CHECK(false);
    }
  }

  void WaitToReadMore() {
    watcher_.Start(consumer_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                   MOJO_DEADLINE_INDEFINITE,
                   base::Bind(&SimpleDataPipeReader::OnHandleReady,
                              base::Unretained(this)));
  }

  void OnHandleReady(MojoResult result) { ReadMore(); }

  ScopedDataPipeConsumerHandle consumer_;
  common::HandleWatcher watcher_;
  CompletionCallback completion_callback_;
  scoped_ptr<std::string> buffer_;

  DISALLOW_COPY_AND_ASSIGN(SimpleDataPipeReader);
};

class HttpConnectionImpl::WebSocketImpl : public WebSocket,
                                          public ErrorHandler {
 public:
  // |connection| must outlive this object.
  WebSocketImpl(HttpConnectionImpl* connection,
                InterfaceRequest<WebSocket> request,
                ScopedDataPipeConsumerHandle send_stream,
                WebSocketClientPtr client)
      : connection_(connection),
        binding_(this, request.Pass()),
        client_(client.Pass()),
        send_stream_(send_stream.Pass()),
        read_send_stream_(new WebSocketReadQueue(send_stream_.get())),
        pending_send_count_(0) {
    DCHECK(binding_.is_bound());
    DCHECK(client_);
    DCHECK(send_stream_.is_valid());

    binding_.set_error_handler(this);
    client_.set_error_handler(this);

    DataPipe data_pipe;
    receive_stream_ = data_pipe.producer_handle.Pass();
    write_receive_stream_.reset(new WebSocketWriteQueue(receive_stream_.get()));

    client_->DidConnect("", "", data_pipe.consumer_handle.Pass());
  }

  ~WebSocketImpl() override {}

  void Close() {
    DCHECK(!IsClosing());

    binding_.Close();
    client_.reset();

    NotifyOwnerCloseIfAllDone();
  }

  void OnReceivedWebSocketMessage(const std::string& data) {
    if (IsClosing())
      return;

    // TODO(yzshen): It shouldn't be an issue to pass an empty message. However,
    // WebSocket{Read,Write}Queue doesn't handle that correctly.
    if (data.empty())
      return;

    uint32_t size = static_cast<uint32_t>(data.size());
    write_receive_stream_->Write(
        &data[0], size,
        base::Bind(&WebSocketImpl::OnFinishedWritingReceiveStream,
                   base::Unretained(this), size));
  }

 private:
  // WebSocket implementation.
  void Connect(const String& url,
               Array<String> protocols,
               const String& origin,
               ScopedDataPipeConsumerHandle send_stream,
               WebSocketClientPtr client) override {
    NOTREACHED();
  }

  void Send(bool fin, MessageType type, uint32_t num_bytes) override {
    if (!fin || type != MESSAGE_TYPE_TEXT) {
      NOTIMPLEMENTED();
      Close();
    }

    // TODO(yzshen): It shouldn't be an issue to pass an empty message. However,
    // WebSocket{Read,Write}Queue doesn't handle that correctly.
    if (num_bytes == 0)
      return;

    pending_send_count_++;
    read_send_stream_->Read(
        num_bytes, base::Bind(&WebSocketImpl::OnFinishedReadingSendStream,
                              base::Unretained(this), num_bytes));
  }

  void FlowControl(int64_t quota) override { NOTIMPLEMENTED(); }

  void Close(uint16_t code, const String& reason) override {
    Close();
  }

  // ErrorHandler implementation.
  void OnConnectionError() override { Close(); }

  void OnFinishedReadingSendStream(uint32_t num_bytes, const char* data) {
    DCHECK_GT(pending_send_count_, 0u);
    pending_send_count_--;

    if (data) {
      connection_->server_->server()->SendOverWebSocket(
          connection_->connection_id_, std::string(data, num_bytes));
    }

    if (IsClosing())
      NotifyOwnerCloseIfAllDone();
  }

  void OnFinishedWritingReceiveStream(uint32_t num_bytes, const char* buffer) {
    if (IsClosing())
      return;

    if (buffer)
      client_->DidReceiveData(true, MESSAGE_TYPE_TEXT, num_bytes);
  }

  // Checks whether Close() has been called.
  bool IsClosing() const { return !binding_.is_bound(); }

  void NotifyOwnerCloseIfAllDone() {
    DCHECK(IsClosing());

    if (pending_send_count_ == 0)
      connection_->OnWebSocketClosed();
  }

  HttpConnectionImpl* const connection_;

  Binding<WebSocket> binding_;
  WebSocketClientPtr client_;

  ScopedDataPipeConsumerHandle send_stream_;
  scoped_ptr<WebSocketReadQueue> read_send_stream_;
  size_t pending_send_count_;

  ScopedDataPipeProducerHandle receive_stream_;
  scoped_ptr<WebSocketWriteQueue> write_receive_stream_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketImpl);
};

template <>
struct TypeConverter<HttpRequestPtr, net::HttpServerRequestInfo> {
  static HttpRequestPtr Convert(const net::HttpServerRequestInfo& obj) {
    HttpRequestPtr request(HttpRequest::New());
    request->method = obj.method;
    request->url = obj.path;
    request->headers.resize(obj.headers.size());
    size_t index = 0;
    for (const auto& item : obj.headers) {
      HttpHeaderPtr header(HttpHeader::New());
      header->name = item.first;
      header->value = item.second;
      request->headers[index++] = header.Pass();
    }
    if (!obj.data.empty()) {
      uint32_t num_bytes = static_cast<uint32_t>(obj.data.size());
      MojoCreateDataPipeOptions options;
      options.struct_size = sizeof(MojoCreateDataPipeOptions);
      options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
      options.element_num_bytes = 1;
      options.capacity_num_bytes = num_bytes;
      DataPipe data_pipe(options);
      request->body = data_pipe.consumer_handle.Pass();
      MojoResult result =
          WriteDataRaw(data_pipe.producer_handle.get(), obj.data.data(),
                       &num_bytes, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
      CHECK_EQ(MOJO_RESULT_OK, result);
    }
    return request.Pass();
  }
};

HttpConnectionImpl::HttpConnectionImpl(int connection_id,
                                       HttpServerImpl* server,
                                       HttpConnectionDelegatePtr delegate,
                                       HttpConnectionPtr* connection)
    : connection_id_(connection_id),
      server_(server),
      delegate_(delegate.Pass()),
      binding_(this, connection) {
  DCHECK(delegate_);
  binding_.set_error_handler(this);
  delegate_.set_error_handler(this);
}

HttpConnectionImpl::~HttpConnectionImpl() {
  STLDeleteElements(&response_body_readers_);
}

void HttpConnectionImpl::OnReceivedHttpRequest(
    const net::HttpServerRequestInfo& info) {
  if (IsClosing())
    return;

  delegate_->OnReceivedRequest(
      HttpRequest::From(info), [this](HttpResponsePtr response) {
        if (response->body.is_valid()) {
          SimpleDataPipeReader* reader = new SimpleDataPipeReader;
          response_body_readers_.insert(reader);
          ScopedDataPipeConsumerHandle body = response->body.Pass();
          reader->Start(
              body.Pass(),
              base::Bind(&HttpConnectionImpl::OnFinishedReadingResponseBody,
                         base::Unretained(this), base::Passed(&response)));
        } else {
          OnFinishedReadingResponseBody(response.Pass(), nullptr, nullptr);
        }
      });
}

void HttpConnectionImpl::OnReceivedWebSocketRequest(
    const net::HttpServerRequestInfo& info) {
  if (IsClosing())
    return;

  delegate_->OnReceivedWebSocketRequest(
      HttpRequest::From(info),
      [this, info](InterfaceRequest<WebSocket> web_socket,
                   ScopedDataPipeConsumerHandle send_stream,
                   WebSocketClientPtr web_socket_client) {
        if (!web_socket.is_pending() || !send_stream.is_valid() ||
            !web_socket_client) {
          Close();
          return;
        }

        web_socket_.reset(new WebSocketImpl(this, web_socket.Pass(),
                                            send_stream.Pass(),
                                            web_socket_client.Pass()));
        server_->server()->AcceptWebSocket(connection_id_, info);
      });
}

void HttpConnectionImpl::OnReceivedWebSocketMessage(const std::string& data) {
  if (IsClosing())
    return;

  web_socket_->OnReceivedWebSocketMessage(data);
}

void HttpConnectionImpl::SetSendBufferSize(
    uint32_t size,
    const SetSendBufferSizeCallback& callback) {
  if (size > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    size = std::numeric_limits<int32_t>::max();

  server_->server()->SetSendBufferSize(connection_id_,
                                       static_cast<int32_t>(size));
  callback.Run(MakeNetworkError(net::OK));
}

void HttpConnectionImpl::SetReceiveBufferSize(
    uint32_t size,
    const SetReceiveBufferSizeCallback& callback) {
  if (size > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
    size = std::numeric_limits<int32_t>::max();

  server_->server()->SetReceiveBufferSize(connection_id_,
                                          static_cast<int32_t>(size));
  callback.Run(MakeNetworkError(net::OK));
}

void HttpConnectionImpl::OnConnectionError() {
  // This method is called when the proxy side of |binding_| or the impl side of
  // |delegate_| has closed the pipe. Although it is set as error handler for
  // both |binding_| and |delegate_|, it will only be called at most once
  // because when called it closes/resets |binding_| and |delegate_|.
  Close();
}

void HttpConnectionImpl::OnFinishedReadingResponseBody(
    HttpResponsePtr response,
    SimpleDataPipeReader* reader,
    scoped_ptr<std::string> body) {
  if (reader) {
    delete reader;
    response_body_readers_.erase(reader);
  }

  net::HttpServerResponseInfo info(
      static_cast<net::HttpStatusCode>(response->status_code));

  std::string content_type;
  for (size_t i = 0; i < response->headers.size(); ++i) {
    const HttpHeader& header = *(response->headers[i]);

    if (body) {
      // net::HttpServerResponseInfo::SetBody() automatically sets
      // Content-Length and Content-Types, so skip the two here.
      //
      // TODO(yzshen): Consider adding to net::HttpServerResponseInfo a simple
      // setter for body which doesn't fiddle with headers.
      if (base::strcasecmp(header.name.data(),
                           net::HttpRequestHeaders::kContentLength) == 0) {
        continue;
      } else if (base::strcasecmp(header.name.data(),
                                  net::HttpRequestHeaders::kContentType) == 0) {
        content_type = header.value;
        continue;
      }
    }
    info.AddHeader(header.name, header.value);
  }

  if (body)
    info.SetBody(*body, content_type);

  server_->server()->SendResponse(connection_id_, info);

  if (IsClosing())
    NotifyOwnerCloseIfAllDone();
}

void HttpConnectionImpl::Close() {
  DCHECK(!IsClosing());

  binding_.Close();
  delegate_.reset();

  if (web_socket_)
    web_socket_->Close();

  NotifyOwnerCloseIfAllDone();
}

void HttpConnectionImpl::NotifyOwnerCloseIfAllDone() {
  DCHECK(IsClosing());

  // Don't close the connection until all pending sends are done.
  bool should_wait = !response_body_readers_.empty() || web_socket_;
  if (!should_wait)
    server_->server()->Close(connection_id_);
}

void HttpConnectionImpl::OnWebSocketClosed() {
  web_socket_.reset();

  if (IsClosing()) {
    // The close operation is initiated by this object.
    NotifyOwnerCloseIfAllDone();
  } else {
    // The close operation is initiated by |web_socket_|; start closing this
    // object.
    Close();
  }
}

}  // namespace mojo
