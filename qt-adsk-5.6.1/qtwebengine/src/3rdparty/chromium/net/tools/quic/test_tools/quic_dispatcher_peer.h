// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_DISPATCHER_PEER_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_DISPATCHER_PEER_H_

#include "net/tools/quic/quic_dispatcher.h"

#include "net/base/ip_endpoint.h"

namespace net {
namespace tools {

class QuicPacketWriterWrapper;

namespace test {

class QuicDispatcherPeer {
 public:
  static void SetTimeWaitListManager(
      QuicDispatcher* dispatcher,
      QuicTimeWaitListManager* time_wait_list_manager);

  // Injects |writer| into |dispatcher| as the shared writer.
  static void UseWriter(QuicDispatcher* dispatcher,
                        QuicPacketWriterWrapper* writer);

  static QuicPacketWriter* GetWriter(QuicDispatcher* dispatcher);

  static void SetPacketWriterFactory(
      QuicDispatcher* dispatcher,
      QuicDispatcher::PacketWriterFactory* packet_writer_factory);

  static QuicConnectionHelperInterface* GetHelper(QuicDispatcher* dispatcher);

  static QuicDispatcher::WriteBlockedList* GetWriteBlockedList(
      QuicDispatcher* dispatcher);

  // Get the dispatcher's record of the last error reported to its framer
  // visitor's OnError() method.  Then set that record to QUIC_NO_ERROR.
  static QuicErrorCode GetAndClearLastError(QuicDispatcher* dispatcher);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicDispatcherPeer);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_DISPATCHER_PEER_H_
