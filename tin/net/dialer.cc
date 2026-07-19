// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/log.h>
#include "tin/error/error.h"
#include "tin/time/time.h"
#include "tin/net/ip_endpoint.h"
#include "tin/net/netfd.h"
#include "tin/net/dialer.h"
#include "tin/runtime/runtime.h"

namespace tin {
namespace net {

TcpConn DialTcpInternal(const IPAddress& address, uint16_t port, int64_t deadline) {
  int err = 0;
  AddressFamily family =
    address.IsIPv4() ? ADDRESS_FAMILY_IPV4 : ADDRESS_FAMILY_IPV6;
  NetFD* netfd = NewFD(family, SOCK_STREAM, &err);
  if (netfd != nullptr) {
    IPEndPoint endpoint(address, port);
    if (deadline == -1)
      deadline = UINT64_MAX;
    err = netfd->Dial(nullptr, &endpoint, UINT64_MAX);
    if (err != 0) {
      delete netfd;
      netfd = nullptr;
    }
  }
  SetErrorCode(TinTranslateSysError(err));
  if (netfd == nullptr) {
    return TcpConn();
  }
  return MakeTcpConn(netfd);
}

TcpConn DialTcpInternal(const absl::string_view& address, uint16_t port,
                        int64_t deadline) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address)) {
    SetErrorCode(TIN_EINVAL);
    return TcpConn();
  }
  return DialTcpInternal(ip_address, port, deadline);
}

TcpConn DialTcp(const IPAddress& address, uint16_t port) {
  return DialTcpInternal(address, port, -1);
}

TcpConn DialTcp(const absl::string_view& address, uint16_t port) {
  return DialTcpInternal(address, port, -1);
}

TcpConn DialTcpTimeout(const IPAddress& address, uint16_t port, int64_t deadline) {
  return DialTcpInternal(address, port, deadline);
}

TcpConn DialTcpTimeout(const absl::string_view& address, uint16_t port,
                       int64_t deadline) {
  return DialTcpInternal(address, port, deadline);
}

TCPListener ListenTcp(const IPAddress& address, uint16_t port, int backlog) {
  int err = 0;
  AddressFamily family =
    address.IsIPv4() ? ADDRESS_FAMILY_IPV4 : ADDRESS_FAMILY_IPV6;
  NetFD* netfd = NewFD(family, SOCK_STREAM, &err);
  if (netfd != nullptr) {
    err = netfd->Init();
    if (err == 0) {
#if defined(OS_POSIX)
      int on = 1;
      err = netfd->SetSockOpt(SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif
    }
    if (err == 0) {
      IPEndPoint endpoint(address, port);
      err = netfd->Bind(endpoint);
      if(err != 0) {
        LOG(INFO) << "Bind failed: " << tin::GetErrorStr();
      }
    }
  }
  if (err == 0) {
    err = netfd->Listen(backlog);
  }
  if (err != 0 && netfd != nullptr) {
    delete netfd;
    netfd = nullptr;
  }
  if (netfd == nullptr) {
    SetErrorCode(TinTranslateSysError(err));
    return TCPListener(nullptr);
  }
  return TCPListener(new TCPListenerImpl(netfd, backlog));
}

TCPListener ListenTcp(const absl::string_view& address, uint16_t port,
                      int backlog) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address)) {
    SetErrorCode(TIN_EINVAL);
    return TCPListener(nullptr);
  }
  return ListenTcp(ip_address, port, backlog);
}

}  // namespace net
}  // namespace tin
