// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "tin/error/error.h"
#include "tin/time/time.h"
#include "tin/net/ip_endpoint.h"
#include "tin/net/netfd.h"
#include "tin/net/dialer.h"
#include "tin/runtime/runtime.h"

namespace tin {
namespace net {

TcpConn DialTcpInternal(const IPAddress& address, uint16 port, int64 deadline) {
  int err = 0;
  AddressFamily family =
    address.IsIPv4() ? ADDRESS_FAMILY_IPV4 : ADDRESS_FAMILY_IPV6;
  NetFD* netfd = NewFD(family, SOCK_STREAM, &err);
  if (netfd != NULL) {
    IPEndPoint endpoint(address, port);
    if (deadline == -1)
      deadline = kint64max;
    err = netfd->Dial(NULL, &endpoint, kint64max);
    if (err != 0) {
      delete netfd;
      netfd = NULL;
    }
  }
  SetErrorCode(TinTranslateSysError(err));
  return MakeTcpConn(new TcpConnImpl(netfd));
}

TcpConn DialTcpInternal(const base::StringPiece& address, uint16 port,
                        int64 deadline) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address)) {
    SetErrorCode(TIN_EINVAL);
    return TcpConn(NULL);
  }
  return DialTcpInternal(ip_address, port, deadline);
}

TcpConn DialTcp(const IPAddress& address, uint16 port) {
  return DialTcpInternal(address, port, -1);
}

TcpConn DialTcp(const base::StringPiece& address, uint16 port) {
  return DialTcpInternal(address, port, -1);
}

TcpConn DialTcpTimeout(const IPAddress& address, uint16 port, int64 deadline) {
  return DialTcpInternal(address, port, deadline);
}

TcpConn DialTcpTimeout(const base::StringPiece& address, uint16 port,
                       int64 deadline) {
  return DialTcpInternal(address, port, deadline);
}

TCPListener ListenTcp(const IPAddress& address, uint16 port, int backlog) {
  int err = 0;
  AddressFamily family =
    address.IsIPv4() ? ADDRESS_FAMILY_IPV4 : ADDRESS_FAMILY_IPV6;
  NetFD* netfd = NewFD(family, SOCK_STREAM, &err);
  if (netfd != NULL) {
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
      VLOG_IF(1, err != 0) << "Bind failed: " << tin::GetErrorStr();
    }
  }
  if (err == 0) {
    err = netfd->Listen(backlog);
  }
  if (err != 0 && netfd != NULL) {
    delete netfd;
    netfd = NULL;
  }
  TCPListenerImpl* listener = NULL;
  if (netfd != NULL) {
    listener = new TCPListenerImpl(netfd, backlog);
  }
  SetErrorCode(TinTranslateSysError(err));
  return TCPListener(listener);
}

TCPListener ListenTcp(const base::StringPiece& address, uint16 port,
                      int backlog) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address)) {
    SetErrorCode(TIN_EINVAL);
    return TCPListener(NULL);
  }
  return ListenTcp(ip_address, port, backlog);
}

}  // namespace net
}  // namespace tin
