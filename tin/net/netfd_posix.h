// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_NET_NETFD_POSIX_H_
#define TIN_NET_NETFD_POSIX_H_
#include <string>
#include "tin/net/fd_mutex.h"
#include "tin/net/poll_desc.h"
#include "tin/net/address_list.h"
#include "tin/net/ip_endpoint.h"
#include "tin/net/sockaddr_storage.h"
#include "tin/net/netfd_common.h"

namespace tin::net {

class NetFD : public NetFDCommon {
 public:
  NetFD(uintptr_t sysfd,
        AddressFamily family,
        int sotype,
        const std::string& net);

  virtual ~NetFD();

  int Init();

  int Read(void* buf, int len, int* nread);

  int Write(const void* buf, int len, int* nwritten);

  virtual void Destroy();

  int Shutdown(int how);

  int CloseRead();

  int CloseWrite();

  int Dial(IpEndpoint* local, IpEndpoint* remote, int64_t deadline);

  int Bind(const IpEndpoint& address);

  int Listen(int backlog = 511);

  int Accept(NetFD** newfd);

  int EofError(int n, int err);

  int GetSockOpt(int level, int name, void* optval,
                 socklen_t* optlen);

  int SetSockOpt(int level, int name, const void* optval,
                 socklen_t);

  int SetTCPKeepAlive(bool enable, int sec);

 private:
  int Connect(SockaddrStorage* laddr, SockaddrStorage* raddr, int64_t deadline);
  int AcceptImpl(NetFD** newfd);
};

NetFD* NewFD(AddressFamily family, int sotype, int* error_code = nullptr);

}  // namespace tin::net
#endif  // TIN_NET_NETFD_POSIX_H_
