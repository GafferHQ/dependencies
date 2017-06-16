// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The base class for client/server reliable streams.

// It does not contain the entire interface needed by an application to interact
// with a QUIC stream.  Some parts of the interface must be obtained by
// accessing the owning session object.  A subclass of ReliableQuicStream
// connects the object and the application that generates and consumes the data
// of the stream.

// The ReliableQuicStream object has a dependent QuicStreamSequencer object,
// which is given the stream frames as they arrive, and provides stream data in
// order by invoking ProcessRawData().

#ifndef NET_QUIC_RELIABLE_QUIC_STREAM_H_
#define NET_QUIC_RELIABLE_QUIC_STREAM_H_

#include <sys/types.h>

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "net/base/iovec.h"
#include "net/base/net_export.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_flow_controller.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_stream_sequencer.h"
#include "net/quic/quic_types.h"

namespace net {

namespace test {
class ReliableQuicStreamPeer;
}  // namespace test

class QuicSession;

class NET_EXPORT_PRIVATE ReliableQuicStream {
 public:
  ReliableQuicStream(QuicStreamId id,
                     QuicSession* session);

  virtual ~ReliableQuicStream();

  // Sets |fec_policy_| parameter from |session_|'s config.
  void SetFromConfig();

  // Called by the session when a (potentially duplicate) stream frame has been
  // received for this stream.
  virtual void OnStreamFrame(const QuicStreamFrame& frame);

  // Called by the session when the connection becomes writeable to allow the
  // stream to write any pending data.
  virtual void OnCanWrite();

  // Called by the session just before the object is destroyed.
  // The object should not be accessed after OnClose is called.
  // Sends a RST_STREAM with code QUIC_RST_ACKNOWLEDGEMENT if neither a FIN nor
  // a RST_STREAM has been sent.
  virtual void OnClose();

  // Called by the session when the endpoint receives a RST_STREAM from the
  // peer.
  virtual void OnStreamReset(const QuicRstStreamFrame& frame);

  // Called by the session when the endpoint receives or sends a connection
  // close, and should immediately close the stream.
  virtual void OnConnectionClosed(QuicErrorCode error, bool from_peer);

  // Called by the sequencer after ProcessRawData has accepted the final
  // incoming data.
  virtual void OnFinRead();

  // Called by the sequencer to deliver to the subclass blocks of the stream's
  // data in sequential order.
  // ProcessRawData is called when the next sequential block of data becomes
  // available, unless the subclass has left the next block of data in the
  // sequencer.  In that case, the subclass must actively retrieve the data
  // using the sequencer's Readv() or FlushBufferedFrames().
  // Return value is the number of data bytes accepted by the callee, i.e.,
  // the first (return value) bytes of data are removed from the sequencer,
  // and the final data_len-(return value) bytes of data are retained by the
  // sequencer.
  virtual uint32 ProcessRawData(const char* data, uint32 data_len) = 0;

  // Called by the subclass or the sequencer to reset the stream from this
  // end.
  virtual void Reset(QuicRstStreamErrorCode error);

  // Called by the subclass or the sequencer to close the entire connection from
  // this end.
  virtual void CloseConnection(QuicErrorCode error);
  virtual void CloseConnectionWithDetails(QuicErrorCode error,
                                          const std::string& details);

  // Returns the effective priority for the stream.  This value may change
  // during the life of the stream.
  virtual QuicPriority EffectivePriority() const = 0;

  QuicStreamId id() const { return id_; }

  QuicRstStreamErrorCode stream_error() const { return stream_error_; }
  QuicErrorCode connection_error() const { return connection_error_; }

  bool read_side_closed() const { return read_side_closed_; }
  bool write_side_closed() const { return write_side_closed_; }

  uint64 stream_bytes_read() const { return stream_bytes_read_; }
  uint64 stream_bytes_written() const { return stream_bytes_written_; }

  void set_fin_sent(bool fin_sent) { fin_sent_ = fin_sent; }
  void set_rst_sent(bool rst_sent) { rst_sent_ = rst_sent; }

  void set_fec_policy(FecPolicy fec_policy) { fec_policy_ = fec_policy; }
  FecPolicy fec_policy() const { return fec_policy_; }

  // Adjust the flow control window according to new offset in |frame|.
  virtual void OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame);

  // Used in Chrome.
  int num_frames_received() const;
  int num_early_frames_received() const;
  int num_duplicate_frames_received() const;

  QuicFlowController* flow_controller() { return &flow_controller_; }

  // Called when endpoint receives a frame which could increase the highest
  // offset.
  // Returns true if the highest offset did increase.
  bool MaybeIncreaseHighestReceivedOffset(QuicStreamOffset new_offset);
  // Called when bytes are sent to the peer.
  void AddBytesSent(QuicByteCount bytes);
  // Called by the stream sequencer as bytes are consumed from the buffer.
  // If the receive window has dropped below the threshold, then send a
  // WINDOW_UPDATE frame.
  void AddBytesConsumed(QuicByteCount bytes);

  // Updates the flow controller's send window offset and calls OnCanWrite if
  // it was blocked before.
  void UpdateSendWindowOffset(QuicStreamOffset new_offset);

  // Returns true if the stream have received either a RST_STREAM or a FIN -
  // either of which gives a definitive number of bytes which the peer has
  // sent. If this is not true on deletion of the stream object, the session
  // must keep track of the stream's byte offset until a definitive final value
  // arrives.
  bool HasFinalReceivedByteOffset() const {
    return fin_received_ || rst_received_;
  }

  // Returns true if the stream has queued data waiting to write.
  bool HasBufferedData() const;

  // Returns the version of QUIC being used for this stream.
  QuicVersion version() const;

  bool fin_received() const { return fin_received_; }

 protected:
  // Sends as much of 'data' to the connection as the connection will consume,
  // and then buffers any remaining data in queued_data_.
  // If fin is true: if it is immediately passed on to the session,
  // write_side_closed() becomes true, otherwise fin_buffered_ becomes true.
  void WriteOrBufferData(
      base::StringPiece data,
      bool fin,
      QuicAckNotifier::DelegateInterface* ack_notifier_delegate);

  // Sends as many bytes in the first |count| buffers of |iov| to the connection
  // as the connection will consume.
  // If |ack_notifier_delegate| is provided, then it will be notified once all
  // the ACKs for this write have been received.
  // Returns the number of bytes consumed by the connection.
  QuicConsumedData WritevData(
      const struct iovec* iov,
      int iov_count,
      bool fin,
      QuicAckNotifier::DelegateInterface* ack_notifier_delegate);

  // Close the read side of the stream.  Further incoming stream frames will be
  // discarded.  Can be called by the subclass or internally.
  // May cause the stream to be closed.
  virtual void CloseReadSide();

  // Close the write side of the socket.  Further writes will fail.
  // Can be called by the subclass or internally.
  // Does not send a FIN.  May cause the stream to be closed.
  void CloseWriteSide();

  // Helper method that returns FecProtection to use when writing.
  FecProtection GetFecProtection();

  bool fin_buffered() const { return fin_buffered_; }

  const QuicSession* session() const { return session_; }
  QuicSession* session() { return session_; }

  const QuicStreamSequencer* sequencer() const { return &sequencer_; }
  QuicStreamSequencer* sequencer() { return &sequencer_; }

  void DisableConnectionFlowControlForThisStream() {
    stream_contributes_to_connection_flow_control_ = false;
  }

 private:
  friend class test::ReliableQuicStreamPeer;
  friend class QuicStreamUtils;
  class ProxyAckNotifierDelegate;

  struct PendingData {
    PendingData(std::string data_in,
                scoped_refptr<ProxyAckNotifierDelegate> delegate_in);
    ~PendingData();

    // Pending data to be written.
    std::string data;
    // Index of the first byte in data still to be written.
    size_t offset;
    // Delegate that should be notified when the pending data is acked.
    // Can be nullptr.
    scoped_refptr<ProxyAckNotifierDelegate> delegate;
  };

  // Calls MaybeSendBlocked on the stream's flow controller and the connection
  // level flow controller.  If the stream is flow control blocked by the
  // connection-level flow controller but not by the stream-level flow
  // controller, marks this stream as connection-level write blocked.
  void MaybeSendBlocked();

  std::list<PendingData> queued_data_;

  QuicStreamSequencer sequencer_;
  QuicStreamId id_;
  // Pointer to the owning QuicSession object.
  QuicSession* session_;
  // Bytes read and written refer to payload bytes only: they do not include
  // framing, encryption overhead etc.
  uint64 stream_bytes_read_;
  uint64 stream_bytes_written_;

  // Stream error code received from a RstStreamFrame or error code sent by the
  // visitor or sequencer in the RstStreamFrame.
  QuicRstStreamErrorCode stream_error_;
  // Connection error code due to which the stream was closed. |stream_error_|
  // is set to |QUIC_STREAM_CONNECTION_ERROR| when this happens and consumers
  // should check |connection_error_|.
  QuicErrorCode connection_error_;

  // True if the read side is closed and further frames should be rejected.
  bool read_side_closed_;
  // True if the write side is closed, and further writes should fail.
  bool write_side_closed_;

  // True if the subclass has written a FIN with WriteOrBufferData, but it was
  // buffered in queued_data_ rather than being sent to the session.
  bool fin_buffered_;
  // True if a FIN has been sent to the session.
  bool fin_sent_;

  // True if this stream has received (and the sequencer has accepted) a
  // StreamFrame with the FIN set.
  bool fin_received_;

  // True if an RST_STREAM has been sent to the session.
  // In combination with fin_sent_, used to ensure that a FIN and/or a
  // RST_STREAM is always sent to terminate the stream.
  bool rst_sent_;

  // True if this stream has received a RST_STREAM frame.
  bool rst_received_;

  // FEC policy to be used for this stream.
  FecPolicy fec_policy_;

  // Tracks if the session this stream is running under was created by a
  // server or a client.
  Perspective perspective_;

  QuicFlowController flow_controller_;

  // The connection level flow controller. Not owned.
  QuicFlowController* connection_flow_controller_;

  // Special streams, such as the crypto and headers streams, do not respect
  // connection level flow control limits (but are stream level flow control
  // limited).
  bool stream_contributes_to_connection_flow_control_;

  DISALLOW_COPY_AND_ASSIGN(ReliableQuicStream);
};

}  // namespace net

#endif  // NET_QUIC_RELIABLE_QUIC_STREAM_H_
