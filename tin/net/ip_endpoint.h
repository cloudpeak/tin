// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IP_ENDPOINT_H_
#define NET_BASE_IP_ENDPOINT_H_

#include <string>

#include "tin/net/address_family.h"
#include "tin/net/ip_address.h"
#include "tin/net/sys_addrinfo.h"

struct sockaddr;

namespace tin::net {

// An IpEndpoint represents the address of a transport endpoint:
//  * IP address (either v4 or v6)
//  * Port
class  IpEndpoint {
 public:
  IpEndpoint();
  ~IpEndpoint();
  IpEndpoint(const IpAddress& address, uint16_t port);
  IpEndpoint(const IpEndpoint& endpoint);

  const IpAddress& address() const {
    return address_;
  }
  uint16_t port() const {
    return port_;
  }

  // Returns AddressFamily of the address.
  AddressFamily GetFamily() const;

  // Returns the sockaddr family of the address, AF_INET or AF_INET6.
  int GetSockAddrFamily() const;

  // Convert to a provided sockaddr struct.
  // |address| is the sockaddr to copy into.  Should be at least
  //    sizeof(struct sockaddr_storage) bytes.
  // |address_length| is an input/output parameter.  On input, it is the
  //    size of data in |address| available.  On output, it is the size of
  //    the address that was copied into |address|.
  // Returns true on success, false on failure.
  bool ToSockAddr(struct sockaddr* address, socklen_t* address_length) const
  ABSL_MUST_USE_RESULT;

  // Convert from a sockaddr struct.
  // |address| is the address.
  // |address_length| is the length of |address|.
  // Returns true on success, false on failure.
  bool FromSockAddr(const struct sockaddr* address, socklen_t address_length)
  ABSL_MUST_USE_RESULT;

  // Returns value as a string (e.g. "127.0.0.1:80"). Returns the empty string
  // when |address_| is invalid (the port will be ignored).
  std::string ToString() const;

  // As above, but without port. Returns the empty string when address_ is
  // invalid.
  std::string ToStringWithoutPort() const;

  bool operator<(const IpEndpoint& that) const;
  bool operator==(const IpEndpoint& that) const;

 private:
  IpAddress address_;
  uint16_t port_;
};

}  // namespace tin::net

#endif  // NET_BASE_IP_ENDPOINT_H_
