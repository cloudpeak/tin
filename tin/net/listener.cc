// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "tin/net/sys_socket.h"
#include "tin/error/error.h"
#include "tin/time/time.h"
#include "tin/runtime/runtime.h"
#include "tin/net/netfd.h"
#include "tin/net/listener_impl.h"  // internal: TCPListenerImpl
#include "tin/net/listener.h"       // public: TCPListener (PIMPL)
#include "tin/net/tcp_conn.h"       // public: TcpConn, MakeTcpConn

namespace tin {
namespace net {

// ---------------------------------------------------------------------------
// TCPListenerImpl — full implementation (internal).
// ---------------------------------------------------------------------------

TCPListenerImpl::TCPListenerImpl(NetFD* netfd, int backlog)
  : netfd_(netfd) {
}

TCPListenerImpl::~TCPListenerImpl() {
  delete netfd_;
}

Status TCPListenerImpl::SetDeadline(int64_t t) {
  int err = netfd_->SetDeadline(t);
  return Status::FromErrno(TinTranslateSysError(err));
}

Status TCPListenerImpl::Close() {
  int err = netfd_->Close();
  return Status::FromErrno(TinTranslateSysError(err));
}

Result<TcpConn> TCPListenerImpl::Accept() {
  NetFD* newfd = nullptr;
  int err = netfd_->Accept(&newfd);
  if (err != 0) {
    delete newfd;
    return Result<TcpConn>::Err(TinTranslateSysError(err));
  }
  return Result<TcpConn>::Ok(MakeTcpConn(newfd));
}

// ---------------------------------------------------------------------------
// TCPListener — PIMPL forwarding methods (public API).
// ---------------------------------------------------------------------------

Status TCPListener::SetDeadline(int64_t t) {
  return impl_ ? impl_->SetDeadline(t) : Status::FromErrno(TIN_EBADF);
}

Result<TcpConn> TCPListener::Accept() {
  return impl_ ? impl_->Accept()
               : Result<TcpConn>::Err(TIN_EBADF);
}

Status TCPListener::Close() {
  return impl_ ? impl_->Close() : Status::FromErrno(TIN_EBADF);
}

}  // namespace net
}  // namespace tin
