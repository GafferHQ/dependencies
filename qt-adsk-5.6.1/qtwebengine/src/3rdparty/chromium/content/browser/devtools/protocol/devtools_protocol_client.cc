// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/devtools_protocol_client.h"

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"

namespace content {

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";
const char kResultParam[] = "result";
const char kErrorParam[] = "error";
const char kErrorCodeParam[] = "code";
const char kErrorMessageParam[] = "message";

// Special values.
const int kStatusOk = -1;
const int kStatusFallThrough = -2;
// JSON RPC 2.0 spec: http://www.jsonrpc.org/specification#error_object
const int kStatusInvalidParams = -32602;
const int kStatusInternalError = -32603;
const int kStatusServerError = -32000;

}  // namespace

// static
const DevToolsCommandId DevToolsProtocolClient::kNoId = -1;

DevToolsProtocolClient::DevToolsProtocolClient(
    const RawMessageCallback& raw_message_callback)
    : raw_message_callback_(raw_message_callback) {
}

DevToolsProtocolClient::~DevToolsProtocolClient() {
}

void DevToolsProtocolClient::SendRawMessage(const std::string& message) {
  raw_message_callback_.Run(message);
}

void DevToolsProtocolClient::SendMessage(const base::DictionaryValue& message) {
  std::string json_message;
  base::JSONWriter::Write(message, &json_message);
  SendRawMessage(json_message);
}

void DevToolsProtocolClient::SendNotification(
    const std::string& method,
    scoped_ptr<base::DictionaryValue> params) {
  base::DictionaryValue notification;
  notification.SetString(kMethodParam, method);
  if (params)
    notification.Set(kParamsParam, params.release());

  SendMessage(notification);
}

void DevToolsProtocolClient::SendSuccess(
    DevToolsCommandId command_id,
    scoped_ptr<base::DictionaryValue> params) {
  base::DictionaryValue response;
  response.SetInteger(kIdParam, command_id);

  response.Set(kResultParam,
      params ? params.release() : new base::DictionaryValue());

  SendMessage(response);
}

bool DevToolsProtocolClient::SendError(DevToolsCommandId command_id,
                                       const Response& response) {
  if (response.status() == kStatusOk ||
      response.status() == kStatusFallThrough) {
    return false;
  }
  base::DictionaryValue dict;
  if (command_id == kNoId)
    dict.Set(kIdParam, base::Value::CreateNullValue());
  else
    dict.SetInteger(kIdParam, command_id);

  base::DictionaryValue* error_object = new base::DictionaryValue();
  error_object->SetInteger(kErrorCodeParam, response.status());
  if (!response.message().empty())
    error_object->SetString(kErrorMessageParam, response.message());

  dict.Set(kErrorParam, error_object);
  SendMessage(dict);
  return true;
}

typedef DevToolsProtocolClient::Response Response;

Response Response::FallThrough() {
  return Response(kStatusFallThrough);
}

Response Response::OK() {
  return Response(kStatusOk);
}

Response Response::InvalidParams(const std::string& param) {
  return Response(kStatusInvalidParams,
      base::StringPrintf("Missing or invalid '%s' parameter", param.c_str()));
}

Response Response::InternalError(const std::string& message) {
  return Response(kStatusInternalError, message);
}

Response Response::ServerError(const std::string& message) {
  return Response(kStatusServerError, message);
}

int Response::status() const {
  return status_;
}

const std::string& Response::message() const {
  return message_;
}

bool Response::IsFallThrough() const {
  return status_ == kStatusFallThrough;
}

Response::Response(int status)
    : status_(status) {
}

Response::Response(int status, const std::string& message)
    : status_(status),
      message_(message) {
}

}  // namespace content
