// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_QUERY_H_
#define NET_DNS_DNS_QUERY_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace net {

class IOBufferWithSize;

// Represents on-the-wire DNS query message as an object.
// TODO(szym): add support for the OPT pseudo-RR (EDNS0/DNSSEC).
class NET_EXPORT_PRIVATE DnsQuery {
 public:
  // Constructs a query message from |qname| which *MUST* be in a valid
  // DNS name format, and |qtype|. The qclass is set to IN.
  DnsQuery(uint16 id, const base::StringPiece& qname, uint16 qtype);
  ~DnsQuery();

  // Clones |this| verbatim, with ID field of the header set to |id|.
  DnsQuery* CloneWithNewId(uint16 id) const;

  // DnsQuery field accessors.
  uint16 id() const;
  base::StringPiece qname() const;
  uint16 qtype() const;

  // Returns the Question section of the query.  Used when matching the
  // response.
  base::StringPiece question() const;

  // IOBuffer accessor to be used for writing out the query.
  IOBufferWithSize* io_buffer() const { return io_buffer_.get(); }

  void set_flags(uint16 flags);

 private:
  DnsQuery(const DnsQuery& orig, uint16 id);

  // Size of the DNS name (*NOT* hostname) we are trying to resolve; used
  // to calculate offsets.
  size_t qname_size_;

  // Contains query bytes to be consumed by higher level Write() call.
  scoped_refptr<IOBufferWithSize> io_buffer_;

  DISALLOW_COPY_AND_ASSIGN(DnsQuery);
};

}  // namespace net

#endif  // NET_DNS_DNS_QUERY_H_
