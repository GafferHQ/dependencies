// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides infrastructure for dispatching messasges from host
// resource, inlcuding reply messages or unsolicited replies. Normal IPC Reply
// handlers can't take extra parameters. We want to take a
// ResourceMessageReplyParams as a parameter.

#ifndef PPAPI_PROXY_DISPATCH_REPLY_MESSAGE_H_
#define PPAPI_PROXY_DISPATCH_REPLY_MESSAGE_H_

#include "base/callback.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {
namespace proxy {

class ResourceMessageReplyParams;

template <class ObjT, class Method>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const base::Tuple<>& arg) {
  (obj->*method)(params);
}

template <class ObjT, class Method, class A>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const base::Tuple<A>& arg) {
  (obj->*method)(params, base::get<0>(arg));
}

template<class ObjT, class Method, class A, class B>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const base::Tuple<A, B>& arg) {
  (obj->*method)(params, base::get<0>(arg), base::get<1>(arg));
}

template<class ObjT, class Method, class A, class B, class C>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const base::Tuple<A, B, C>& arg) {
  (obj->*method)(params, base::get<0>(arg), base::get<1>(arg),
                 base::get<2>(arg));
}

template<class ObjT, class Method, class A, class B, class C, class D>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const base::Tuple<A, B, C, D>& arg) {
  (obj->*method)(params, base::get<0>(arg), base::get<1>(arg),
                 base::get<2>(arg), base::get<3>(arg));
}

template<class ObjT, class Method, class A, class B, class C, class D, class E>
inline void DispatchResourceReply(ObjT* obj, Method method,
                                  const ResourceMessageReplyParams& params,
                                  const base::Tuple<A, B, C, D, E>& arg) {
  (obj->*method)(params, base::get<0>(arg), base::get<1>(arg),
                 base::get<2>(arg), base::get<3>(arg), base::get<4>(arg));
}

// Used to dispatch resource replies. In most cases, you should not call this
// function to dispatch a resource reply manually, but instead use
// |PluginResource::CallBrowser|/|PluginResource::CallRenderer| with a
// |base::Callback| which will be called when a reply message is received
// (see plugin_resource.h).
//
// This function will call your callback with the nested reply message's
// parameters on success. On failure, your callback will be called with each
// parameter having its default constructed value.
//
// Resource replies are a bit weird in that the host will automatically
// generate a reply in error cases (when the call handler returns error rather
// than returning "completion pending"). This makes it more convenient to write
// the call message handlers. But this also means that the reply handler has to
// handle both the success case (when all of the reply message paramaters are
// specified) and the error case (when the nested reply message is empty).
// In both cases the resource will want to issue completion callbacks to the
// plugin.
//
// This function handles the error case by calling your reply handler with the
// default value for each paramater in the error case. In most situations this
// will be the right thing. You should always dispatch completion callbacks
// using the result code present in the ResourceMessageReplyParams.
template<class MsgClass, class ObjT, class Method>
void DispatchResourceReplyOrDefaultParams(
    ObjT* obj,
    Method method,
    const ResourceMessageReplyParams& reply_params,
    const IPC::Message& msg) {
  typename MsgClass::Schema::Param msg_params;
  // We either expect the nested message type to match, or that there is no
  // nested message. No nested message indicates a default reply sent from
  // the host: when the resource message handler returns an error, a reply
  // is implicitly sent with no nested message.
  DCHECK(msg.type() == MsgClass::ID || msg.type() == 0)
      << "Resource reply message of unexpected type.";
  if (msg.type() == MsgClass::ID && MsgClass::Read(&msg, &msg_params)) {
    // Message type matches and the parameters were successfully read.
    DispatchResourceReply(obj, method, reply_params, msg_params);
  } else {
    // The nested message is empty because the host handler didn't explicitly
    // send a reply (likely), or you screwed up and didn't use the correct
    // message type when calling this function (you should have hit the
    // assertion above, Einstein).
    //
    // Dispatch the reply function with the default parameters. We explicitly
    // use a new Params() structure since if the Read failed due to an invalid
    // message, the params could have been partially filled in.
    DispatchResourceReply(obj, method, reply_params,
        typename MsgClass::Schema::Param());
  }
}

// Template specialization for |Callback|s that only accept a
// |ResourceMessageReplyParams|. In this case |msg| shouldn't contain any
// arguments, so just call the |method| with the |reply_params|.
template<class MsgClass, class Method>
void DispatchResourceReplyOrDefaultParams(
    base::Callback<void(const ResourceMessageReplyParams&)>* obj,
    Method method,
    const ResourceMessageReplyParams& reply_params,
    const IPC::Message& msg) {
  DCHECK(msg.type() == MsgClass::ID || msg.type() == 0)
      << "Resource reply message of unexpected type.";
  (obj->*method)(reply_params);
}

// When using PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL* below, use this macro to
// begin the map instead of IPC_BEGIN_MESSAGE_MAP. The reason is that the macros
// in src/ipc are all closely tied together, and there might be errors for
// unused variables or other errors if they're used with these macros.
#define PPAPI_BEGIN_MESSAGE_MAP(class_name, msg) \
  { \
    typedef class_name _IpcMessageHandlerClass ALLOW_UNUSED_TYPE; \
    const IPC::Message& ipc_message__ = msg; \
    switch (ipc_message__.type()) { \

// Note that this only works for message with 1 or more parameters. For
// 0-parameter messages you need to use the _0 version below (since there are
// no params in the message).
#define PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(msg_class, member_func) \
    case msg_class::ID: { \
      msg_class::Schema::Param p; \
      if (msg_class::Read(&ipc_message__, &p)) { \
        ppapi::proxy::DispatchResourceReply( \
            this, \
            &_IpcMessageHandlerClass::member_func, \
            params, p); \
      } else { \
        NOTREACHED(); \
      } \
      break; \
    }

#define PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_0(msg_class, member_func) \
  case msg_class::ID: { \
    member_func(params); \
    break; \
  }

#define PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(code) \
    default: { \
        code; \
    } \
    break;

#define PPAPI_END_MESSAGE_MAP() \
  } \
}

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_DISPATCH_REPLY_MESSAGE_H_
