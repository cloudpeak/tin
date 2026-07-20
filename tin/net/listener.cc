// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "tin/net/sys_socket.h"
#include "tin/error/error.h"
#include "tin/time/time.h"
#include "tin/runtime/runtime.h"
#include "tin/net/netfd.h"
#include "tin/net/listener_impl.h"  // internal: TcpListenerImpl
#include "tin/net/listener.h"       // public: TcpListener (PIMPL)
#include "tin/net/tcp_conn_impl.h"  // internal: MakeTcpConn

namespace tin::net {

// ---------------------------------------------------------------------------
// TcpListenerImpl ? full implementation (internal).
// ---------------------------------------------------------------------------

TcpListenerImpl::TcpListenerImpl(std::unique_ptr<NetFD> netfd, int backlog)
  : netfd_(std::move(netfd)) {
}

TcpListenerImpl::~TcpListenerImpl() = default;

Status TcpListenerImpl::SetDeadline(int64_t t) {
  int err = netfd_->SetDeadline(t);
  return Status::FromErrno(TinTranslateSysError(err));
}

Status TcpListenerImpl::Close() {
  int err = netfd_->Close();
  return Status::FromErrno(TinTranslateSysError(err));
}

Result<TcpConn> TcpListenerImpl::Accept() {
  NetFD* newfd = nullptr;
  int err = netfd_->Accept(&newfd);
  if (err != 0) {
    delete newfd;
    return Result<TcpConn>::Err(TinTranslateSysError(err));
  }
  return Result<TcpConn>::Ok(MakeTcpConn(std::unique_ptr<NetFD>(newfd)));
}

// ---------------------------------------------------------------------------
// TcpListener ? PIMPL forwarding methods (public API).
// ---------------------------------------------------------------------------

Status TcpListener::SetDeadline(int64_t t) {
  return impl_ ? impl_->SetDeadline(t) : Status::FromErrno(TIN_EBADF);
}

Result<TcpConn> TcpListener::Accept() {
  return impl_ ? impl_->Accept()
               : Result<TcpConn>::Err(TIN_EBADF);
}

Status TcpListener::Close() {
  return impl_ ? impl_->Close() : Status::FromErrno(TIN_EBADF);
}

}  // namespace tin::net
