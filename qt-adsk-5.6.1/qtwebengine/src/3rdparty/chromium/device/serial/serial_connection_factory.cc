// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_connection_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "device/serial/serial_connection.h"
#include "device/serial/serial_io_handler.h"

namespace device {
namespace {

void FillDefaultConnectionOptions(serial::ConnectionOptions* options) {
  if (!options->bitrate)
    options->bitrate = 9600;
  if (options->data_bits == serial::DATA_BITS_NONE)
    options->data_bits = serial::DATA_BITS_EIGHT;
  if (options->stop_bits == serial::STOP_BITS_NONE)
    options->stop_bits = serial::STOP_BITS_ONE;
  if (options->parity_bit == serial::PARITY_BIT_NONE)
    options->parity_bit = serial::PARITY_BIT_NO;
  if (!options->has_cts_flow_control) {
    options->has_cts_flow_control = true;
    options->cts_flow_control = false;
  }
}

}  // namespace

class SerialConnectionFactory::ConnectTask
    : public base::RefCountedThreadSafe<SerialConnectionFactory::ConnectTask> {
 public:
  ConnectTask(scoped_refptr<SerialConnectionFactory> factory,
              const std::string& path,
              serial::ConnectionOptionsPtr options,
              mojo::InterfaceRequest<serial::Connection> connection_request,
              mojo::InterfaceRequest<serial::DataSink> sink,
              mojo::InterfaceRequest<serial::DataSource> source,
              mojo::InterfacePtr<serial::DataSourceClient> source_client);
  void Run();

 private:
  friend class base::RefCountedThreadSafe<SerialConnectionFactory::ConnectTask>;
  virtual ~ConnectTask();
  void Connect();
  void OnConnected(bool success);

  scoped_refptr<SerialConnectionFactory> factory_;
  const std::string path_;
  serial::ConnectionOptionsPtr options_;
  mojo::InterfaceRequest<serial::Connection> connection_request_;
  mojo::InterfaceRequest<serial::DataSink> sink_;
  mojo::InterfaceRequest<serial::DataSource> source_;
  mojo::InterfacePtr<serial::DataSourceClient> source_client_;
  scoped_refptr<SerialIoHandler> io_handler_;

  DISALLOW_COPY_AND_ASSIGN(ConnectTask);
};

SerialConnectionFactory::SerialConnectionFactory(
    const IoHandlerFactory& io_handler_factory,
    scoped_refptr<base::SingleThreadTaskRunner> connect_task_runner)
    : io_handler_factory_(io_handler_factory),
      connect_task_runner_(connect_task_runner) {
}

void SerialConnectionFactory::CreateConnection(
    const std::string& path,
    serial::ConnectionOptionsPtr options,
    mojo::InterfaceRequest<serial::Connection> connection_request,
    mojo::InterfaceRequest<serial::DataSink> sink,
    mojo::InterfaceRequest<serial::DataSource> source,
    mojo::InterfacePtr<serial::DataSourceClient> source_client) {
  scoped_refptr<ConnectTask> task(
      new ConnectTask(this, path, options.Pass(), connection_request.Pass(),
                      sink.Pass(), source.Pass(), source_client.Pass()));
  task->Run();
}

SerialConnectionFactory::~SerialConnectionFactory() {
}

SerialConnectionFactory::ConnectTask::ConnectTask(
    scoped_refptr<SerialConnectionFactory> factory,
    const std::string& path,
    serial::ConnectionOptionsPtr options,
    mojo::InterfaceRequest<serial::Connection> connection_request,
    mojo::InterfaceRequest<serial::DataSink> sink,
    mojo::InterfaceRequest<serial::DataSource> source,
    mojo::InterfacePtr<serial::DataSourceClient> source_client)
    : factory_(factory),
      path_(path),
      options_(options.Pass()),
      connection_request_(connection_request.Pass()),
      sink_(sink.Pass()),
      source_(source.Pass()),
      source_client_(source_client.Pass()) {
  if (!options_) {
    options_ = serial::ConnectionOptions::New();
  }
  FillDefaultConnectionOptions(options_.get());
}

void SerialConnectionFactory::ConnectTask::Run() {
  factory_->connect_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&SerialConnectionFactory::ConnectTask::Connect, this));
}

SerialConnectionFactory::ConnectTask::~ConnectTask() {
}

void SerialConnectionFactory::ConnectTask::Connect() {
  io_handler_ = factory_->io_handler_factory_.Run();
  io_handler_->Open(
      path_, *options_,
      base::Bind(&SerialConnectionFactory::ConnectTask::OnConnected, this));
}

void SerialConnectionFactory::ConnectTask::OnConnected(bool success) {
  DCHECK(io_handler_.get());
  if (!success) {
    return;
  }

  new SerialConnection(io_handler_, sink_.Pass(), source_.Pass(),
                       source_client_.Pass(), connection_request_.Pass());
}

}  // namespace device
