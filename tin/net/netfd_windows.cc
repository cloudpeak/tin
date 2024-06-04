// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <winsock2.h>
#include <Mswsock.h>
#include <mstcpip.h>

#include <absl/log/log.h>
#include <absl/log/check.h>
#include <absl/base/call_once.h>
#include "tin/net/net.h"
#include "tin/net/sockaddr_storage.h"
#include "tin/net/ip_address.h"
#include "tin/time/time.h"
#include "tin/error/error.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/greenlet.h"
#include "tin/communication/chan.h"
#include "tin/runtime/net/pollops.h"

#include "tin/net/netfd_windows.h"

namespace tin {
namespace net {

class WinIoServer;

namespace {
WinIoServer* rsrv = NULL;
WinIoServer* wsrv = NULL;

absl::once_flag start_server_once_flag;

SockaddrStorage addr_ipv4_any;
SockaddrStorage addr_ipv6_any;
absl::once_flag addr_any_init_once_flag;

void InitAddrAny() {
  IPEndPoint addr4(IPAddress::IPv4AllZeros(), 0);
  addr4.ToSockAddr(addr_ipv4_any.addr, &addr_ipv4_any.addr_len);
  IPEndPoint addr6(IPAddress::IPv6AllZeros(), 0);
  addr6.ToSockAddr(addr_ipv6_any.addr, &addr_ipv6_any.addr_len);
}

SockaddrStorage* GetAddrAny(int family) {
 absl::call_once(addr_any_init_once_flag, InitAddrAny);
  return family == ADDRESS_FAMILY_IPV4 ? &addr_ipv4_any : &addr_ipv6_any;
}
}  // namespace

struct IoSrvReq {
  explicit IoSrvReq(Operation* o, bool cancel_io_flag = false)
    : op(o)
    , cancel_io(cancel_io_flag) {
  }
  Operation* op;
  bool cancel_io;
};

enum WinIoSyscallTyle {
  kWSARecv = 0,
  kWSASend = 1,
  kWSARecvFrom = 2,
  kWSASendto = 3,
  kAcceptEx = 4,
  kConnectEx = 5
};

int WinSubmitIO(Operation* op) {
  int err = 0;
  ZeroMemory(&op->overlapped, sizeof(OVERLAPPED));
  op->qty = 0;
  op->flags = 0;
  switch (op->io_type) {
  case kWSARecv: {
    err = WSARecv(op->fd->SysFd(), &op->buf, 1, &op->qty, &op->flags,
                  &op->overlapped, NULL);
    break;
  }
  case kWSASend: {
    err = WSASend(op->fd->SysFd(), &op->buf, 1, &op->qty, 0, &op->overlapped,
                  NULL);
    break;
  }
  case kWSARecvFrom: {
    break;
  }
  case kWSASendto: {
    break;
  }
  case kAcceptEx: {
    err = AcceptEx(op->fd->SysFd(),
                   op->handle,
                   op->accept_buf.get(),
                   0,
                   op->rsan,
                   op->rsan,
                   &op->qty,
                   &op->overlapped);
    int abc = WSAGetLastError();
    break;
  }
  case kConnectEx: {
    err = pConnectEx(op->fd->SysFd(), op->sa->addr, op->sa->addr_len, NULL, 0,
                     0, &op->overlapped);
    if (err == FALSE) {
      err = SOCKET_ERROR;
    }
    break;
  }
  default:
    LOG(FATAL) << "invalid syscall type";
  }  // switch

  if (err == SOCKET_ERROR) {
    err = WSAGetLastError();
  } else {
    // succeeded immediately
    err = 0;
  }
  return err;
}

class WinIoServer {
 public:
  WinIoServer()
    : chan_(MakeChan<IoSrvReq>()) {
  }

    // Disable copy (and move) semantics.
  WinIoServer(const WinIoServer&) = delete;
  WinIoServer& operator=(const WinIoServer&) = delete;

  void Start() {
   // runtime::SpawnSimple(
     // base::Bind(&WinIoServer::ProcessRemoteIO, base::Unretained(this)));
    // std::bind(&WinIoServer::ProcessRemoteIO, this));
  }

  int ExecIO(Operation* o, int* n);

 private:
  void ProcessRemoteIO() {
    tin::LockOSThread();

    IoSrvReq r(NULL);
    while (chan_->Pop(&r)) {
      int err = 0;
      if (!r.cancel_io) {
        err = WinSubmitIO(r.op);
      } else {
        if (CancelIo(reinterpret_cast<HANDLE>(r.op->fd->SysFd())) == FALSE) {
          err = GetLastError();
        }
      }
      if (!r.op->err_chan->Push(err))
        break;
    }

    tin::UnlockOSThread();
  }

 private:
  Chan<IoSrvReq> chan_;
};

int WinIoServer::ExecIO(Operation* op, int* n) {
  NetFD* fd = op->fd;
  int err = fd->Pd()->Prepare(op->mode);
  if (err != 0) {
    *n = 0;
    return err;
  }

  if (flag_cancelioex_avaiable) {
    err = WinSubmitIO(op);
  } else {
    // for os before windows xp.
    IoSrvReq req(op);
    if (chan_->Push(req)) {
      if (!op->err_chan->Pop(&err)) {
        // pop failed.
        err = TIN_OBJECT_CLOSED;
      }
    } else {
      // push failed.
      err = TIN_OBJECT_CLOSED;
    }
  }

  switch (err) {
  case 0: {
    // IO completed immediately
    if (op->fd->SkipSyncNotification()) {
      // No completion message will follow, so return immediately.
      *n = op->qty;
      return 0;
    }
    break;
  }
  // Need to get our completion message anyway.
  case ERROR_IO_PENDING: {
    // IO started, and we have to wait for its completion.
    err = 0;
    break;
  }
  default:
    // post async io request failed.
    *n = 0;
    return err;
  }
  // Wait for our request to complete.
  err = fd->Pd()->Wait(op->mode);
  if (err == 0) {
    // All is good. Extract our IO results and return.
    if (op->error_no != 0) {
      // GetQueuedCompletionStatus failed.
      err = op->error_no;
      *n = 0;
      return err;
    }
    *n = op->qty;
    return 0;
  }

  // IO is interrupted by "close" or "timeout"
  int netpoll_err = err;
  switch (netpoll_err) {
  case TIN_ECLOSE_INTR:
  case TIN_ETIMEOUT_INTR:
    break;
  default:
    LOG(FATAL) << "io interrupted.";
  }
  // Cancel our request.
  if (flag_cancelioex_avaiable) {
    // Assuming ERROR_NOT_FOUND is returned, if IO is completed.
    // If the function fails, the return value is zero.
    err = CancelIoEx((HANDLE)fd->SysFd(), &op->overlapped);
    if (err == 0 && WSAGetLastError() != ERROR_NOT_FOUND) {
      // maybe do something else, but panic.
      LOG(FATAL) << "CancelIoEx.";
    }
  } else {
    IoSrvReq req(op, true);
    if (!chan_->Push(req)) {
      err = TIN_OBJECT_CLOSED;
    } else {
      if (!op->err_chan->Pop(&err)) {
        err = TIN_OBJECT_CLOSED;
      }
    }
  }
  // Wait for cancellation to complete.
  fd->Pd()->WaitCanceled(op->mode);
  if (op->error_no != 0) {
    err = op->error_no;
    if (err == ERROR_OPERATION_ABORTED) {
      err = netpoll_err;
    }
    *n = 0;
    return err;
  }
  // We issued a cancellation request. But, it seems, IO operation succeeded
  // before the cancellation request run. We need to treat the IO operation as
  // succeeded (the bytes are actually sent/recv from network).
  *n = op->qty;
  return 0;
}

void StartWinIOServer() {
  rsrv = new WinIoServer;
  wsrv = new WinIoServer;

  if (!flag_cancelioex_avaiable) {
    rsrv->Start();
    wsrv->Start();
  }
}

NetFD* NewFD(AddressFamily family, int sotype, int* error_code) {
  // int sotype = SOCK_STREAM;
  // protocol: If a value of 0 is specified, the caller does not wish to specify
  // a protocol and the service provider will choose the protocol to use.
  uintptr_t sysfd = socket(ConvertAddressFamily(family), sotype, 0);
  if (sysfd == INVALID_SOCKET) {
    if (error_code != NULL)
      *error_code = WSAGetLastError();
    return NULL;
  }
  return new NetFD(sysfd, family, sotype, "unused");
}

NetFD::NetFD(uintptr_t sysfd,
             AddressFamily family,
             int sotype,
             const std::string& net)
  : NetFDCommon(sysfd, family, sotype, net)
  , skip_sync_notification_(false) {
  absl::call_once(start_server_once_flag, StartWinIOServer);
}

NetFD::~NetFD() {
  Close();
}

int NetFD::Init() {
  int err = pd_.Init(sysfd_);
  if (err != 0)
    return err;
  if (hasLoadSetFileCompletionNotificationModes) {
    // We do not use events, so we can skip them always.
    uint8_t flags = FILE_SKIP_SET_EVENT_ON_HANDLE;
    // It's not safe to skip completion notifications for UDP:
    if (global_skip_sync_notificaton && net_ == "tcp") {
      flags |= FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    }
    err = pSetFileCompletionNotificationModes((HANDLE)sysfd_, flags);
    if ((err == 0) &&
        ((flags & FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) != 0)) {
      skip_sync_notification_ = true;
    }
  }

  // Disable SIO_UDP_CONNRESET behavior.
  // http://support.microsoft.com/kb/263823
  if (net_ == "udp" || net_ == "udp4" || net_ == "udp6") {
    DWORD ret = 0;
    DWORD flag = 0;
    DWORD size = sizeof(flag);
    int result = WSAIoctl(sysfd_, SIO_UDP_CONNRESET, &flag, size, NULL, 0, &ret,
                          NULL, 0);
    if (result == SOCKET_ERROR) {
      return WSAGetLastError();
    }
  }

  rop_.mode = 'r';
  wop_.mode = 'w';
  rop_.fd = this;
  wop_.fd = this;

  rop_.runtime_ctx = pd_.DescAsUintptr();
  wop_.runtime_ctx = pd_.DescAsUintptr();

  return 0;
}

int NetFD::EofError(int n, int err) {
  if (n == 0 && err == 0 && sotype_ != SOCK_DGRAM && sotype_ != SOCK_RAW) {
    err = ERROR_BROKEN_PIPE;
  }
  return err;
}

int NetFD::GetSockOpt(int level,
                      int name,
                      void* optval,
                      socklen_t* optlen) {
  // If no error occurs, setsockopt returns zero.
  int err = getsockopt(SysFd(),
                       level,
                       name,
                       static_cast<char*>(optval),
                       optlen);

  return err == SOCKET_ERROR ? WSAGetLastError() : 0;
}

int NetFD::SetSockOpt(int level,
                      int name,
                      const void* optval,
                      socklen_t optlen) {
  // If no error occurs, setsockopt returns zero.
  int err = setsockopt(SysFd(),
                       level,
                       name,
                       static_cast<const char*>(optval),
                       optlen);

  return err == SOCKET_ERROR ? WSAGetLastError() : 0;
}

int NetFD::SetTCPKeepAlive(bool enable, int sec) {
  unsigned delay = sec * 1000;
  struct tcp_keepalive keepalive_vals = {
    enable ? 1u : 0u,  // TCP keep-alive on.
    delay,  // Delay seconds before sending first TCP keep-alive packet.
    delay   // Delay seconds between sending TCP keep-alive packets.
  };
  DWORD bytes_returned = 0xABAB;
  int rv = WSAIoctl(SysFd(), SIO_KEEPALIVE_VALS, &keepalive_vals,
                    sizeof(keepalive_vals), NULL, 0,
                    &bytes_returned, NULL, NULL);
  DCHECK(!rv) << "Could not enable TCP Keep-Alive for socket: " << SysFd()
              << " [error: " << WSAGetLastError() << "].";
  return rv == -1 ? WSAGetLastError() : 0;
}

void NetFD::Destroy() {
  if (sysfd_ == INVALID_SOCKET)
    return;
  pd_.Close();
  closesocket(sysfd_);
  sysfd_ = INVALID_SOCKET;
}

int NetFD::Shutdown(int how) {
  int err = Incref();
  if (err != 0) {
    return err;
  }
  err = (shutdown(sysfd_, how) == SOCKET_ERROR) ? WSAGetLastError() : 0;
  Decref();
  return err;
}

int NetFD::CloseRead() {
  return Shutdown(SD_RECEIVE);
}

int NetFD::CloseWrite() {
  return Shutdown(SD_SEND);
}

int NetFD::Connect(SockaddrStorage* laddr, SockaddrStorage* raddr,
                   int64_t deadline) {
  int err = Init();
  if (err != 0) {
    return err;
  }
  if (deadline != 0)
    SetWriteDeadline(deadline);
  do {
    if (laddr == NULL) {
      SockaddrStorage* any_addr = GetAddrAny(family_);
      if (::bind(sysfd_, any_addr->addr, any_addr->addr_len) != 0) {
        err = WSAGetLastError();
        break;
      }
    }

    Operation* op = &wop_;
    op->sa = raddr;
    op->io_type = kConnectEx;
    int n = 0;
    err = wsrv->ExecIO(op, &n);
  } while (false);

  if (deadline != 0)
    SetWriteDeadline(0);

  return err;
}

int NetFD::Dial(IPEndPoint* local, IPEndPoint* remote, int64_t deadline) {
  int err = 0;
  SockaddrStorage lstorage;
  if (local != NULL) {
    if (local->GetFamily() != family_) {
      return ERROR_INVALID_PARAMETER;
    }
    if (!local->ToSockAddr(lstorage.addr, &lstorage.addr_len)) {
      return ERROR_INVALID_PARAMETER;
    }

    if (::bind(sysfd_, lstorage.addr, lstorage.addr_len) != 0) {
      return WSAGetLastError();
    }
  }

  SockaddrStorage rstorage;
  if (remote != NULL) {
    if (remote->GetFamily() != family_) {
      return ERROR_INVALID_PARAMETER;
    }
    if (!remote->ToSockAddr(rstorage.addr, &rstorage.addr_len)) {
      return ERROR_INVALID_PARAMETER;
    }
    err = Connect(local == NULL ? NULL : &lstorage, &rstorage, deadline);
    if (err != 0) {
      return err;
    }
    is_connected_ = true;
  } else {
    err = Init();
    if (err != 0) {
      return err;
    }
  }
  return 0;
}

/* the magic 511 constant is from nginx */
int NetFD::Bind(const IPEndPoint& address) {
  SockaddrStorage storage;
  if (address.ToSockAddr(storage.addr, &storage.addr_len)) {
    if (::bind(sysfd_, storage.addr, storage.addr_len) == 0) {
      return 0;
    }
    return WSAGetLastError();
  }
  return ERROR_INVALID_PARAMETER;
}

int NetFD::Listen(int backlog /*= 511*/) {
  if (listen(sysfd_, backlog) != SOCKET_ERROR) {
    return 0;
  }
  return WSAGetLastError();
}

// note: me is listen socket.
int NetFD::AcceptOne(Operation* op, NetFD** new_fd) {
  int err = 0;
  std::unique_ptr<NetFD> net_fd(NewFD(family_, sotype_, &err));
  if (!net_fd) {
    return err;
  }

  err = net_fd->Init();
  if (err != 0) {
    return err;
  }

  op->fd = this;
  op->handle = net_fd->SysFd();
  op->io_type = kAcceptEx;
  op->rsan = sizeof(sockaddr_storage);
  int n = 0;
  err = rsrv->ExecIO(op, &n);
  if (err != 0) {
    return err;
  }
  err = ::setsockopt(net_fd->SysFd(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                     reinterpret_cast<const char*>(&sysfd_), sizeof(sysfd_));
  if (err != 0) {
    return err;
  }
  *new_fd = net_fd.release();
  return 0;
}

int NetFD::Accept(NetFD** new_fd) {
  int err = ReadLock();
  if (err != 0)
    return err;

  Operation* op = &rop_;
  if (!op->accept_buf) {
    op->accept_buf.reset(new sockaddr_storage[2]);
  }
  while (true) {
    NetFD* net_fd = NULL;
    err = AcceptOne(op, &net_fd);
    if (err == 0) {
      *new_fd = net_fd;
      break;
    }
    // Sometimes we see WSAECONNRESET and ERROR_NETNAME_DELETED is
    // returned here. These happen if connection reset is received
    // before AcceptEx could complete. These errors relate to new
    // connection, not to AcceptEx, so ignore broken connection and
    // try AcceptEx again for more connections.

    if (err != ERROR_NETNAME_DELETED && err != WSAECONNRESET)
      break;
  }

  ReadUnlock();
  return err;
}

int NetFD::Read(void* buf, int len, int* nread) {
  int err = ReadLock();
  if (err != 0)
    return err;

  Operation* op = &rop_;
  op->InitBuf(buf, len);
  op->io_type = kWSARecv;
  int n = 0;
  err = rsrv->ExecIO(op, &n);
  err = EofError(n, err);
  if (err == 0)
    *nread = n;

  ReadUnlock();
  return err;
}

int NetFD::Write(const void* buf, int len, int* nwritten) {
  int err = WriteLock();
  if (err != 0)
    return err;

  Operation* op = &wop_;
  op->InitBuf(const_cast<void*>(buf), len);
  op->io_type = kWSASend;
  int n = 0;
  err = rsrv->ExecIO(op, &n);
  if (err == 0)
    *nwritten = n;

  WriteUnlock();
  return err;
}

}  // namespace net
}  // namespace tin
