// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_NET_NETFD_WINDOWS_H_
#define TIN_NET_NETFD_WINDOWS_H_
#include <windows.h>
#include <winsock2.h>
#include <string>

#include <absl/strings/string_view.h>
#include "tin/platform/platform_win.h"
#include "tin/net/fd_mutex.h"
#include "tin/net/poll_desc.h"
#include "tin/net/address_list.h"
#include "tin/net/ip_endpoint.h"
#include "tin/communication/chan.h"

#include "tin/net/netfd_common.h"

namespace tin::net {

class NetFD;
struct SockaddrStorage;

struct Operation {
  Operation()
    : sa(nullptr)
    , rsan(0)
    , accept_buf()
    , handle(nullptr)
    , error_no(0)
    , qty(0)
    , flags(0)
    , fd(nullptr)
    , mode(0)
    , err_chan(tin::MakeChan<int>(1)) {
  }

  ~Operation() {
  }

  void InitBuf(void* ptr, int len) {
    buf.buf = static_cast<char*>(ptr);
    buf.len = len;
  }

  int io_type;
  OVERLAPPED overlapped;
  uintptr_t runtime_ctx;
  int32_t mode;
  int32_t error_no;
  DWORD qty;

  NetFD* fd;
  WSABUF buf;
  SockaddrStorage* sa;
  DWORD flags;
  uintptr_t handle;  // listen socket handle.
  std::unique_ptr<sockaddr_storage[]> accept_buf;
  int32_t rsan;
  tin::Chan<int> err_chan;
};

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

  int GetSockOpt(int level, int name, void* optval, socklen_t* optlen);

  int SetSockOpt(int level, int name, const void* optval, socklen_t optlen);

  int SetTCPKeepAlive(bool enable, int sec);

  bool SkipSyncNotification() {
    return skip_sync_notification_;
  }

 private:
  int Connect(SockaddrStorage* laddr, SockaddrStorage* raddr, int64_t deadline);
  int AcceptOne(Operation* op, NetFD** newfd);

 private:
  bool skip_sync_notification_;
  Operation rop_;
  Operation wop_;
};

NetFD* NewFD(AddressFamily family, int sotype, int* error_code = nullptr);

}  // namespace tin::net
#endif  // TIN_NET_NETFD_WINDOWS_H_
