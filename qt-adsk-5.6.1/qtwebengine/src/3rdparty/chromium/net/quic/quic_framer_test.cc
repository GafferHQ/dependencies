// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_framer.h"

#include <stdint.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_framer_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/test/gtest_util.h"

using base::hash_set;
using base::StringPiece;
using std::make_pair;
using std::map;
using std::numeric_limits;
using std::pair;
using std::string;
using std::vector;
using testing::Return;
using testing::Truly;
using testing::_;

namespace net {
namespace test {

const QuicPacketSequenceNumber kEpoch = UINT64_C(1) << 48;
const QuicPacketSequenceNumber kMask = kEpoch - 1;

// Index into the connection_id offset in the header.
const size_t kConnectionIdOffset = kPublicFlagsSize;
// Index into the version string in the header. (if present).
const size_t kVersionOffset = kConnectionIdOffset + PACKET_8BYTE_CONNECTION_ID;

// Size in bytes of the stream frame fields for an arbitrary StreamID and
// offset and the last frame in a packet.
size_t GetMinStreamFrameSize() {
  return kQuicFrameTypeSize + kQuicMaxStreamIdSize + kQuicMaxStreamOffsetSize;
}

// Index into the sequence number offset in the header.
size_t GetSequenceNumberOffset(QuicConnectionIdLength connection_id_length,
                               bool include_version) {
  return kConnectionIdOffset + connection_id_length +
      (include_version ? kQuicVersionSize : 0);
}

size_t GetSequenceNumberOffset(bool include_version) {
  return GetSequenceNumberOffset(PACKET_8BYTE_CONNECTION_ID, include_version);
}

// Index into the private flags offset in the data packet header.
size_t GetPrivateFlagsOffset(QuicConnectionIdLength connection_id_length,
                             bool include_version) {
  return GetSequenceNumberOffset(connection_id_length, include_version) +
      PACKET_6BYTE_SEQUENCE_NUMBER;
}

size_t GetPrivateFlagsOffset(bool include_version) {
  return GetPrivateFlagsOffset(PACKET_8BYTE_CONNECTION_ID, include_version);
}

size_t GetPrivateFlagsOffset(bool include_version,
                             QuicSequenceNumberLength sequence_number_length) {
  return GetSequenceNumberOffset(PACKET_8BYTE_CONNECTION_ID, include_version) +
      sequence_number_length;
}

// Index into the fec group offset in the header.
size_t GetFecGroupOffset(QuicConnectionIdLength connection_id_length,
                         bool include_version) {
  return GetPrivateFlagsOffset(connection_id_length, include_version) +
      kPrivateFlagsSize;
}

size_t GetFecGroupOffset(bool include_version) {
  return GetPrivateFlagsOffset(PACKET_8BYTE_CONNECTION_ID, include_version) +
      kPrivateFlagsSize;
}

size_t GetFecGroupOffset(bool include_version,
                         QuicSequenceNumberLength sequence_number_length) {
  return GetPrivateFlagsOffset(include_version, sequence_number_length) +
      kPrivateFlagsSize;
}

// Index into the message tag of the public reset packet.
// Public resets always have full connection_ids.
const size_t kPublicResetPacketMessageTagOffset =
    kConnectionIdOffset + PACKET_8BYTE_CONNECTION_ID;

class TestEncrypter : public QuicEncrypter {
 public:
  ~TestEncrypter() override {}
  bool SetKey(StringPiece key) override { return true; }
  bool SetNoncePrefix(StringPiece nonce_prefix) override { return true; }
  bool EncryptPacket(QuicPacketSequenceNumber sequence_number,
                     StringPiece associated_data,
                     StringPiece plaintext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override {
    sequence_number_ = sequence_number;
    associated_data_ = associated_data.as_string();
    plaintext_ = plaintext.as_string();
    memcpy(output, plaintext.data(), plaintext.length());
    *output_length = plaintext.length();
    return true;
  }
  size_t GetKeySize() const override { return 0; }
  size_t GetNoncePrefixSize() const override { return 0; }
  size_t GetMaxPlaintextSize(size_t ciphertext_size) const override {
    return ciphertext_size;
  }
  size_t GetCiphertextSize(size_t plaintext_size) const override {
    return plaintext_size;
  }
  StringPiece GetKey() const override { return StringPiece(); }
  StringPiece GetNoncePrefix() const override { return StringPiece(); }
  QuicPacketSequenceNumber sequence_number_;
  string associated_data_;
  string plaintext_;
};

class TestDecrypter : public QuicDecrypter {
 public:
  ~TestDecrypter() override {}
  bool SetKey(StringPiece key) override { return true; }
  bool SetNoncePrefix(StringPiece nonce_prefix) override { return true; }
  bool DecryptPacket(QuicPacketSequenceNumber sequence_number,
                     const StringPiece& associated_data,
                     const StringPiece& ciphertext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override {
    sequence_number_ = sequence_number;
    associated_data_ = associated_data.as_string();
    ciphertext_ = ciphertext.as_string();
    memcpy(output, ciphertext.data(), ciphertext.length());
    *output_length = ciphertext.length();
    return true;
  }
  StringPiece GetKey() const override { return StringPiece(); }
  StringPiece GetNoncePrefix() const override { return StringPiece(); }
  QuicPacketSequenceNumber sequence_number_;
  string associated_data_;
  string ciphertext_;
};

class TestQuicVisitor : public QuicFramerVisitorInterface {
 public:
  TestQuicVisitor()
      : error_count_(0),
        version_mismatch_(0),
        packet_count_(0),
        frame_count_(0),
        fec_count_(0),
        complete_packets_(0),
        revived_packets_(0),
        accept_packet_(true),
        accept_public_header_(true) {
  }

  ~TestQuicVisitor() override {
    STLDeleteElements(&stream_frames_);
    STLDeleteElements(&ack_frames_);
    STLDeleteElements(&stop_waiting_frames_);
    STLDeleteElements(&ping_frames_);
    STLDeleteElements(&fec_data_);
    STLDeleteElements(&stream_data_);
    STLDeleteElements(&fec_data_redundancy_);
  }

  void OnError(QuicFramer* f) override {
    DVLOG(1) << "QuicFramer Error: " << QuicUtils::ErrorToString(f->error())
             << " (" << f->error() << ")";
    ++error_count_;
  }

  void OnPacket() override {}

  void OnPublicResetPacket(const QuicPublicResetPacket& packet) override {
    public_reset_packet_.reset(new QuicPublicResetPacket(packet));
  }

  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override {
    version_negotiation_packet_.reset(new QuicVersionNegotiationPacket(packet));
  }

  void OnRevivedPacket() override { ++revived_packets_; }

  bool OnProtocolVersionMismatch(QuicVersion version) override {
    DVLOG(1) << "QuicFramer Version Mismatch, version: " << version;
    ++version_mismatch_;
    return true;
  }

  bool OnUnauthenticatedPublicHeader(
      const QuicPacketPublicHeader& header) override {
    public_header_.reset(new QuicPacketPublicHeader(header));
    return accept_public_header_;
  }

  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override {
    return true;
  }

  void OnDecryptedPacket(EncryptionLevel level) override {}

  bool OnPacketHeader(const QuicPacketHeader& header) override {
    ++packet_count_;
    header_.reset(new QuicPacketHeader(header));
    return accept_packet_;
  }

  bool OnStreamFrame(const QuicStreamFrame& frame) override {
    ++frame_count_;
    // Save a copy of the data so it is valid after the packet is processed.
    string* string_data = new string();
    frame.data.AppendToString(string_data);
    stream_data_.push_back(string_data);
    QuicStreamFrame* stream_frame = new QuicStreamFrame(frame);
    // Make sure that the stream frame points to this data.
    stream_frame->data = StringPiece(*string_data);
    stream_frames_.push_back(stream_frame);
    return true;
  }

  void OnFecProtectedPayload(StringPiece payload) override {
    fec_protected_payload_ = payload.as_string();
  }

  bool OnAckFrame(const QuicAckFrame& frame) override {
    ++frame_count_;
    ack_frames_.push_back(new QuicAckFrame(frame));
    return true;
  }

  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override {
    ++frame_count_;
    stop_waiting_frames_.push_back(new QuicStopWaitingFrame(frame));
    return true;
  }

  bool OnPingFrame(const QuicPingFrame& frame) override {
    ++frame_count_;
    ping_frames_.push_back(new QuicPingFrame(frame));
    return true;
  }

  void OnFecData(const QuicFecData& fec) override {
    ++fec_count_;
    QuicFecData* fec_data = new QuicFecData();
    fec_data->fec_group = fec.fec_group;
    // Save a copy of the data so it is valid after the packet is processed.
    string* redundancy = new string(fec.redundancy.as_string());
    fec_data_redundancy_.push_back(redundancy);
    fec_data->redundancy = StringPiece(*redundancy);
    fec_data_.push_back(fec_data);
  }

  void OnPacketComplete() override { ++complete_packets_; }

  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override {
    rst_stream_frame_ = frame;
    return true;
  }

  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override {
    connection_close_frame_ = frame;
    return true;
  }

  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override {
    goaway_frame_ = frame;
    return true;
  }

  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override {
    window_update_frame_ = frame;
    return true;
  }

  bool OnBlockedFrame(const QuicBlockedFrame& frame) override {
    blocked_frame_ = frame;
    return true;
  }

  // Counters from the visitor_ callbacks.
  int error_count_;
  int version_mismatch_;
  int packet_count_;
  int frame_count_;
  int fec_count_;
  int complete_packets_;
  int revived_packets_;
  bool accept_packet_;
  bool accept_public_header_;

  scoped_ptr<QuicPacketHeader> header_;
  scoped_ptr<QuicPacketPublicHeader> public_header_;
  scoped_ptr<QuicPublicResetPacket> public_reset_packet_;
  scoped_ptr<QuicVersionNegotiationPacket> version_negotiation_packet_;
  vector<QuicStreamFrame*> stream_frames_;
  vector<QuicAckFrame*> ack_frames_;
  vector<QuicStopWaitingFrame*> stop_waiting_frames_;
  vector<QuicPingFrame*> ping_frames_;
  vector<QuicFecData*> fec_data_;
  string fec_protected_payload_;
  QuicRstStreamFrame rst_stream_frame_;
  QuicConnectionCloseFrame connection_close_frame_;
  QuicGoAwayFrame goaway_frame_;
  QuicWindowUpdateFrame window_update_frame_;
  QuicBlockedFrame blocked_frame_;
  vector<string*> stream_data_;
  vector<string*> fec_data_redundancy_;
};

class QuicFramerTest : public ::testing::TestWithParam<QuicVersion> {
 public:
  QuicFramerTest()
      : encrypter_(new test::TestEncrypter()),
        decrypter_(new test::TestDecrypter()),
        start_(QuicTime::Zero().Add(QuicTime::Delta::FromMicroseconds(0x10))),
        framer_(QuicSupportedVersions(), start_, Perspective::IS_SERVER) {
    version_ = GetParam();
    framer_.set_version(version_);
    framer_.SetDecrypter(ENCRYPTION_NONE, decrypter_);
    framer_.SetEncrypter(ENCRYPTION_NONE, encrypter_);
    framer_.set_visitor(&visitor_);
    framer_.set_received_entropy_calculator(&entropy_calculator_);
  }

  // Helper function to get unsigned char representation of digit in the
  // units place of the current QUIC version number.
  unsigned char GetQuicVersionDigitOnes() {
    return static_cast<unsigned char> ('0' + version_%10);
  }

  // Helper function to get unsigned char representation of digit in the
  // tens place of the current QUIC version number.
  unsigned char GetQuicVersionDigitTens() {
    return static_cast<unsigned char> ('0' + (version_/10)%10);
  }

  bool CheckEncryption(QuicPacketSequenceNumber sequence_number,
                       QuicPacket* packet) {
    if (sequence_number != encrypter_->sequence_number_) {
      LOG(ERROR) << "Encrypted incorrect packet sequence number.  expected "
                 << sequence_number << " actual: "
                 << encrypter_->sequence_number_;
      return false;
    }
    if (packet->AssociatedData() != encrypter_->associated_data_) {
      LOG(ERROR) << "Encrypted incorrect associated data.  expected "
                 << packet->AssociatedData() << " actual: "
                 << encrypter_->associated_data_;
      return false;
    }
    if (packet->Plaintext() != encrypter_->plaintext_) {
      LOG(ERROR) << "Encrypted incorrect plaintext data.  expected "
                 << packet->Plaintext() << " actual: "
                 << encrypter_->plaintext_;
      return false;
    }
    return true;
  }

  bool CheckDecryption(const QuicEncryptedPacket& encrypted,
                       bool includes_version) {
    if (visitor_.header_->packet_sequence_number !=
        decrypter_->sequence_number_) {
      LOG(ERROR) << "Decrypted incorrect packet sequence number.  expected "
                 << visitor_.header_->packet_sequence_number << " actual: "
                 << decrypter_->sequence_number_;
      return false;
    }
    if (QuicFramer::GetAssociatedDataFromEncryptedPacket(
            encrypted, PACKET_8BYTE_CONNECTION_ID,
            includes_version, PACKET_6BYTE_SEQUENCE_NUMBER) !=
        decrypter_->associated_data_) {
      LOG(ERROR) << "Decrypted incorrect associated data.  expected "
                 << QuicFramer::GetAssociatedDataFromEncryptedPacket(
                     encrypted, PACKET_8BYTE_CONNECTION_ID,
                     includes_version, PACKET_6BYTE_SEQUENCE_NUMBER)
                 << " actual: " << decrypter_->associated_data_;
      return false;
    }
    StringPiece ciphertext(encrypted.AsStringPiece().substr(
        GetStartOfEncryptedData(PACKET_8BYTE_CONNECTION_ID, includes_version,
                                PACKET_6BYTE_SEQUENCE_NUMBER)));
    if (ciphertext != decrypter_->ciphertext_) {
      LOG(ERROR) << "Decrypted incorrect ciphertext data.  expected "
                 << ciphertext << " actual: "
                 << decrypter_->ciphertext_;
      return false;
    }
    return true;
  }

  char* AsChars(unsigned char* data) {
    return reinterpret_cast<char*>(data);
  }

  void CheckProcessingFails(unsigned char* packet,
                            size_t len,
                            string expected_error,
                            QuicErrorCode error_code) {
    QuicEncryptedPacket encrypted(AsChars(packet), len, false);
    EXPECT_FALSE(framer_.ProcessPacket(encrypted)) << "len: " << len;
    EXPECT_EQ(expected_error, framer_.detailed_error()) << "len: " << len;
    EXPECT_EQ(error_code, framer_.error()) << "len: " << len;
  }

  // Checks if the supplied string matches data in the supplied StreamFrame.
  void CheckStreamFrameData(string str, QuicStreamFrame* frame) {
    EXPECT_EQ(str, frame->data);
  }

  void CheckStreamFrameBoundaries(unsigned char* packet,
                                  size_t stream_id_size,
                                  bool include_version) {
    // Now test framing boundaries.
    for (size_t i = kQuicFrameTypeSize; i < GetMinStreamFrameSize(); ++i) {
      string expected_error;
      if (i < kQuicFrameTypeSize + stream_id_size) {
        expected_error = "Unable to read stream_id.";
      } else if (i < kQuicFrameTypeSize + stream_id_size +
                         kQuicMaxStreamOffsetSize) {
        expected_error = "Unable to read offset.";
      } else {
        expected_error = "Unable to read frame data.";
      }
      CheckProcessingFails(
          packet,
          i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, include_version,
                                  PACKET_6BYTE_SEQUENCE_NUMBER,
                                  NOT_IN_FEC_GROUP),
          expected_error, QUIC_INVALID_STREAM_DATA);
    }
  }

  void CheckCalculatePacketSequenceNumber(
      QuicPacketSequenceNumber expected_sequence_number,
      QuicPacketSequenceNumber last_sequence_number) {
    QuicPacketSequenceNumber wire_sequence_number =
        expected_sequence_number & kMask;
    QuicFramerPeer::SetLastSequenceNumber(&framer_, last_sequence_number);
    EXPECT_EQ(expected_sequence_number,
              QuicFramerPeer::CalculatePacketSequenceNumberFromWire(
                  &framer_, PACKET_6BYTE_SEQUENCE_NUMBER, wire_sequence_number))
        << "last_sequence_number: " << last_sequence_number
        << " wire_sequence_number: " << wire_sequence_number;
  }

  QuicPacket* BuildDataPacket(const QuicPacketHeader& header,
                              const QuicFrames& frames) {
    return BuildUnsizedDataPacket(&framer_, header, frames);
  }

  QuicPacket* BuildDataPacket(const QuicPacketHeader& header,
                              const QuicFrames& frames,
                              size_t packet_size) {
    return BuildUnsizedDataPacket(&framer_, header, frames, packet_size);
  }

  test::TestEncrypter* encrypter_;
  test::TestDecrypter* decrypter_;
  QuicVersion version_;
  QuicTime start_;
  QuicFramer framer_;
  test::TestQuicVisitor visitor_;
  test::TestEntropyCalculator entropy_calculator_;
};

// Run all framer tests with all supported versions of QUIC.
INSTANTIATE_TEST_CASE_P(QuicFramerTests,
                        QuicFramerTest,
                        ::testing::ValuesIn(kSupportedQuicVersions));

TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearEpochStart) {
  // A few quick manual sanity checks
  CheckCalculatePacketSequenceNumber(UINT64_C(1), UINT64_C(0));
  CheckCalculatePacketSequenceNumber(kEpoch + 1, kMask);
  CheckCalculatePacketSequenceNumber(kEpoch, kMask);

  // Cases where the last number was close to the start of the range
  for (uint64 last = 0; last < 10; last++) {
    // Small numbers should not wrap (even if they're out of order).
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(j, last);
    }

    // Large numbers should not wrap either (because we're near 0 already).
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(kEpoch - 1 - j, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearEpochEnd) {
  // Cases where the last number was close to the end of the range
  for (uint64 i = 0; i < 10; i++) {
    QuicPacketSequenceNumber last = kEpoch - i;

    // Small numbers should wrap.
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(kEpoch + j, last);
    }

    // Large numbers should not (even if they're out of order).
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(kEpoch - 1 - j, last);
    }
  }
}

// Next check where we're in a non-zero epoch to verify we handle
// reverse wrapping, too.
TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearPrevEpoch) {
  const uint64 prev_epoch = 1 * kEpoch;
  const uint64 cur_epoch = 2 * kEpoch;
  // Cases where the last number was close to the start of the range
  for (uint64 i = 0; i < 10; i++) {
    uint64 last = cur_epoch + i;
    // Small number should not wrap (even if they're out of order).
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(cur_epoch + j, last);
    }

    // But large numbers should reverse wrap.
    for (uint64 j = 0; j < 10; j++) {
      uint64 num = kEpoch - 1 - j;
      CheckCalculatePacketSequenceNumber(prev_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearNextEpoch) {
  const uint64 cur_epoch = 2 * kEpoch;
  const uint64 next_epoch = 3 * kEpoch;
  // Cases where the last number was close to the end of the range
  for (uint64 i = 0; i < 10; i++) {
    QuicPacketSequenceNumber last = next_epoch - 1 - i;

    // Small numbers should wrap.
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(next_epoch + j, last);
    }

    // but large numbers should not (even if they're out of order).
    for (uint64 j = 0; j < 10; j++) {
      uint64 num = kEpoch - 1 - j;
      CheckCalculatePacketSequenceNumber(cur_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearNextMax) {
  const uint64 max_number = numeric_limits<uint64>::max();
  const uint64 max_epoch = max_number & ~kMask;

  // Cases where the last number was close to the end of the range
  for (uint64 i = 0; i < 10; i++) {
    // Subtract 1, because the expected next sequence number is 1 more than the
    // last sequence number.
    QuicPacketSequenceNumber last = max_number - i - 1;

    // Small numbers should not wrap, because they have nowhere to go.
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(max_epoch + j, last);
    }

    // Large numbers should not wrap either.
    for (uint64 j = 0; j < 10; j++) {
      uint64 num = kEpoch - 1 - j;
      CheckCalculatePacketSequenceNumber(max_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, EmptyPacket) {
  char packet[] = { 0x00 };
  QuicEncryptedPacket encrypted(packet, 0, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
}

TEST_P(QuicFramerTest, LargePacket) {
  unsigned char packet[kMaxPacketSize + 1] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10,
      0x32,
      0x54,
      0x76,
      0x98,
      0xBA,
      0xDC,
      0xFE,
      // packet sequence number
      0xBC,
      0x9A,
      0x78,
      0x56,
      0x34,
      0x12,
      // private flags
      0x00,
  };

  memset(packet + GetPacketHeaderSize(
             PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
             PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP), 0,
         kMaxPacketSize - GetPacketHeaderSize(
             PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
             PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP) + 1);

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  ASSERT_TRUE(visitor_.header_.get());
  // Make sure we've parsed the packet header, so we can send an error.
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  // Make sure the correct error is propagated.
  EXPECT_EQ(QUIC_PACKET_TOO_LARGE, framer_.error());
}

TEST_P(QuicFramerTest, PacketHeader) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries.
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(!kIncludeVersion)) {
      expected_error = "Unable to read ConnectionId.";
    } else if (i < GetPrivateFlagsOffset(!kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(!kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith4ByteConnectionId) {
  QuicFramerPeer::SetLastSerializedConnectionId(
      &framer_, UINT64_C(0xFEDCBA9876543210));

  unsigned char packet[] = {
    // public flags (4 byte connection_id)
    0x38,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries.
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_4BYTE_CONNECTION_ID, !kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(PACKET_4BYTE_CONNECTION_ID,
                                           !kIncludeVersion)) {
      expected_error = "Unable to read ConnectionId.";
    } else if (i < GetPrivateFlagsOffset(PACKET_4BYTE_CONNECTION_ID,
                                         !kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(PACKET_4BYTE_CONNECTION_ID,
                                     !kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeader1ByteConnectionId) {
  QuicFramerPeer::SetLastSerializedConnectionId(
      &framer_, UINT64_C(0xFEDCBA9876543210));

  unsigned char packet[] = {
    // public flags (1 byte connection_id)
    0x34,
    // connection_id
    0x10,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries.
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_1BYTE_CONNECTION_ID, !kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(PACKET_1BYTE_CONNECTION_ID,
                                           !kIncludeVersion)) {
      expected_error = "Unable to read ConnectionId.";
    } else if (i < GetPrivateFlagsOffset(PACKET_1BYTE_CONNECTION_ID,
                                         !kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(PACKET_1BYTE_CONNECTION_ID,
                                     !kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith0ByteConnectionId) {
  QuicFramerPeer::SetLastSerializedConnectionId(
      &framer_, UINT64_C(0xFEDCBA9876543210));

  unsigned char packet[] = {
    // public flags (0 byte connection_id)
    0x30,
    // connection_id
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries.
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_0BYTE_CONNECTION_ID, !kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(PACKET_0BYTE_CONNECTION_ID,
                                           !kIncludeVersion)) {
      expected_error = "Unable to read ConnectionId.";
    } else if (i < GetPrivateFlagsOffset(PACKET_0BYTE_CONNECTION_ID,
                                         !kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(PACKET_0BYTE_CONNECTION_ID,
                                     !kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWithVersionFlag) {
  unsigned char packet[] = {
    // public flags (version)
    0x3D,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', GetQuicVersionDigitTens(), GetQuicVersionDigitOnes(),
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_TRUE(visitor_.header_->public_header.version_flag);
  EXPECT_EQ(GetParam(), visitor_.header_->public_header.versions[0]);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries.
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < kVersionOffset) {
      expected_error = "Unable to read ConnectionId.";
    } else if (i <  GetSequenceNumberOffset(kIncludeVersion)) {
      expected_error = "Unable to read protocol version.";
    } else if (i < GetPrivateFlagsOffset(kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith4ByteSequenceNumber) {
  QuicFramerPeer::SetLastSequenceNumber(&framer_, UINT64_C(0x123456789ABA));

  unsigned char packet[] = {
    // public flags (8 byte connection_id and 4 byte sequence number)
    0x2C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(UINT64_C(0x123456789ABC), visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries.
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                               PACKET_4BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(!kIncludeVersion)) {
      expected_error = "Unable to read ConnectionId.";
    } else if (i < GetPrivateFlagsOffset(!kIncludeVersion,
                                         PACKET_4BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(!kIncludeVersion,
                                     PACKET_4BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith2ByteSequenceNumber) {
  QuicFramerPeer::SetLastSequenceNumber(&framer_, UINT64_C(0x123456789ABA));

  unsigned char packet[] = {
    // public flags (8 byte connection_id and 2 byte sequence number)
    0x1C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(UINT64_C(0x123456789ABC), visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries.
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                               PACKET_2BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(!kIncludeVersion)) {
      expected_error = "Unable to read ConnectionId.";
    } else if (i < GetPrivateFlagsOffset(!kIncludeVersion,
                                         PACKET_2BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(!kIncludeVersion,
                                     PACKET_2BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith1ByteSequenceNumber) {
  QuicFramerPeer::SetLastSequenceNumber(&framer_, UINT64_C(0x123456789ABA));

  unsigned char packet[] = {
    // public flags (8 byte connection_id and 1 byte sequence number)
    0x0C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_MISSING_PAYLOAD, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(UINT64_C(0x123456789ABC), visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries.
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                               PACKET_1BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(!kIncludeVersion)) {
      expected_error = "Unable to read ConnectionId.";
    } else if (i < GetPrivateFlagsOffset(!kIncludeVersion,
                                         PACKET_1BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(!kIncludeVersion,
                                     PACKET_1BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, InvalidPublicFlag) {
  unsigned char packet[] = {
    // public flags: all flags set but the public reset flag and version flag.
    0xFC,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  CheckProcessingFails(packet,
                       arraysize(packet),
                       "Illegal public flags value.",
                       QUIC_INVALID_PACKET_HEADER);

  // Now turn off validation.
  framer_.set_validate_flags(false);
  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
};

TEST_P(QuicFramerTest, InvalidPublicFlagWithMatchingVersions) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id and version flag and an unknown flag)
    0x4D,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', GetQuicVersionDigitTens(), GetQuicVersionDigitOnes(),
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  CheckProcessingFails(packet,
                       arraysize(packet),
                       "Illegal public flags value.",
                       QUIC_INVALID_PACKET_HEADER);
};

TEST_P(QuicFramerTest, LargePublicFlagWithMismatchedVersions) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id, version flag and an unknown flag)
    0x7D,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', '0', '0',
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(0, visitor_.frame_count_);
  EXPECT_EQ(1, visitor_.version_mismatch_);
};

TEST_P(QuicFramerTest, InvalidPrivateFlag) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x10,

    // frame type (padding)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };
  CheckProcessingFails(packet,
                       arraysize(packet),
                       "Illegal private flags value.",
                       QUIC_INVALID_PACKET_HEADER);
};

TEST_P(QuicFramerTest, InvalidFECGroupOffset) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0x01, 0x00, 0x00, 0x00,
    0x00, 0x00,
    // private flags (fec group)
    0x02,
    // first fec protected packet offset
    0x10
  };
  CheckProcessingFails(packet,
                       arraysize(packet),
                       "First fec protected packet offset must be less "
                       "than the sequence number.",
                       QUIC_INVALID_PACKET_HEADER);
};

TEST_P(QuicFramerTest, PaddingFrame) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (padding frame)
    0x00,
    // Ignored data (which in this case is a stream frame)
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // A packet with no frames is not acceptable.
  CheckProcessingFails(
      packet,
      GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                          PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
      "Packet has no frames.", QUIC_MISSING_PAYLOAD);
}

TEST_P(QuicFramerTest, StreamFrame) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x01020304),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(UINT64_C(0xBA98FEDC32107654), visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0]);

  // Now test framing boundaries.
  CheckStreamFrameBoundaries(packet, kQuicMaxStreamIdSize, !kIncludeVersion);
}

TEST_P(QuicFramerTest, StreamFrame3ByteStreamId) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFE,
    // stream id
    0x04, 0x03, 0x02,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(UINT64_C(0x00020304), visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(UINT64_C(0xBA98FEDC32107654), visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0]);

  // Now test framing boundaries.
  const size_t stream_id_size = 3;
  CheckStreamFrameBoundaries(packet, stream_id_size, !kIncludeVersion);
}

TEST_P(QuicFramerTest, StreamFrame2ByteStreamId) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFD,
    // stream id
    0x04, 0x03,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x00000304),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(UINT64_C(0xBA98FEDC32107654), visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0]);

  // Now test framing boundaries.
  const size_t stream_id_size = 2;
  CheckStreamFrameBoundaries(packet, stream_id_size, !kIncludeVersion);
}

TEST_P(QuicFramerTest, StreamFrame1ByteStreamId) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFC,
    // stream id
    0x04,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x00000004),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(UINT64_C(0xBA98FEDC32107654), visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0]);

  // Now test framing boundaries.
  const size_t stream_id_size = 1;
  CheckStreamFrameBoundaries(packet, stream_id_size, !kIncludeVersion);
}

TEST_P(QuicFramerTest, StreamFrameWithVersion) {
  unsigned char packet[] = {
    // public flags (version, 8 byte connection_id)
    0x3D,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', GetQuicVersionDigitTens(), GetQuicVersionDigitOnes(),
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(visitor_.header_->public_header.version_flag);
  EXPECT_EQ(GetParam(), visitor_.header_->public_header.versions[0]);
  EXPECT_TRUE(CheckDecryption(encrypted, kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x01020304),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(UINT64_C(0xBA98FEDC32107654), visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0]);

  // Now test framing boundaries.
  CheckStreamFrameBoundaries(packet, kQuicMaxStreamIdSize, kIncludeVersion);
}

TEST_P(QuicFramerTest, RejectPacket) {
  visitor_.accept_packet_ = false;

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
}

TEST_P(QuicFramerTest, RejectPublicHeader) {
  visitor_.accept_public_header_ = false;

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_header_.get());
  ASSERT_FALSE(visitor_.header_.get());
}

TEST_P(QuicFramerTest, RevivedStreamFrame) {
  unsigned char payload[] = {
    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = true;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  // Do not encrypt the payload because the revived payload is post-encryption.
  EXPECT_TRUE(framer_.ProcessRevivedPacket(&header,
                                           StringPiece(AsChars(payload),
                                                       arraysize(payload))));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_EQ(1, visitor_.revived_packets_);
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.connection_id);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_TRUE(visitor_.header_->fec_flag);
  EXPECT_TRUE(visitor_.header_->entropy_flag);
  EXPECT_EQ(1 << (header.packet_sequence_number % 8),
            visitor_.header_->entropy_hash);
  EXPECT_EQ(UINT64_C(0x123456789ABC), visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(UINT64_C(0x01020304), visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(UINT64_C(0xBA98FEDC32107654), visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0]);
}

TEST_P(QuicFramerTest, StreamFrameInFecGroup) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x12, 0x34,
    // private flags (fec group)
    0x02,
    // first fec protected packet offset
    0x02,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));
  EXPECT_EQ(IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(UINT64_C(0x341256789ABA), visitor_.header_->fec_group);
  const size_t fec_offset =
      GetStartOfFecProtectedData(PACKET_8BYTE_CONNECTION_ID,
                                 !kIncludeVersion,
                                 PACKET_6BYTE_SEQUENCE_NUMBER);
  EXPECT_EQ(
      string(AsChars(packet) + fec_offset, arraysize(packet) - fec_offset),
      visitor_.fec_protected_payload_);

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(UINT64_C(0x01020304), visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(UINT64_C(0xBA98FEDC32107654), visitor_.stream_frames_[0]->offset);
  CheckStreamFrameData("hello world!", visitor_.stream_frames_[0]);
}

TEST_P(QuicFramerTest, AckFrameTwoTimestamp) {
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xA8, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy)
      0x01,

      // frame type (ack frame)
      // (has nacks, not truncated, 6 byte largest observed, 1 byte delta)
      0x6C,
      // entropy hash of all received packets.
      0xBA,
      // largest observed packet sequence number
      0xBF, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // Number of timestamps.
      0x02,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
      // Delta from largest observed.
      0x02,
      // Delta time.
      0x10, 0x32,
      // num missing packets
      0x01,
      // missing packet delta
      0x01,
      // 0 more missing packets in range.
      0x00,
      // Number of revived packets.
      0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(0xBA, frame.entropy_hash);
  EXPECT_EQ(UINT64_C(0x0123456789ABF), frame.largest_observed);
  ASSERT_EQ(1u, frame.missing_packets.size());
  ASSERT_EQ(2u, frame.received_packet_times.size());
  SequenceNumberSet::const_iterator missing_iter =
      frame.missing_packets.begin();
  EXPECT_EQ(UINT64_C(0x0123456789ABE), *missing_iter);

  const size_t kReceivedEntropyOffset = kQuicFrameTypeSize;
  const size_t kLargestObservedOffset = kReceivedEntropyOffset +
      kQuicEntropyHashSize;
  const size_t kMissingDeltaTimeOffset = kLargestObservedOffset +
      PACKET_6BYTE_SEQUENCE_NUMBER;
  const size_t kNumTimestampsOffset = kMissingDeltaTimeOffset +
      kQuicDeltaTimeLargestObservedSize;
  const size_t kTimestampDeltaLargestObserved1 = kNumTimestampsOffset +
      kQuicNumTimestampsSize;
  const size_t kTimestampTimeDeltaLargestObserved1 =
      kTimestampDeltaLargestObserved1 + 1;
  const size_t kTimestampDeltaLargestObserved2 =
      kTimestampTimeDeltaLargestObserved1 + 4;
  const size_t kTimestampTimeDeltaLargestObserved2 =
      kTimestampDeltaLargestObserved2 + 1;
  const size_t kNumMissingPacketOffset =
      kTimestampTimeDeltaLargestObserved2 + 2;
  const size_t kMissingPacketsOffset = kNumMissingPacketOffset +
      kNumberOfNackRangesSize;
  const size_t kMissingPacketsRange = kMissingPacketsOffset +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  const size_t kRevivedPacketsLength = kMissingPacketsRange +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  // Now test framing boundaries.
  const size_t ack_frame_size = kRevivedPacketsLength +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  for (size_t i = kQuicFrameTypeSize; i < ack_frame_size; ++i) {
    string expected_error;
    if (i < kLargestObservedOffset) {
      expected_error = "Unable to read entropy hash for received packets.";
    } else if (i < kMissingDeltaTimeOffset) {
      expected_error = "Unable to read largest observed.";
    } else if (i < kNumTimestampsOffset) {
      expected_error = "Unable to read delta time largest observed.";
    } else if (i < kTimestampDeltaLargestObserved1) {
      expected_error = "Unable to read num received packets.";
    } else if (i < kTimestampTimeDeltaLargestObserved1) {
      expected_error = "Unable to read sequence delta in received packets.";
    } else if (i < kTimestampDeltaLargestObserved2) {
      expected_error = "Unable to read time delta in received packets.";
    } else if (i < kTimestampTimeDeltaLargestObserved2) {
      expected_error = "Unable to read sequence delta in received packets.";
    } else if (i < kNumMissingPacketOffset) {
      expected_error =
          "Unable to read incremental time delta in received packets.";
    } else if (i < kMissingPacketsOffset) {
      expected_error = "Unable to read num missing packet ranges.";
    } else if (i < kMissingPacketsRange) {
      expected_error = "Unable to read missing sequence number delta.";
    } else if (i < kRevivedPacketsLength) {
      expected_error = "Unable to read missing sequence number range.";
    } else {
      expected_error = "Unable to read num revived packets.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_ACK_DATA);
  }
}


TEST_P(QuicFramerTest, AckFrameOneTimestamp) {
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xA8, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy)
      0x01,

      // frame type (ack frame)
      // (has nacks, not truncated, 6 byte largest observed, 1 byte delta)
      0x6C,
      // entropy hash of all received packets.
      0xBA,
      // largest observed packet sequence number
      0xBF, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // Number of timestamps.
      0x01,
      // Delta from largest observed.
      0x01,
      // Delta time.
      0x10, 0x32, 0x54, 0x76,
      // num missing packets
      0x01,
      // missing packet delta
      0x01,
      // 0 more missing packets in range.
      0x00,
      // Number of revived packets.
      0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(0xBA, frame.entropy_hash);
  EXPECT_EQ(UINT64_C(0x0123456789ABF), frame.largest_observed);
  ASSERT_EQ(1u, frame.missing_packets.size());
  ASSERT_EQ(1u, frame.received_packet_times.size());
  SequenceNumberSet::const_iterator missing_iter =
      frame.missing_packets.begin();
  EXPECT_EQ(UINT64_C(0x0123456789ABE), *missing_iter);

  const size_t kReceivedEntropyOffset = kQuicFrameTypeSize;
  const size_t kLargestObservedOffset = kReceivedEntropyOffset +
      kQuicEntropyHashSize;
  const size_t kMissingDeltaTimeOffset = kLargestObservedOffset +
      PACKET_6BYTE_SEQUENCE_NUMBER;
  const size_t kNumTimestampsOffset = kMissingDeltaTimeOffset +
      kQuicDeltaTimeLargestObservedSize;
  const size_t kTimestampDeltaLargestObserved = kNumTimestampsOffset +
      kQuicNumTimestampsSize;
  const size_t kTimestampTimeDeltaLargestObserved =
    kTimestampDeltaLargestObserved + 1;
  const size_t kNumMissingPacketOffset = kTimestampTimeDeltaLargestObserved + 4;
  const size_t kMissingPacketsOffset = kNumMissingPacketOffset +
      kNumberOfNackRangesSize;
  const size_t kMissingPacketsRange = kMissingPacketsOffset +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  const size_t kRevivedPacketsLength = kMissingPacketsRange +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  // Now test framing boundaries.
  const size_t ack_frame_size = kRevivedPacketsLength +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  for (size_t i = kQuicFrameTypeSize; i < ack_frame_size; ++i) {
    string expected_error;
    if (i < kLargestObservedOffset) {
      expected_error = "Unable to read entropy hash for received packets.";
    } else if (i < kMissingDeltaTimeOffset) {
      expected_error = "Unable to read largest observed.";
    } else if (i < kNumTimestampsOffset) {
      expected_error = "Unable to read delta time largest observed.";
    } else if (i < kTimestampDeltaLargestObserved) {
      expected_error = "Unable to read num received packets.";
    } else if (i < kTimestampTimeDeltaLargestObserved) {
      expected_error = "Unable to read sequence delta in received packets.";
    } else if (i < kNumMissingPacketOffset) {
      expected_error = "Unable to read time delta in received packets.";
    } else if (i < kMissingPacketsOffset) {
      expected_error = "Unable to read num missing packet ranges.";
    } else if (i < kMissingPacketsRange) {
      expected_error = "Unable to read missing sequence number delta.";
    } else if (i < kRevivedPacketsLength) {
      expected_error = "Unable to read missing sequence number range.";
    } else {
      expected_error = "Unable to read num revived packets.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_ACK_DATA);
  }
}


TEST_P(QuicFramerTest, AckFrame) {
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xA8, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy)
      0x01,

      // frame type (ack frame)
      // (has nacks, not truncated, 6 byte largest observed, 1 byte delta)
      0x6C,
      // entropy hash of all received packets.
      0xBA,
      // largest observed packet sequence number
      0xBF, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // Number of timestamps.
      0x00,
      // num missing packets
      0x01,
      // missing packet delta
      0x01,
      // 0 more missing packets in range.
      0x00,
      // Number of revived packets.
      0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(0xBA, frame.entropy_hash);
  EXPECT_EQ(UINT64_C(0x0123456789ABF), frame.largest_observed);
  ASSERT_EQ(1u, frame.missing_packets.size());
  SequenceNumberSet::const_iterator missing_iter =
      frame.missing_packets.begin();
  EXPECT_EQ(UINT64_C(0x0123456789ABE), *missing_iter);

  const size_t kReceivedEntropyOffset = kQuicFrameTypeSize;
  const size_t kLargestObservedOffset = kReceivedEntropyOffset +
      kQuicEntropyHashSize;
  const size_t kMissingDeltaTimeOffset = kLargestObservedOffset +
      PACKET_6BYTE_SEQUENCE_NUMBER;
  const size_t kNumTimestampsOffset = kMissingDeltaTimeOffset +
      kQuicDeltaTimeLargestObservedSize;
  const size_t kNumMissingPacketOffset = kNumTimestampsOffset +
      kQuicNumTimestampsSize;
  const size_t kMissingPacketsOffset = kNumMissingPacketOffset +
      kNumberOfNackRangesSize;
  const size_t kMissingPacketsRange = kMissingPacketsOffset +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  const size_t kRevivedPacketsLength = kMissingPacketsRange +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  // Now test framing boundaries.
  const size_t ack_frame_size = kRevivedPacketsLength +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  for (size_t i = kQuicFrameTypeSize; i < ack_frame_size; ++i) {
    string expected_error;
    if (i < kLargestObservedOffset) {
      expected_error = "Unable to read entropy hash for received packets.";
    } else if (i < kMissingDeltaTimeOffset) {
      expected_error = "Unable to read largest observed.";
    } else if (i < kNumTimestampsOffset) {
      expected_error = "Unable to read delta time largest observed.";
    } else if (i < kNumMissingPacketOffset) {
      expected_error = "Unable to read num received packets.";
    } else if (i < kMissingPacketsOffset) {
      expected_error = "Unable to read num missing packet ranges.";
    } else if (i < kMissingPacketsRange) {
      expected_error = "Unable to read missing sequence number delta.";
    } else if (i < kRevivedPacketsLength) {
      expected_error = "Unable to read missing sequence number range.";
    } else {
      expected_error = "Unable to read num revived packets.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_ACK_DATA);
  }
}

TEST_P(QuicFramerTest, AckFrameRevivedPackets) {
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xA8, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy)
      0x01,

      // frame type (ack frame)
      // (has nacks, not truncated, 6 byte largest observed, 1 byte delta)
      0x6C,
      // entropy hash of all received packets.
      0xBA,
      // largest observed packet sequence number
      0xBF, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // num received packets.
      0x00,
      // num missing packets
      0x01,
      // missing packet delta
      0x01,
      // 0 more missing packets in range.
      0x00,
      // Number of revived packets.
      0x01,
      // Revived packet sequence number.
      0xBE, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // Number of revived packets.
      0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(0xBA, frame.entropy_hash);
  EXPECT_EQ(UINT64_C(0x0123456789ABF), frame.largest_observed);
  ASSERT_EQ(1u, frame.missing_packets.size());
  SequenceNumberSet::const_iterator missing_iter =
      frame.missing_packets.begin();
  EXPECT_EQ(UINT64_C(0x0123456789ABE), *missing_iter);

  const size_t kReceivedEntropyOffset = kQuicFrameTypeSize;
  const size_t kLargestObservedOffset = kReceivedEntropyOffset +
      kQuicEntropyHashSize;
  const size_t kMissingDeltaTimeOffset = kLargestObservedOffset +
      PACKET_6BYTE_SEQUENCE_NUMBER;
  const size_t kNumTimestampsOffset = kMissingDeltaTimeOffset +
      kQuicDeltaTimeLargestObservedSize;
  const size_t kNumMissingPacketOffset = kNumTimestampsOffset +
      kQuicNumTimestampsSize;
  const size_t kMissingPacketsOffset = kNumMissingPacketOffset +
      kNumberOfNackRangesSize;
  const size_t kMissingPacketsRange = kMissingPacketsOffset +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  const size_t kRevivedPacketsLength = kMissingPacketsRange +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  const size_t kRevivedPacketSequenceNumberLength = kRevivedPacketsLength +
      PACKET_1BYTE_SEQUENCE_NUMBER;
  // Now test framing boundaries.
  const size_t ack_frame_size = kRevivedPacketSequenceNumberLength +
      PACKET_6BYTE_SEQUENCE_NUMBER;
  for (size_t i = kQuicFrameTypeSize; i < ack_frame_size; ++i) {
    string expected_error;
    if (i < kReceivedEntropyOffset) {
      expected_error = "Unable to read least unacked delta.";
    } else if (i < kLargestObservedOffset) {
      expected_error = "Unable to read entropy hash for received packets.";
    } else if (i < kMissingDeltaTimeOffset) {
      expected_error = "Unable to read largest observed.";
    } else if (i < kNumTimestampsOffset) {
      expected_error = "Unable to read delta time largest observed.";
    } else if (i < kNumMissingPacketOffset) {
      expected_error = "Unable to read num received packets.";
    } else if (i < kMissingPacketsOffset) {
      expected_error = "Unable to read num missing packet ranges.";
    } else if (i < kMissingPacketsRange) {
      expected_error = "Unable to read missing sequence number delta.";
    } else if (i < kRevivedPacketsLength) {
      expected_error = "Unable to read missing sequence number range.";
    } else if (i < kRevivedPacketSequenceNumberLength) {
      expected_error = "Unable to read num revived packets.";
    } else {
      expected_error = "Unable to read revived packet.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_ACK_DATA);
  }
}

TEST_P(QuicFramerTest, AckFrameNoNacks) {
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xA8, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy)
      0x01,

      // frame type (ack frame)
      // (no nacks, not truncated, 6 byte largest observed, 1 byte delta)
      0x4C,
      // entropy hash of all received packets.
      0xBA,
      // largest observed packet sequence number
      0xBF, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // Number of received packets.
      0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame* frame = visitor_.ack_frames_[0];
  EXPECT_EQ(0xBA, frame->entropy_hash);
  EXPECT_EQ(UINT64_C(0x0123456789ABF), frame->largest_observed);
  ASSERT_EQ(0u, frame->missing_packets.size());

  // Verify that the packet re-serializes identically.
  QuicFrames frames;
  frames.push_back(QuicFrame(frame));
  scoped_ptr<QuicPacket> data(BuildDataPacket(*visitor_.header_, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, AckFrame500Nacks) {
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xA8, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy)
      0x01,

      // frame type (ack frame)
      // (has nacks, not truncated, 6 byte largest observed, 1 byte delta)
      0x6C,
      // entropy hash of all received packets.
      0xBA,
      // largest observed packet sequence number
      0xBF, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // No received packets.
      0x00,
      // num missing packet ranges
      0x02,
      // missing packet delta
      0x01,
      // 243 more missing packets in range.
      // The ranges are listed in this order so the re-constructed packet
      // matches.
      0xF3,
      // No gap between ranges
      0x00,
      // 255 more missing packets in range.
      0xFF,
      // No revived packets.
      0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame* frame = visitor_.ack_frames_[0];
  EXPECT_EQ(0xBA, frame->entropy_hash);
  EXPECT_EQ(UINT64_C(0x0123456789ABF), frame->largest_observed);
  EXPECT_EQ(0u, frame->revived_packets.size());
  ASSERT_EQ(500u, frame->missing_packets.size());
  SequenceNumberSet::const_iterator first_missing_iter =
      frame->missing_packets.begin();
  EXPECT_EQ(UINT64_C(0x0123456789ABE) - 499, *first_missing_iter);
  SequenceNumberSet::const_reverse_iterator last_missing_iter =
      frame->missing_packets.rbegin();
  EXPECT_EQ(UINT64_C(0x0123456789ABE), *last_missing_iter);

  // Verify that the packet re-serializes identically.
  QuicFrames frames;
  frames.push_back(QuicFrame(frame));
  scoped_ptr<QuicPacket> data(BuildDataPacket(*visitor_.header_, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, StopWaitingFrame) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xA8, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy)
    0x01,

    // frame type (ack frame)
    // (has nacks, not truncated, 6 byte largest observed, 1 byte delta)
    0x06,
    // entropy hash of sent packets till least awaiting - 1.
    0xAB,
    // least packet sequence number awaiting an ack, delta from sequence number.
    0x08, 0x00, 0x00, 0x00,
    0x00, 0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.stop_waiting_frames_.size());
  const QuicStopWaitingFrame& frame = *visitor_.stop_waiting_frames_[0];
  EXPECT_EQ(0xAB, frame.entropy_hash);
  EXPECT_EQ(UINT64_C(0x0123456789AA0), frame.least_unacked);

  const size_t kSentEntropyOffset = kQuicFrameTypeSize;
  const size_t kLeastUnackedOffset = kSentEntropyOffset + kQuicEntropyHashSize;
  const size_t frame_size = 7;
  for (size_t i = kQuicFrameTypeSize; i < frame_size; ++i) {
    string expected_error;
    if (i < kLeastUnackedOffset) {
      expected_error = "Unable to read entropy hash for sent packets.";
    } else {
      expected_error = "Unable to read least unacked delta.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_STOP_WAITING_DATA);
  }
}

TEST_P(QuicFramerTest, RstStreamFrameQuicVersion24) {
  if (version_ > QUIC_VERSION_24) {
    // QUIC_VERSION_25 removes the error_details field from QuicRstStreamFrame.
    return;
  }

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x04, 0x03, 0x02, 0x01,

    // sent byte offset
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08,

    // error code
    0x01, 0x00, 0x00, 0x00,

    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(UINT64_C(0x01020304), visitor_.rst_stream_frame_.stream_id);
  EXPECT_EQ(0x01, visitor_.rst_stream_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.rst_stream_frame_.error_details);
  EXPECT_EQ(UINT64_C(0x0807060504030201),
            visitor_.rst_stream_frame_.byte_offset);

  // Now test framing boundaries.
  for (size_t i = kQuicFrameTypeSize;
       i < QuicFramer::GetMinRstStreamFrameSize(); ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize +
                       kQuicMaxStreamOffsetSize) {
      expected_error = "Unable to read rst stream sent byte offset.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize +
                       kQuicMaxStreamOffsetSize + kQuicErrorCodeSize) {
      expected_error = "Unable to read rst stream error code.";
    } else {
      expected_error = "Unable to read rst stream error details.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_RST_STREAM_DATA);
  }
}

TEST_P(QuicFramerTest, RstStreamFrameQuic) {
  if (version_ <= QUIC_VERSION_24) {
    // QUIC_VERSION_25 removes the error_details field from QuicRstStreamFrame.
    return;
  }

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x04, 0x03, 0x02, 0x01,

    // sent byte offset
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08,

    // error code
    0x01, 0x00, 0x00, 0x00,
  };
  // clang-format on

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(UINT64_C(0x01020304), visitor_.rst_stream_frame_.stream_id);
  EXPECT_EQ(0x01, visitor_.rst_stream_frame_.error_code);
  EXPECT_EQ(UINT64_C(0x0807060504030201),
            visitor_.rst_stream_frame_.byte_offset);

  // Now test framing boundaries.
  for (size_t i = kQuicFrameTypeSize; i < QuicFramer::GetRstStreamFrameSize();
       ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize +
                       kQuicMaxStreamOffsetSize) {
      expected_error = "Unable to read rst stream sent byte offset.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize +
                       kQuicMaxStreamOffsetSize + kQuicErrorCodeSize) {
      expected_error = "Unable to read rst stream error code.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_RST_STREAM_DATA);
  }
}

TEST_P(QuicFramerTest, ConnectionCloseFrame) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (connection close frame)
    0x02,
    // error code
    0x11, 0x00, 0x00, 0x00,

    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(0x11, visitor_.connection_close_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);

  ASSERT_EQ(0u, visitor_.ack_frames_.size());

  // Now test framing boundaries.
  for (size_t i = kQuicFrameTypeSize;
       i < QuicFramer::GetMinConnectionCloseFrameSize(); ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize + kQuicErrorCodeSize) {
      expected_error = "Unable to read connection close error code.";
    } else {
      expected_error = "Unable to read connection close error details.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_CONNECTION_CLOSE_DATA);
  }
}

TEST_P(QuicFramerTest, GoAwayFrame) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (go away frame)
    0x03,
    // error code
    0x09, 0x00, 0x00, 0x00,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(UINT64_C(0x01020304), visitor_.goaway_frame_.last_good_stream_id);
  EXPECT_EQ(0x9, visitor_.goaway_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.goaway_frame_.reason_phrase);

  const size_t reason_size = arraysize("because I can") - 1;
  // Now test framing boundaries.
  for (size_t i = kQuicFrameTypeSize;
       i < QuicFramer::GetMinGoAwayFrameSize() + reason_size; ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize + kQuicErrorCodeSize) {
      expected_error = "Unable to read go away error code.";
    } else if (i < kQuicFrameTypeSize + kQuicErrorCodeSize +
               kQuicMaxStreamIdSize) {
      expected_error = "Unable to read last good stream id.";
    } else {
      expected_error = "Unable to read goaway reason.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_GOAWAY_DATA);
  }
}

TEST_P(QuicFramerTest, WindowUpdateFrame) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (window update frame)
    0x04,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // byte offset
    0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);

  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(UINT64_C(0x01020304), visitor_.window_update_frame_.stream_id);
  EXPECT_EQ(UINT64_C(0x0c0b0a0908070605),
            visitor_.window_update_frame_.byte_offset);

  // Now test framing boundaries.
  for (size_t i = kQuicFrameTypeSize;
       i < QuicFramer::GetWindowUpdateFrameSize(); ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize) {
      expected_error = "Unable to read stream_id.";
    } else {
      expected_error = "Unable to read window byte_offset.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_WINDOW_UPDATE_DATA);
  }
}

TEST_P(QuicFramerTest, BlockedFrame) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (blocked frame)
    0x05,
    // stream id
    0x04, 0x03, 0x02, 0x01,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);

  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(UINT64_C(0x01020304), visitor_.blocked_frame_.stream_id);

  // Now test framing boundaries.
  for (size_t i = kQuicFrameTypeSize; i < QuicFramer::GetBlockedFrameSize();
       ++i) {
    string expected_error = "Unable to read stream_id.";
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_BLOCKED_DATA);
  }
}

TEST_P(QuicFramerTest, PingFrame) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (ping frame)
    0x07,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(1u, visitor_.ping_frames_.size());

  // No need to check the PING frame boundaries because it has no payload.
}

TEST_P(QuicFramerTest, PublicResetPacket) {
  unsigned char packet[] = {
    // public flags (public reset, 8 byte connection_id)
    0x0E,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (2) + padding
    0x02, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // tag kRSEQ
    'R', 'S', 'E', 'Q',
    // end offset 16
    0x10, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
    // rejected sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12, 0x00, 0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_reset_packet_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.public_reset_packet_->public_header.connection_id);
  EXPECT_TRUE(visitor_.public_reset_packet_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.public_reset_packet_->public_header.version_flag);
  EXPECT_EQ(UINT64_C(0xABCDEF0123456789),
            visitor_.public_reset_packet_->nonce_proof);
  EXPECT_EQ(UINT64_C(0x123456789ABC),
            visitor_.public_reset_packet_->rejected_sequence_number);
  EXPECT_TRUE(
      visitor_.public_reset_packet_->client_address.address().empty());

  // Now test framing boundaries.
  for (size_t i = 0; i < arraysize(packet); ++i) {
    string expected_error;
    DVLOG(1) << "iteration: " << i;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PACKET_HEADER);
    } else if (i < kPublicResetPacketMessageTagOffset) {
      expected_error = "Unable to read ConnectionId.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PACKET_HEADER);
    } else {
      expected_error = "Unable to read reset message.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PUBLIC_RST_PACKET);
    }
  }
}

TEST_P(QuicFramerTest, PublicResetPacketWithTrailingJunk) {
  unsigned char packet[] = {
    // public flags (public reset, 8 byte connection_id)
    0x0E,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (2) + padding
    0x02, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // tag kRSEQ
    'R', 'S', 'E', 'Q',
    // end offset 16
    0x10, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
    // rejected sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12, 0x00, 0x00,
    // trailing junk
    'j', 'u', 'n', 'k',
  };

  string expected_error = "Unable to read reset message.";
  CheckProcessingFails(packet, arraysize(packet), expected_error,
                       QUIC_INVALID_PUBLIC_RST_PACKET);
}

TEST_P(QuicFramerTest, PublicResetPacketWithClientAddress) {
  unsigned char packet[] = {
    // public flags (public reset, 8 byte connection_id)
    0x0E,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (3) + padding
    0x03, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // tag kRSEQ
    'R', 'S', 'E', 'Q',
    // end offset 16
    0x10, 0x00, 0x00, 0x00,
    // tag kCADR
    'C', 'A', 'D', 'R',
    // end offset 24
    0x18, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
    // rejected sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12, 0x00, 0x00,
    // client address: 4.31.198.44:443
    0x02, 0x00,
    0x04, 0x1F, 0xC6, 0x2C,
    0xBB, 0x01,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_reset_packet_.get());
  EXPECT_EQ(UINT64_C(0xFEDCBA9876543210),
            visitor_.public_reset_packet_->public_header.connection_id);
  EXPECT_TRUE(visitor_.public_reset_packet_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.public_reset_packet_->public_header.version_flag);
  EXPECT_EQ(UINT64_C(0xABCDEF0123456789),
            visitor_.public_reset_packet_->nonce_proof);
  EXPECT_EQ(UINT64_C(0x123456789ABC),
            visitor_.public_reset_packet_->rejected_sequence_number);
  EXPECT_EQ("4.31.198.44",
            IPAddressToString(visitor_.public_reset_packet_->
                client_address.address()));
  EXPECT_EQ(443, visitor_.public_reset_packet_->client_address.port());

  // Now test framing boundaries.
  for (size_t i = 0; i < arraysize(packet); ++i) {
    string expected_error;
    DVLOG(1) << "iteration: " << i;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PACKET_HEADER);
    } else if (i < kPublicResetPacketMessageTagOffset) {
      expected_error = "Unable to read ConnectionId.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PACKET_HEADER);
    } else {
      expected_error = "Unable to read reset message.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PUBLIC_RST_PACKET);
    }
  }
}

TEST_P(QuicFramerTest, VersionNegotiationPacket) {
  unsigned char packet[] = {
    // public flags (version, 8 byte connection_id)
    0x3D,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', GetQuicVersionDigitTens(), GetQuicVersionDigitOnes(),
    'Q', '2', '.', '0',
  };

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.version_negotiation_packet_.get());
  EXPECT_EQ(2u, visitor_.version_negotiation_packet_->versions.size());
  EXPECT_EQ(GetParam(), visitor_.version_negotiation_packet_->versions[0]);

  for (size_t i = 0; i <= kPublicFlagsSize + PACKET_8BYTE_CONNECTION_ID; ++i) {
    string expected_error;
    QuicErrorCode error_code = QUIC_INVALID_PACKET_HEADER;
    if (i < kConnectionIdOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < kVersionOffset) {
      expected_error = "Unable to read ConnectionId.";
    } else {
      expected_error = "Unable to read supported version in negotiation.";
      error_code = QUIC_INVALID_VERSION_NEGOTIATION_PACKET;
    }
    CheckProcessingFails(packet, i, expected_error, error_code);
  }
}

TEST_P(QuicFramerTest, FecPacket) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (fec group & FEC)
    0x06,
    // first fec protected packet offset
    0x01,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  ASSERT_EQ(1, visitor_.fec_count_);
  const QuicFecData& fec_data = *visitor_.fec_data_[0];
  EXPECT_EQ(UINT64_C(0x0123456789ABB), fec_data.fec_group);
  EXPECT_EQ("abcdefghijklmnop", fec_data.redundancy);
}

TEST_P(QuicFramerTest, BuildPaddingFramePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicPaddingFrame padding_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&padding_frame));

  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  uint64 header_size =
      GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                          PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  memset(packet + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, Build4ByteSequenceNumberPaddingFramePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.public_header.sequence_number_length = PACKET_4BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicPaddingFrame padding_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&padding_frame));

  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte connection_id and 4 byte sequence number)
    0x2C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    // private flags
    0x00,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  uint64 header_size =
      GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                          PACKET_4BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  memset(packet + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, Build2ByteSequenceNumberPaddingFramePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.public_header.sequence_number_length = PACKET_2BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicPaddingFrame padding_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&padding_frame));

  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte connection_id and 2 byte sequence number)
    0x1C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A,
    // private flags
    0x00,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  uint64 header_size =
      GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                          PACKET_2BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  memset(packet + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, Build1ByteSequenceNumberPaddingFramePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicPaddingFrame padding_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&padding_frame));

  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte connection_id and 1 byte sequence number)
    0x0C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC,
    // private flags
    0x00,

    // frame type (padding frame)
    0x00,
    0x00, 0x00, 0x00, 0x00
  };

  uint64 header_size =
      GetPacketHeaderSize(PACKET_8BYTE_CONNECTION_ID, !kIncludeVersion,
                          PACKET_1BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  memset(packet + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x77123456789ABC);
  header.fec_group = 0;

  QuicStreamFrame stream_frame(0x01020304, true, UINT64_C(0xBA98FEDC32107654),
                               StringPiece("hello world!"));

  QuicFrames frames;
  frames.push_back(QuicFrame(&stream_frame));

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy)
    0x01,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacketInFecGroup) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x77123456789ABC);
  header.is_in_fec_group = IN_FEC_GROUP;
  header.fec_group = UINT64_C(0x77123456789ABC);

  QuicStreamFrame stream_frame(0x01020304, true, UINT64_C(0xBA98FEDC32107654),
                               StringPiece("hello world!"));

  QuicFrames frames;
  frames.push_back(QuicFrame(&stream_frame));
  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy, is_in_fec_group)
      0x03,
      // FEC group
      0x00,
      // frame type (stream frame with fin and data length field)
      0xFF,
      // stream id
      0x04, 0x03, 0x02, 0x01,
      // offset
      0x54, 0x76, 0x10, 0x32, 0xDC, 0xFE, 0x98, 0xBA,
      // data length (since packet is in an FEC group)
      0x0C, 0x00,
      // data
      'h',  'e',  'l',  'l',  'o',  ' ',  'w',  'o',  'r', 'l', 'd', '!',
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacketWithVersionFlag) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = true;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x77123456789ABC);
  header.fec_group = 0;

  QuicStreamFrame stream_frame(0x01020304, true, UINT64_C(0xBA98FEDC32107654),
                               StringPiece("hello world!"));

  QuicFrames frames;
  frames.push_back(QuicFrame(&stream_frame));

  unsigned char packet[] = {
      // public flags (version, 8 byte connection_id)
      0x3D,
      // connection_id
      0x10,
      0x32,
      0x54,
      0x76,
      0x98,
      0xBA,
      0xDC,
      0xFE,
      // version tag
      'Q',
      '0',
      GetQuicVersionDigitTens(),
      GetQuicVersionDigitOnes(),
      // packet sequence number
      0xBC,
      0x9A,
      0x78,
      0x56,
      0x34,
      0x12,
      // private flags (entropy)
      0x01,

      // frame type (stream frame with fin and no length)
      0xDF,
      // stream id
      0x04,
      0x03,
      0x02,
      0x01,
      // offset
      0x54,
      0x76,
      0x10,
      0x32,
      0xDC,
      0xFE,
      0x98,
      0xBA,
      // data
      'h',
      'e',
      'l',
      'l',
      'o',
      ' ',
      'w',
      'o',
      'r',
      'l',
      'd',
      '!',
  };

  QuicFramerPeer::SetPerspective(&framer_, Perspective::IS_CLIENT);
  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildVersionNegotiationPacket) {
  QuicPacketPublicHeader header;
  header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.reset_flag = false;
  header.version_flag = true;

  unsigned char packet[] = {
      // public flags (version, 8 byte connection_id)
      0x0D,
      // connection_id
      0x10,
      0x32,
      0x54,
      0x76,
      0x98,
      0xBA,
      0xDC,
      0xFE,
      // version tag
      'Q',
      '0',
      GetQuicVersionDigitTens(),
      GetQuicVersionDigitOnes(),
  };

  QuicVersionVector versions;
  versions.push_back(GetParam());
  scoped_ptr<QuicEncryptedPacket> data(
      framer_.BuildVersionNegotiationPacket(header, versions));

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, BuildAckFramePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x770123456789AA8);
  header.fec_group = 0;

  QuicAckFrame ack_frame;
  ack_frame.entropy_hash = 0x43;
  ack_frame.largest_observed = UINT64_C(0x770123456789ABF);
  ack_frame.delta_time_largest_observed = QuicTime::Delta::Zero();
  ack_frame.missing_packets.insert(UINT64_C(0x770123456789ABE));

  QuicFrames frames;
  frames.push_back(QuicFrame(&ack_frame));

  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xA8, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy)
      0x01,

      // frame type (ack frame)
      // (has nacks, not truncated, 6 byte largest observed, 1 byte delta)
      0x6C,
      // entropy hash of all received packets.
      0x43,
      // largest observed packet sequence number
      0xBF, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // Zero delta time.
      0x00, 0x00,
      // num received packets.
      0x00,
      // num missing packet ranges
      0x01,
      // missing packet delta
      0x01,
      // 0 more missing packets in range.
      0x00,
      // 0 revived packets.
      0x00,
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

// TODO(jri): Add test for tuncated packets in which the original ack frame had
// revived packets. (In both the large and small packet cases below).

TEST_P(QuicFramerTest, BuildTruncatedAckFrameLargePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x770123456789AA8);
  header.fec_group = 0;

  QuicAckFrame ack_frame;
  // This entropy hash is different from what shows up in the packet below,
  // since entropy is recomputed by the framer on ack truncation (by
  // TestEntropyCalculator for this test.)
  ack_frame.entropy_hash = 0x43;
  ack_frame.largest_observed = 2 * 300;
  ack_frame.delta_time_largest_observed = QuicTime::Delta::Zero();
  for (size_t i = 1; i < 2 * 300; i += 2) {
    ack_frame.missing_packets.insert(i);
  }

  QuicFrames frames;
  frames.push_back(QuicFrame(&ack_frame));

  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xA8, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy)
      0x01,

      // frame type (ack frame)
      // (has nacks, is truncated, 2 byte largest observed, 1 byte delta)
      0x74,
      // entropy hash of all received packets, set to 1 by TestEntropyCalculator
      // since ack is truncated.
      0x01,
      // 2-byte largest observed packet sequence number.
      // Expected to be 510 (0x1FE), since only 255 nack ranges can fit.
      0xFE, 0x01,
      // Zero delta time.
      0x00, 0x00,
      // num missing packet ranges (limited to 255 by size of this field).
      0xFF,
      // {missing packet delta, further missing packets in range}
      // 6 nack ranges x 42 + 3 nack ranges
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,

      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,

      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,

      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,

      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00,

      // 0 revived packets.
      0x00,
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildTruncatedAckFrameSmallPacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x770123456789AA8);
  header.fec_group = 0;

  QuicAckFrame ack_frame;
  // This entropy hash is different from what shows up in the packet below,
  // since entropy is recomputed by the framer on ack truncation (by
  // TestEntropyCalculator for this test.)
  ack_frame.entropy_hash = 0x43;
  ack_frame.largest_observed = 2 * 300;
  ack_frame.delta_time_largest_observed = QuicTime::Delta::Zero();
  for (size_t i = 1; i < 2 * 300; i += 2) {
    ack_frame.missing_packets.insert(i);
  }

  QuicFrames frames;
  frames.push_back(QuicFrame(&ack_frame));

  unsigned char packet[] = {
      // public flags (8 byte connection_id)
      0x3C,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xA8, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags (entropy)
      0x01,

      // frame type (ack frame)
      // (has nacks, is truncated, 2 byte largest observed, 1 byte delta)
      0x74,
      // entropy hash of all received packets, set to 1 by TestEntropyCalculator
      // since ack is truncated.
      0x01,
      // 2-byte largest observed packet sequence number.
      // Expected to be 12 (0x0C), since only 6 nack ranges can fit.
      0x0C, 0x00,
      // Zero delta time.
      0x00, 0x00,
      // num missing packet ranges (limited to 6 by packet size of 37).
      0x06,
      // {missing packet delta, further missing packets in range}
      // 6 nack ranges
      0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
      // 0 revived packets.
      0x00,
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames, 37u));
  ASSERT_TRUE(data != nullptr);
  // Expect 1 byte unused since at least 2 bytes are needed to fit more nacks.
  EXPECT_EQ(36u, data->length());
  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildStopWaitingPacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x770123456789AA8);
  header.fec_group = 0;

  QuicStopWaitingFrame stop_waiting_frame;
  stop_waiting_frame.entropy_hash = 0x14;
  stop_waiting_frame.least_unacked = UINT64_C(0x770123456789AA0);

  QuicFrames frames;
  frames.push_back(QuicFrame(&stop_waiting_frame));

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xA8, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy)
    0x01,

    // frame type (stop waiting frame)
    0x06,
    // entropy hash of sent packets till least awaiting - 1.
    0x14,
    // least packet sequence number awaiting an ack, delta from sequence number.
    0x08, 0x00, 0x00, 0x00,
    0x00, 0x00,
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildRstFramePacketQuicVersion24) {
  if (version_ > QUIC_VERSION_24) {
    // QUIC_VERSION_25 removes the error_details field from QuicRstStreamFrame.
    return;
  }

  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicRstStreamFrame rst_frame;
  rst_frame.stream_id = 0x01020304;
  rst_frame.error_code = static_cast<QuicRstStreamErrorCode>(0x05060708);
  rst_frame.error_details = "because I can";
  rst_frame.byte_offset = 0x0807060504030201;

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // sent byte offset
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  QuicFrames frames;
  frames.push_back(QuicFrame(&rst_frame));

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, BuildRstFramePacketQuic) {
  if (version_ <= QUIC_VERSION_24) {
    // QUIC_VERSION_25 removes the error_details field from QuicRstStreamFrame.
    return;
  }

  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicRstStreamFrame rst_frame;
  rst_frame.stream_id = 0x01020304;
  rst_frame.error_code = static_cast<QuicRstStreamErrorCode>(0x05060708);
  rst_frame.byte_offset = 0x0807060504030201;

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (rst stream frame)
    0x01,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // sent byte offset
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08,
    // error code
    0x08, 0x07, 0x06, 0x05,
  };
  // clang-format on

  QuicFrames frames;
  frames.push_back(QuicFrame(&rst_frame));

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, BuildCloseFramePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicConnectionCloseFrame close_frame;
  close_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  close_frame.error_details = "because I can";

  QuicFrames frames;
  frames.push_back(QuicFrame(&close_frame));

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy)
    0x01,

    // frame type (connection close frame)
    0x02,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildGoAwayPacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicGoAwayFrame goaway_frame;
  goaway_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  goaway_frame.last_good_stream_id = 0x01020304;
  goaway_frame.reason_phrase = "because I can";

  QuicFrames frames;
  frames.push_back(QuicFrame(&goaway_frame));

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags(entropy)
    0x01,

    // frame type (go away frame)
    0x03,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildWindowUpdatePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicWindowUpdateFrame window_update_frame;
  window_update_frame.stream_id = 0x01020304;
  window_update_frame.byte_offset = 0x1122334455667788;

  QuicFrames frames;
  frames.push_back(QuicFrame(&window_update_frame));

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags(entropy)
    0x01,

    // frame type (window update frame)
    0x04,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // byte offset
    0x88, 0x77, 0x66, 0x55,
    0x44, 0x33, 0x22, 0x11,
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, BuildBlockedPacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicBlockedFrame blocked_frame;
  blocked_frame.stream_id = 0x01020304;

  QuicFrames frames;
  frames.push_back(QuicFrame(&blocked_frame));

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags(entropy)
    0x01,

    // frame type (blocked frame)
    0x05,
    // stream id
    0x04, 0x03, 0x02, 0x01,
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, BuildPingPacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicPingFrame ping_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&ping_frame));

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags(entropy)
    0x01,

    // frame type (ping frame)
    0x07,
  };

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      arraysize(packet));
}

// Test that the MTU discovery packet is serialized correctly as a PING packet.
TEST_P(QuicFramerTest, BuildMtuDiscoveryPacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicMtuDiscoveryFrame mtu_discovery_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&mtu_discovery_frame));

  // clang-format off
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags(entropy)
    0x01,

    // frame type (ping frame)
    0x07,
  };
  // clang-format on

  scoped_ptr<QuicPacket> data(BuildDataPacket(header, frames));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet", data->data(),
                                      data->length(), AsChars(packet),
                                      arraysize(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacket) {
  QuicPublicResetPacket reset_packet;
  reset_packet.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  reset_packet.public_header.reset_flag = true;
  reset_packet.public_header.version_flag = false;
  reset_packet.rejected_sequence_number = UINT64_C(0x123456789ABC);
  reset_packet.nonce_proof = UINT64_C(0xABCDEF0123456789);

  unsigned char packet[] = {
    // public flags (public reset, 8 byte ConnectionId)
    0x0E,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (2) + padding
    0x02, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // tag kRSEQ
    'R', 'S', 'E', 'Q',
    // end offset 16
    0x10, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
    // rejected sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12, 0x00, 0x00,
  };

  scoped_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacketWithClientAddress) {
  QuicPublicResetPacket reset_packet;
  reset_packet.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  reset_packet.public_header.reset_flag = true;
  reset_packet.public_header.version_flag = false;
  reset_packet.rejected_sequence_number = UINT64_C(0x123456789ABC);
  reset_packet.nonce_proof = UINT64_C(0xABCDEF0123456789);
  reset_packet.client_address = IPEndPoint(Loopback4(), 0x1234);

  unsigned char packet[] = {
    // public flags (public reset, 8 byte ConnectionId)
    0x0E,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // message tag (kPRST)
    'P', 'R', 'S', 'T',
    // num_entries (3) + padding
    0x03, 0x00, 0x00, 0x00,
    // tag kRNON
    'R', 'N', 'O', 'N',
    // end offset 8
    0x08, 0x00, 0x00, 0x00,
    // tag kRSEQ
    'R', 'S', 'E', 'Q',
    // end offset 16
    0x10, 0x00, 0x00, 0x00,
    // tag kCADR
    'C', 'A', 'D', 'R',
    // end offset 24
    0x18, 0x00, 0x00, 0x00,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
    // rejected sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12, 0x00, 0x00,
    // client address
    0x02, 0x00,
    0x7F, 0x00, 0x00, 0x01,
    0x34, 0x12,
  };

  scoped_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildFecPacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = true;
  header.entropy_flag = true;
  header.packet_sequence_number = (UINT64_C(0x123456789ABC));
  header.is_in_fec_group = IN_FEC_GROUP;
  header.fec_group = UINT64_C(0x123456789ABB);

  QuicFecData fec_data;
  fec_data.fec_group = 1;
  fec_data.redundancy = "abcdefghijklmnop";

  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy & fec group & fec packet)
    0x07,
    // first fec protected packet offset
    0x01,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  scoped_ptr<QuicPacket> data(framer_.BuildFecPacket(header, fec_data));
  ASSERT_TRUE(data != nullptr);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, EncryptPacket) {
  QuicPacketSequenceNumber sequence_number = UINT64_C(0x123456789ABC);
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (fec group & fec packet)
    0x06,
    // first fec protected packet offset
    0x01,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  scoped_ptr<QuicPacket> raw(new QuicPacket(
      AsChars(packet), arraysize(packet), false, PACKET_8BYTE_CONNECTION_ID,
      !kIncludeVersion, PACKET_6BYTE_SEQUENCE_NUMBER));
  char buffer[kMaxPacketSize];
  scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPayload(
      ENCRYPTION_NONE, sequence_number, *raw, buffer, kMaxPacketSize));

  ASSERT_TRUE(encrypted.get() != nullptr);
  EXPECT_TRUE(CheckEncryption(sequence_number, raw.get()));
}

TEST_P(QuicFramerTest, EncryptPacketWithVersionFlag) {
  QuicPacketSequenceNumber sequence_number = UINT64_C(0x123456789ABC);
  unsigned char packet[] = {
    // public flags (version, 8 byte connection_id)
    0x3D,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '.', '1', '0',
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (fec group & fec flags)
    0x06,
    // first fec protected packet offset
    0x01,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  scoped_ptr<QuicPacket> raw(new QuicPacket(
      AsChars(packet), arraysize(packet), false, PACKET_8BYTE_CONNECTION_ID,
      kIncludeVersion, PACKET_6BYTE_SEQUENCE_NUMBER));
  char buffer[kMaxPacketSize];
  scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPayload(
      ENCRYPTION_NONE, sequence_number, *raw, buffer, kMaxPacketSize));

  ASSERT_TRUE(encrypted.get() != nullptr);
  EXPECT_TRUE(CheckEncryption(sequence_number, raw.get()));
}

TEST_P(QuicFramerTest, AckTruncationLargePacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  // Create a packet with just the ack.
  QuicAckFrame ack_frame = MakeAckFrameWithNackRanges(300, 0u);
  QuicFrame frame;
  frame.type = ACK_FRAME;
  frame.ack_frame = &ack_frame;
  QuicFrames frames;
  frames.push_back(frame);

  // Build an ack packet with truncation due to limit in number of nack ranges.
  scoped_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxPacketSize];
  scoped_ptr<QuicEncryptedPacket> ack_packet(
      framer_.EncryptPayload(ENCRYPTION_NONE, header.packet_sequence_number,
                             *raw_ack_packet, buffer, kMaxPacketSize));
  // Now make sure we can turn our ack packet back into an ack frame.
  ASSERT_TRUE(framer_.ProcessPacket(*ack_packet));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_TRUE(processed_ack_frame.is_truncated);
  EXPECT_EQ(510u, processed_ack_frame.largest_observed);
  ASSERT_EQ(255u, processed_ack_frame.missing_packets.size());
  SequenceNumberSet::const_iterator missing_iter =
      processed_ack_frame.missing_packets.begin();
  EXPECT_EQ(1u, *missing_iter);
  SequenceNumberSet::const_reverse_iterator last_missing_iter =
      processed_ack_frame.missing_packets.rbegin();
  EXPECT_EQ(509u, *last_missing_iter);
}

TEST_P(QuicFramerTest, AckTruncationSmallPacket) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  // Create a packet with just the ack.
  QuicAckFrame ack_frame = MakeAckFrameWithNackRanges(300, 0u);
  QuicFrame frame;
  frame.type = ACK_FRAME;
  frame.ack_frame = &ack_frame;
  QuicFrames frames;
  frames.push_back(frame);

  // Build an ack packet with truncation due to limit in number of nack ranges.
  scoped_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames, 500));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  char buffer[kMaxPacketSize];
  scoped_ptr<QuicEncryptedPacket> ack_packet(
      framer_.EncryptPayload(ENCRYPTION_NONE, header.packet_sequence_number,
                             *raw_ack_packet, buffer, kMaxPacketSize));
  // Now make sure we can turn our ack packet back into an ack frame.
  ASSERT_TRUE(framer_.ProcessPacket(*ack_packet));
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  QuicAckFrame& processed_ack_frame = *visitor_.ack_frames_[0];
  EXPECT_TRUE(processed_ack_frame.is_truncated);
  EXPECT_EQ(476u, processed_ack_frame.largest_observed);
  ASSERT_EQ(238u, processed_ack_frame.missing_packets.size());
  SequenceNumberSet::const_iterator missing_iter =
      processed_ack_frame.missing_packets.begin();
  EXPECT_EQ(1u, *missing_iter);
  SequenceNumberSet::const_reverse_iterator last_missing_iter =
      processed_ack_frame.missing_packets.rbegin();
  EXPECT_EQ(475u, *last_missing_iter);
}

TEST_P(QuicFramerTest, CleanTruncation) {
  QuicPacketHeader header;
  header.public_header.connection_id = UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicAckFrame ack_frame;
  ack_frame.largest_observed = 201;
  for (uint64 i = 1; i < ack_frame.largest_observed; ++i) {
    ack_frame.missing_packets.insert(i);
  }

  // Create a packet with just the ack.
  QuicFrame frame;
  frame.type = ACK_FRAME;
  frame.ack_frame = &ack_frame;
  QuicFrames frames;
  frames.push_back(frame);

  scoped_ptr<QuicPacket> raw_ack_packet(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);

  char buffer[kMaxPacketSize];
  scoped_ptr<QuicEncryptedPacket> ack_packet(
      framer_.EncryptPayload(ENCRYPTION_NONE, header.packet_sequence_number,
                             *raw_ack_packet, buffer, kMaxPacketSize));

  // Now make sure we can turn our ack packet back into an ack frame.
  ASSERT_TRUE(framer_.ProcessPacket(*ack_packet));

  // Test for clean truncation of the ack by comparing the length of the
  // original packets to the re-serialized packets.
  frames.clear();
  frame.type = ACK_FRAME;
  frame.ack_frame = visitor_.ack_frames_[0];
  frames.push_back(frame);

  size_t original_raw_length = raw_ack_packet->length();
  raw_ack_packet.reset(BuildDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != nullptr);
  EXPECT_EQ(original_raw_length, raw_ack_packet->length());
  ASSERT_TRUE(raw_ack_packet != nullptr);
}

TEST_P(QuicFramerTest, EntropyFlagTest) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (Entropy)
    0x01,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(visitor_.header_->entropy_flag);
  EXPECT_EQ(1 << 4, visitor_.header_->entropy_hash);
  EXPECT_FALSE(visitor_.header_->fec_flag);
};

TEST_P(QuicFramerTest, FecEntropyTest) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (Entropy & fec group & FEC)
    0x07,
    // first fec protected packet offset
    0xFF,

    // frame type (stream frame with fin and no length)
    0xDF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(visitor_.header_->fec_flag);
  EXPECT_TRUE(visitor_.header_->entropy_flag);
  EXPECT_EQ(1 << 4, visitor_.header_->entropy_hash);
};

TEST_P(QuicFramerTest, StopPacketProcessing) {
  unsigned char packet[] = {
    // public flags (8 byte connection_id)
    0x3C,
    // connection_id
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // Entropy
    0x01,

    // frame type (stream frame with fin)
    0xFF,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x40,
    // entropy hash of sent packets till least awaiting - 1.
    0x14,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // entropy hash of all received packets.
    0x43,
    // largest observed packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket());
  EXPECT_CALL(visitor, OnPacketHeader(_));
  EXPECT_CALL(visitor, OnStreamFrame(_)).WillOnce(Return(false));
  EXPECT_CALL(visitor, OnAckFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnPacketComplete());
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_)).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnDecryptedPacket(_));

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
}

static char kTestString[] = "At least 20 characters.";
static QuicStreamId kTestQuicStreamId = 1;
static bool ExpectedStreamFrame(const QuicStreamFrame& frame) {
  return frame.stream_id == kTestQuicStreamId && !frame.fin &&
         frame.offset == 0 && frame.data == kTestString;
  // FIN is hard-coded false in ConstructEncryptedPacket.
  // Offset 0 is hard-coded in ConstructEncryptedPacket.
}

// Verify that the packet returned by ConstructEncryptedPacket() can be properly
// parsed by the framer.
TEST_P(QuicFramerTest, ConstructEncryptedPacket) {
  // Since we are using ConstructEncryptedPacket, we have to set the framer's
  // crypto to be Null.
  framer_.SetDecrypter(ENCRYPTION_NONE, QuicDecrypter::Create(kNULL));
  framer_.SetEncrypter(ENCRYPTION_NONE, QuicEncrypter::Create(kNULL));

  scoped_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      42, false, false, kTestQuicStreamId, kTestString,
      PACKET_8BYTE_CONNECTION_ID, PACKET_6BYTE_SEQUENCE_NUMBER));

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket()).Times(1);
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnPacketHeader(_)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(visitor, OnDecryptedPacket(_)).Times(1);
  EXPECT_CALL(visitor, OnError(_)).Times(0);
  EXPECT_CALL(visitor, OnStreamFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnStreamFrame(Truly(ExpectedStreamFrame))).Times(1);
  EXPECT_CALL(visitor, OnAckFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnPacketComplete()).Times(1);

  EXPECT_TRUE(framer_.ProcessPacket(*packet));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
}

// Verify that the packet returned by ConstructMisFramedEncryptedPacket()
// does cause the framer to return an error.
TEST_P(QuicFramerTest, ConstructMisFramedEncryptedPacket) {
  // Since we are using ConstructEncryptedPacket, we have to set the framer's
  // crypto to be Null.
  framer_.SetDecrypter(ENCRYPTION_NONE, QuicDecrypter::Create(kNULL));
  framer_.SetEncrypter(ENCRYPTION_NONE, QuicEncrypter::Create(kNULL));

  scoped_ptr<QuicEncryptedPacket> packet(ConstructMisFramedEncryptedPacket(
      42, false, false, kTestQuicStreamId, kTestString,
      PACKET_8BYTE_CONNECTION_ID, PACKET_6BYTE_SEQUENCE_NUMBER, nullptr));

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket()).Times(1);
  EXPECT_CALL(visitor, OnUnauthenticatedPublicHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnUnauthenticatedHeader(_))
      .Times(1)
      .WillOnce(Return(true));
  EXPECT_CALL(visitor, OnPacketHeader(_)).Times(0);
  EXPECT_CALL(visitor, OnDecryptedPacket(_)).Times(1);
  EXPECT_CALL(visitor, OnError(_)).Times(1);
  EXPECT_CALL(visitor, OnStreamFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnAckFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnPacketComplete()).Times(0);

  EXPECT_FALSE(framer_.ProcessPacket(*packet));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
}

}  // namespace test
}  // namespace net
