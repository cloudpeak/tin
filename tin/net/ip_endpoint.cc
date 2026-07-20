// Copyright (c) 2012 The Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



#include "base/sys_byteorder.h"

#if defined(OS_WIN)
#include <winsock2.h>
#include <ws2bth.h>
#elif defined(OS_POSIX)
#include <netinet/in.h>
#endif

#include <absl/log/log.h>
#include <absl/log/check.h>

#include "tin/net/ip_address.h"

#if defined(OS_WIN)
#include "tin/net/winsock_util.h"
#endif

#include "tin/net/ip_endpoint.h"

namespace tin::net {

namespace {

// By definition, socklen_t is large enough to hold both sizes.
const socklen_t kSockaddrInSize = sizeof(struct sockaddr_in);
const socklen_t kSockaddrIn6Size = sizeof(struct sockaddr_in6);

// Extracts the address and port portions of a sockaddr.
bool GetIpAddressFromSockAddr(const struct sockaddr* sock_addr,
                              socklen_t sock_addr_len,
                              const uint8_t** address,
                              size_t* address_len,
                              uint16_t* port) {
  if (sock_addr->sa_family == AF_INET) {
    if (sock_addr_len < static_cast<socklen_t>(sizeof(struct sockaddr_in)))
      return false;
    const struct sockaddr_in* addr =
        reinterpret_cast<const struct sockaddr_in*>(sock_addr);
    *address = reinterpret_cast<const uint8_t*>(&addr->sin_addr);
    *address_len = IpAddress::kIPv4AddressSize;
    if (port)
      *port = base::NetToHost16(addr->sin_port);
    return true;
  }

  if (sock_addr->sa_family == AF_INET6) {
    if (sock_addr_len < static_cast<socklen_t>(sizeof(struct sockaddr_in6)))
      return false;
    const struct sockaddr_in6* addr =
        reinterpret_cast<const struct sockaddr_in6*>(sock_addr);
    *address = reinterpret_cast<const uint8_t*>(&addr->sin6_addr);
    *address_len = IpAddress::kIPv6AddressSize;
    if (port)
      *port = base::NetToHost16(addr->sin6_port);
    return true;
  }

#if defined(OS_WIN)
  if (sock_addr->sa_family == AF_BTH) {
    if (sock_addr_len < static_cast<socklen_t>(sizeof(SOCKADDR_BTH)))
      return false;
    const SOCKADDR_BTH* addr = reinterpret_cast<const SOCKADDR_BTH*>(sock_addr);
    *address = reinterpret_cast<const uint8_t*>(&addr->btAddr);
    *address_len = kBluetoothAddressSize;
    if (port)
      *port = static_cast<uint16_t>(addr->port);
    return true;
  }
#endif

  return false;  // Unrecognized |sa_family|.
}

}  // namespace

IpEndpoint::IpEndpoint() : port_(0) {}

IpEndpoint::~IpEndpoint() {}

IpEndpoint::IpEndpoint(const IpAddress& address, uint16_t port)
  : address_(address), port_(port) {}

IpEndpoint::IpEndpoint(const IpEndpoint& endpoint) {
  address_ = endpoint.address_;
  port_ = endpoint.port_;
}

AddressFamily IpEndpoint::GetFamily() const {
  return GetAddressFamily(address_);
}

int IpEndpoint::GetSockAddrFamily() const {
  switch (address_.size()) {
  case IpAddress::kIPv4AddressSize:
    return AF_INET;
  case IpAddress::kIPv6AddressSize:
    return AF_INET6;
  default:
    // NOTREACHED() << "Bad IP address";

    return AF_UNSPEC;
  }
}

bool IpEndpoint::ToSockAddr(struct sockaddr* address,
                            socklen_t* address_length) const {
  DCHECK(address);
  DCHECK(address_length);
  switch (address_.size()) {
  case IpAddress::kIPv4AddressSize: {
    if (*address_length < kSockaddrInSize)
      return false;
    *address_length = kSockaddrInSize;
    struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(address);
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = base::HostToNet16(port_);
    memcpy(&addr->sin_addr, &address_.bytes()[0], // vector_as_array
           IpAddress::kIPv4AddressSize);
    break;
  }
  case IpAddress::kIPv6AddressSize: {
    if (*address_length < kSockaddrIn6Size)
      return false;
    *address_length = kSockaddrIn6Size;
    struct sockaddr_in6* addr6 =
        reinterpret_cast<struct sockaddr_in6*>(address);
    memset(addr6, 0, sizeof(struct sockaddr_in6));
    addr6->sin6_family = AF_INET6;
    addr6->sin6_port = base::HostToNet16(port_);
    memcpy(&addr6->sin6_addr, &address_.bytes()[0], // vector_as_array(&address_.bytes()),
           IpAddress::kIPv6AddressSize);
    break;
  }
  default:
    return false;
  }
  return true;
}

bool IpEndpoint::FromSockAddr(const struct sockaddr* sock_addr,
                              socklen_t sock_addr_len) {
  DCHECK(sock_addr);

  const uint8_t* address;
  size_t address_len;
  uint16_t port;
  if (!GetIpAddressFromSockAddr(sock_addr, sock_addr_len, &address,
                                &address_len, &port)) {
    return false;
  }

  address_ = net::IpAddress(address, address_len);
  port_ = port;
  return true;
}

std::string IpEndpoint::ToString() const {
  return IpAddressToStringWithPort(address_, port_);
}

std::string IpEndpoint::ToStringWithoutPort() const {
  return address_.ToString();
}

bool IpEndpoint::operator<(const IpEndpoint& other) const {
  // Sort IPv4 before IPv6.
  if (address_.size() != other.address_.size()) {
    return address_.size() < other.address_.size();
  }
  if (address_ != other.address_) {
    return address_ < other.address_;
  }
  return port_ < other.port_;
}

bool IpEndpoint::operator==(const IpEndpoint& other) const {
  return address_ == other.address_ && port_ == other.port_;
}

}  // namespace tin::net
