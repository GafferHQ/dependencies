// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_RECORD_RDATA_H_
#define NET_DNS_RECORD_RDATA_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/base/net_util.h"
#include "net/dns/dns_protocol.h"

namespace net {

class DnsRecordParser;

// Parsed represenation of the extra data in a record. Does not include standard
// DNS record data such as TTL, Name, Type and Class.
class NET_EXPORT_PRIVATE RecordRdata {
 public:
  virtual ~RecordRdata() {}

  virtual bool IsEqual(const RecordRdata* other) const = 0;
  virtual uint16 Type() const = 0;

 protected:
  RecordRdata();

  DISALLOW_COPY_AND_ASSIGN(RecordRdata);
};

// SRV record format (http://www.ietf.org/rfc/rfc2782.txt):
// 2 bytes network-order unsigned priority
// 2 bytes network-order unsigned weight
// 2 bytes network-order unsigned port
// target: domain name (on-the-wire representation)
class NET_EXPORT_PRIVATE SrvRecordRdata : public RecordRdata {
 public:
  static const uint16 kType = dns_protocol::kTypeSRV;

  ~SrvRecordRdata() override;
  static scoped_ptr<SrvRecordRdata> Create(const base::StringPiece& data,
                                           const DnsRecordParser& parser);

  bool IsEqual(const RecordRdata* other) const override;
  uint16 Type() const override;

  uint16 priority() const { return priority_; }
  uint16 weight() const { return weight_; }
  uint16 port() const { return port_; }

  const std::string& target() const { return target_; }

 private:
  SrvRecordRdata();

  uint16 priority_;
  uint16 weight_;
  uint16 port_;

  std::string target_;

  DISALLOW_COPY_AND_ASSIGN(SrvRecordRdata);
};

// A Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 4 bytes for IP address.
class NET_EXPORT_PRIVATE ARecordRdata : public RecordRdata {
 public:
  static const uint16 kType = dns_protocol::kTypeA;

  ~ARecordRdata() override;
  static scoped_ptr<ARecordRdata> Create(const base::StringPiece& data,
                                         const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16 Type() const override;

  const IPAddressNumber& address() const { return address_; }

 private:
  ARecordRdata();

  IPAddressNumber address_;

  DISALLOW_COPY_AND_ASSIGN(ARecordRdata);
};

// AAAA Record format (http://www.ietf.org/rfc/rfc1035.txt):
// 16 bytes for IP address.
class NET_EXPORT_PRIVATE AAAARecordRdata : public RecordRdata {
 public:
  static const uint16 kType = dns_protocol::kTypeAAAA;

  ~AAAARecordRdata() override;
  static scoped_ptr<AAAARecordRdata> Create(const base::StringPiece& data,
                                         const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16 Type() const override;

  const IPAddressNumber& address() const { return address_; }

 private:
  AAAARecordRdata();

  IPAddressNumber address_;

  DISALLOW_COPY_AND_ASSIGN(AAAARecordRdata);
};

// CNAME record format (http://www.ietf.org/rfc/rfc1035.txt):
// cname: On the wire representation of domain name.
class NET_EXPORT_PRIVATE CnameRecordRdata : public RecordRdata {
 public:
  static const uint16 kType = dns_protocol::kTypeCNAME;

  ~CnameRecordRdata() override;
  static scoped_ptr<CnameRecordRdata> Create(const base::StringPiece& data,
                                             const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16 Type() const override;

  std::string cname() const { return cname_; }

 private:
  CnameRecordRdata();

  std::string cname_;

  DISALLOW_COPY_AND_ASSIGN(CnameRecordRdata);
};

// PTR record format (http://www.ietf.org/rfc/rfc1035.txt):
// domain: On the wire representation of domain name.
class NET_EXPORT_PRIVATE PtrRecordRdata : public RecordRdata {
 public:
  static const uint16 kType = dns_protocol::kTypePTR;

  ~PtrRecordRdata() override;
  static scoped_ptr<PtrRecordRdata> Create(const base::StringPiece& data,
                                           const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16 Type() const override;

  std::string ptrdomain() const { return ptrdomain_; }

 private:
  PtrRecordRdata();

  std::string ptrdomain_;

  DISALLOW_COPY_AND_ASSIGN(PtrRecordRdata);
};

// TXT record format (http://www.ietf.org/rfc/rfc1035.txt):
// texts: One or more <character-string>s.
// a <character-string> is a length octet followed by as many characters.
class NET_EXPORT_PRIVATE TxtRecordRdata : public RecordRdata {
 public:
  static const uint16 kType = dns_protocol::kTypeTXT;

  ~TxtRecordRdata() override;
  static scoped_ptr<TxtRecordRdata> Create(const base::StringPiece& data,
                                           const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16 Type() const override;

  const std::vector<std::string>& texts() const { return texts_; }

 private:
  TxtRecordRdata();

  std::vector<std::string> texts_;

  DISALLOW_COPY_AND_ASSIGN(TxtRecordRdata);
};

// Only the subset of the NSEC record format required by mDNS is supported.
// Nsec record format is described in http://www.ietf.org/rfc/rfc3845.txt and
// the limited version required for mDNS described in
// http://www.rfc-editor.org/rfc/rfc6762.txt Section 6.1.
class NET_EXPORT_PRIVATE NsecRecordRdata : public RecordRdata {
 public:
  static const uint16 kType = dns_protocol::kTypeNSEC;

  ~NsecRecordRdata() override;
  static scoped_ptr<NsecRecordRdata> Create(const base::StringPiece& data,
                                            const DnsRecordParser& parser);
  bool IsEqual(const RecordRdata* other) const override;
  uint16 Type() const override;

  // Length of the bitmap in bits.
  unsigned bitmap_length() const { return bitmap_.size() * 8; }

  // Returns bit i-th bit in the bitmap, where bits withing a byte are organized
  // most to least significant. If it is set, a record with rrtype i exists for
  // the domain name of this nsec record.
  bool GetBit(unsigned i) const;

 private:
  NsecRecordRdata();

  std::vector<uint8> bitmap_;

  DISALLOW_COPY_AND_ASSIGN(NsecRecordRdata);
};


}  // namespace net

#endif  // NET_DNS_RECORD_RDATA_H_
