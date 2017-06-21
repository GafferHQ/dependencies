// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/embedded_test_server.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server_connection_listener.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {
namespace test_server {

namespace {

class CustomHttpResponse : public HttpResponse {
 public:
  CustomHttpResponse(const std::string& headers, const std::string& contents)
      : headers_(headers), contents_(contents) {
  }

  std::string ToResponseString() const override {
    return headers_ + "\r\n" + contents_;
  }

 private:
  std::string headers_;
  std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(CustomHttpResponse);
};

// Handles |request| by serving a file from under |server_root|.
scoped_ptr<HttpResponse> HandleFileRequest(
    const base::FilePath& server_root,
    const HttpRequest& request) {
  // This is a test-only server. Ignore I/O thread restrictions.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  std::string relative_url(request.relative_url);
  // A proxy request will have an absolute path. Simulate the proxy by stripping
  // the scheme, host, and port.
  GURL relative_gurl(relative_url);
  if (relative_gurl.is_valid())
    relative_url = relative_gurl.PathForRequest();

  // Trim the first byte ('/').
  std::string request_path = relative_url.substr(1);

  // Remove the query string if present.
  size_t query_pos = request_path.find('?');
  if (query_pos != std::string::npos)
    request_path = request_path.substr(0, query_pos);

  base::FilePath file_path(server_root.AppendASCII(request_path));
  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents))
    return scoped_ptr<HttpResponse>();

  base::FilePath headers_path(
      file_path.AddExtension(FILE_PATH_LITERAL("mock-http-headers")));

  if (base::PathExists(headers_path)) {
    std::string headers_contents;
    if (!base::ReadFileToString(headers_path, &headers_contents))
      return scoped_ptr<HttpResponse>();

    scoped_ptr<CustomHttpResponse> http_response(
        new CustomHttpResponse(headers_contents, file_contents));
    return http_response.Pass();
  }

  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_code(HTTP_OK);
  http_response->set_content(file_contents);
  return http_response.Pass();
}

}  // namespace

HttpListenSocket::HttpListenSocket(const SocketDescriptor socket_descriptor,
                                   StreamListenSocket::Delegate* delegate)
    : TCPListenSocket(socket_descriptor, delegate) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HttpListenSocket::Listen() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TCPListenSocket::Listen();
}

void HttpListenSocket::ListenOnIOThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
#if !defined(OS_POSIX)
  // This method may be called after the IO thread is changed, thus we need to
  // call |WatchSocket| again to make sure it listens on the current IO thread.
  // Only needed for non POSIX platforms, since on POSIX platforms
  // StreamListenSocket::Listen already calls WatchSocket inside the function.
  WatchSocket(WAITING_ACCEPT);
#endif
  Listen();
}

HttpListenSocket::~HttpListenSocket() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HttpListenSocket::DetachFromThread() {
  thread_checker_.DetachFromThread();
}

EmbeddedTestServer::EmbeddedTestServer()
    : connection_listener_(nullptr), port_(0), weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
}

EmbeddedTestServer::~EmbeddedTestServer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (Started() && !ShutdownAndWaitUntilComplete()) {
    LOG(ERROR) << "EmbeddedTestServer failed to shut down.";
  }
}

void EmbeddedTestServer::SetConnectionListener(
    EmbeddedTestServerConnectionListener* listener) {
  DCHECK(!Started());
  connection_listener_ = listener;
}

bool EmbeddedTestServer::InitializeAndWaitUntilReady() {
  StartThread();
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!PostTaskToIOThreadAndWait(base::Bind(
          &EmbeddedTestServer::InitializeOnIOThread, base::Unretained(this)))) {
    return false;
  }
  return Started() && base_url_.is_valid();
}

void EmbeddedTestServer::StopThread() {
  DCHECK(io_thread_ && io_thread_->IsRunning());

#if defined(OS_LINUX)
  const int thread_count =
      base::GetNumberOfThreads(base::GetCurrentProcessHandle());
#endif

  io_thread_->Stop();
  io_thread_.reset();
  thread_checker_.DetachFromThread();
  listen_socket_->DetachFromThread();

#if defined(OS_LINUX)
  // Busy loop to wait for thread count to decrease. This is needed because
  // pthread_join does not guarantee that kernel stat is updated when it
  // returns. Thus, GetNumberOfThreads does not immediately reflect the stopped
  // thread and hits the thread number DCHECK in render_sandbox_host_linux.cc
  // in browser_tests.
  while (thread_count ==
         base::GetNumberOfThreads(base::GetCurrentProcessHandle())) {
    base::PlatformThread::YieldCurrentThread();
  }
#endif
}

void EmbeddedTestServer::RestartThreadAndListen() {
  StartThread();
  CHECK(PostTaskToIOThreadAndWait(base::Bind(
      &EmbeddedTestServer::ListenOnIOThread, base::Unretained(this))));
}

bool EmbeddedTestServer::ShutdownAndWaitUntilComplete() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return PostTaskToIOThreadAndWait(base::Bind(
      &EmbeddedTestServer::ShutdownOnIOThread, base::Unretained(this)));
}

void EmbeddedTestServer::StartThread() {
  DCHECK(!io_thread_.get());
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.reset(new base::Thread("EmbeddedTestServer io thread"));
  CHECK(io_thread_->StartWithOptions(thread_options));
  CHECK(io_thread_->WaitUntilThreadStarted());
}

void EmbeddedTestServer::InitializeOnIOThread() {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  DCHECK(!Started());

  SocketDescriptor socket_descriptor =
      TCPListenSocket::CreateAndBindAnyPort("127.0.0.1", &port_);
  if (socket_descriptor == kInvalidSocket)
    return;

  listen_socket_.reset(new HttpListenSocket(socket_descriptor, this));
  listen_socket_->Listen();

  IPEndPoint address;
  int result = listen_socket_->GetLocalAddress(&address);
  if (result == OK) {
    base_url_ = GURL(std::string("http://") + address.ToString());
  } else {
    LOG(ERROR) << "GetLocalAddress failed: " << ErrorToString(result);
  }
}

void EmbeddedTestServer::ListenOnIOThread() {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  DCHECK(Started());
  listen_socket_->ListenOnIOThread();
}

void EmbeddedTestServer::ShutdownOnIOThread() {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());

  listen_socket_.reset();
  STLDeleteContainerPairSecondPointers(connections_.begin(),
                                       connections_.end());
  connections_.clear();
}

void EmbeddedTestServer::HandleRequest(HttpConnection* connection,
                               scoped_ptr<HttpRequest> request) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());

  bool request_handled = false;

  for (size_t i = 0; i < request_handlers_.size(); ++i) {
    scoped_ptr<HttpResponse> response =
        request_handlers_[i].Run(*request.get());
    if (response.get()) {
      connection->SendResponse(response.Pass());
      request_handled = true;
      break;
    }
  }

  if (!request_handled) {
    LOG(WARNING) << "Request not handled. Returning 404: "
                 << request->relative_url;
    scoped_ptr<BasicHttpResponse> not_found_response(new BasicHttpResponse);
    not_found_response->set_code(HTTP_NOT_FOUND);
    connection->SendResponse(not_found_response.Pass());
  }

  // Drop the connection, since we do not support multiple requests per
  // connection.
  connections_.erase(connection->socket_.get());
  delete connection;
}

GURL EmbeddedTestServer::GetURL(const std::string& relative_url) const {
  DCHECK(Started()) << "You must start the server first.";
  DCHECK(base::StartsWith(relative_url, "/", base::CompareCase::SENSITIVE))
      << relative_url;
  return base_url_.Resolve(relative_url);
}

GURL EmbeddedTestServer::GetURL(
    const std::string& hostname,
    const std::string& relative_url) const {
  GURL local_url = GetURL(relative_url);
  GURL::Replacements replace_host;
  replace_host.SetHostStr(hostname);
  return local_url.ReplaceComponents(replace_host);
}

bool EmbeddedTestServer::GetAddressList(net::AddressList* address_list) const {
  if (!listen_socket_)
    return false;
  IPEndPoint endpoint;
  int result = listen_socket_->GetLocalAddress(&endpoint);
  if (result != OK)
    return false;

  *address_list = net::AddressList(endpoint);
  return true;
}

void EmbeddedTestServer::ServeFilesFromDirectory(
    const base::FilePath& directory) {
  RegisterRequestHandler(base::Bind(&HandleFileRequest, directory));
}

void EmbeddedTestServer::RegisterRequestHandler(
    const HandleRequestCallback& callback) {
  request_handlers_.push_back(callback);
}

void EmbeddedTestServer::DidAccept(StreamListenSocket* server,
                                   scoped_ptr<StreamListenSocket> connection) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  if (connection_listener_)
    connection_listener_->AcceptedSocket(*connection);

  HttpConnection* http_connection = new HttpConnection(
      connection.Pass(), base::Bind(&EmbeddedTestServer::HandleRequest,
                                    weak_factory_.GetWeakPtr()));
  // TODO(szym): Make HttpConnection the StreamListenSocket delegate.
  connections_[http_connection->socket_.get()] = http_connection;
}

void EmbeddedTestServer::DidRead(StreamListenSocket* connection,
                                 const char* data,
                                 int length) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());
  if (connection_listener_)
    connection_listener_->ReadFromSocket(*connection);

  HttpConnection* http_connection = FindConnection(connection);
  if (http_connection == NULL) {
    LOG(WARNING) << "Unknown connection.";
    return;
  }
  http_connection->ReceiveData(std::string(data, length));
}

void EmbeddedTestServer::DidClose(StreamListenSocket* connection) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());

  HttpConnection* http_connection = FindConnection(connection);
  if (http_connection == NULL) {
    LOG(WARNING) << "Unknown connection.";
    return;
  }
  delete http_connection;
  connections_.erase(connection);
}

HttpConnection* EmbeddedTestServer::FindConnection(
    StreamListenSocket* socket) {
  DCHECK(io_thread_->task_runner()->BelongsToCurrentThread());

  std::map<StreamListenSocket*, HttpConnection*>::iterator it =
      connections_.find(socket);
  if (it == connections_.end()) {
    return NULL;
  }
  return it->second;
}

bool EmbeddedTestServer::PostTaskToIOThreadAndWait(
    const base::Closure& closure) {
  // Note that PostTaskAndReply below requires
  // base::ThreadTaskRunnerHandle::Get() to return a task runner for posting
  // the reply task. However, in order to make EmbeddedTestServer universally
  // usable, it needs to cope with the situation where it's running on a thread
  // on which a message loop is not (yet) available or as has been destroyed
  // already.
  //
  // To handle this situation, create temporary message loop to support the
  // PostTaskAndReply operation if the current thread as no message loop.
  scoped_ptr<base::MessageLoop> temporary_loop;
  if (!base::MessageLoop::current())
    temporary_loop.reset(new base::MessageLoop());

  base::RunLoop run_loop;
  if (!io_thread_->task_runner()->PostTaskAndReply(FROM_HERE, closure,
                                                   run_loop.QuitClosure())) {
    return false;
  }
  run_loop.Run();

  return true;
}

}  // namespace test_server
}  // namespace net
