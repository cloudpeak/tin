// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include <absl/log/log.h>
#include <absl/log/check.h>
#include  <absl/strings/string_view.h>

#include "tin/net/sys_socket.h"
#include "tin/error/error.h"
#include "tin/time/time.h"
#include "tin/runtime/runtime.h"
#include "tin/net/netfd.h"
#include "tin/net/tcp_conn_impl.h"  // internal: TcpConnImpl
#include "tin/net/tcp_conn.h"       // public: TcpConn (PIMPL)

namespace tin::net {

// ---------------------------------------------------------------------------
// TcpConnImpl ? full implementation (internal).
// ---------------------------------------------------------------------------

TcpConnImpl::TcpConnImpl(std::unique_ptr<NetFD> netfd)
  : netfd_(std::move(netfd)) ,
    total_read_bytes_(0) {
}

TcpConnImpl::~TcpConnImpl() = default;

Result<size_t> TcpConnImpl::Read(void* buf, int nbytes) {
  LOG_IF(FATAL, nbytes == 0) << "Read on zero buffer.";
  int nread = 0;
  int err = netfd_->Read(buf, nbytes, &nread);
  // If some data was read, return it as success ? the error (if any)
  // will surface on the next call to Read().
  if (nread > 0) {
    total_read_bytes_ += nread;
    return Result<size_t>::Ok(static_cast<size_t>(nread));
  }
  if (err != 0) {
    return Result<size_t>::Err(TinTranslateSysError(err));
  }
  return Result<size_t>::Ok(0);
}

Result<size_t> TcpConnImpl::Write(const void* buf, int nbytes) {
  LOG_IF(FATAL, nbytes == 0) << "Write on zero buffer.";
  int nwritten = 0;
  int err = netfd_->Write(buf, nbytes, &nwritten);
  if (nwritten > 0) {
    return Result<size_t>::Ok(static_cast<size_t>(nwritten));
  }
  if (err != 0) {
    return Result<size_t>::Err(TinTranslateSysError(err));
  }
  return Result<size_t>::Ok(0);
}

void TcpConnImpl::SetDeadline(int64_t t) {
  netfd_->SetDeadline(t);
}

void TcpConnImpl::SetReadDeadline(int64_t t) {
  netfd_->SetReadDeadline(t);
}

void TcpConnImpl::SetWriteDeadline(int64_t t) {
  netfd_->SetWriteDeadline(t);
}

Status TcpConnImpl::SetKeepAlive(bool enable, int sec) {
  int err = netfd_->SetTCPKeepAlive(enable, sec);
  return Status::FromErrno(TinTranslateSysError(err));
}

void TcpConnImpl::SetLinger(int sec) {
#if defined(OS_WIN)
  LINGER  linger = { 0 };
#else
  struct linger linger = { 0 };
#endif
  if (sec >= 0) {
    linger.l_onoff = 1;
    linger.l_linger = sec;
  }
  (void)SetSockOpt(SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
}

void TcpConnImpl::SetNoDelay(bool no_delay) {
  int on = no_delay ? 1 : 0;
  (void)SetSockOpt(IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

void TcpConnImpl::SetReadBuffer(int bytes) {
  (void)SetSockOpt(SOL_SOCKET, SO_RCVBUF, &bytes, sizeof(bytes));
}

void TcpConnImpl::SetWriteBuffer(int bytes) {
  (void)SetSockOpt(SOL_SOCKET, SO_SNDBUF, &bytes, sizeof(bytes));
}

Status TcpConnImpl::GetSockOpt(int level,
                               int name,
                               void* optval,
                               socklen_t* optlen) {
  int err = netfd_->GetSockOpt(level, name, optval, optlen);
  return Status::FromErrno(TinTranslateSysError(err));
}

Status TcpConnImpl::SetSockOpt(int level,
                               int name,
                               const void* optval,
                               socklen_t optlen) {
  int err = netfd_->SetSockOpt(level, name, optval, optlen);
  return Status::FromErrno(TinTranslateSysError(err));
}

Status TcpConnImpl::CloseRead() {
  int err = netfd_->CloseRead();
  return Status::FromErrno(TinTranslateSysError(err));
}

Status TcpConnImpl::CloseWrite() {
  int err = netfd_->CloseWrite();
  return Status::FromErrno(TinTranslateSysError(err));
}

void TcpConnImpl::Close() {
  netfd_->Close();
}

// ---------------------------------------------------------------------------
// TcpConn ? PIMPL forwarding methods (public API).
// ---------------------------------------------------------------------------

Result<size_t> TcpConn::Read(void* buf, int nbytes) {
  return impl_ ? impl_->Read(buf, nbytes)
               : Result<size_t>::Err(TIN_EBADF);
}

Result<size_t> TcpConn::Write(const void* buf, int nbytes) {
  return impl_ ? impl_->Write(buf, nbytes)
               : Result<size_t>::Err(TIN_EBADF);
}

void TcpConn::SetDeadline(int64_t t) {
  if (impl_) impl_->SetDeadline(t);
}

void TcpConn::SetReadDeadline(int64_t t) {
  if (impl_) impl_->SetReadDeadline(t);
}

void TcpConn::SetWriteDeadline(int64_t t) {
  if (impl_) impl_->SetWriteDeadline(t);
}

Status TcpConn::SetKeepAlive(bool enable, int sec) {
  return impl_ ? impl_->SetKeepAlive(enable, sec)
               : Status::FromErrno(TIN_EBADF);
}

void TcpConn::SetLinger(int sec) {
  if (impl_) impl_->SetLinger(sec);
}

void TcpConn::SetNoDelay(bool no_delay) {
  if (impl_) impl_->SetNoDelay(no_delay);
}

void TcpConn::SetReadBuffer(int bytes) {
  if (impl_) impl_->SetReadBuffer(bytes);
}

void TcpConn::SetWriteBuffer(int bytes) {
  if (impl_) impl_->SetWriteBuffer(bytes);
}

Status TcpConn::GetSockOpt(int level, int name, void* optval, int* optlen) {
  if (!impl_) return Status::FromErrno(TIN_EBADF);
  socklen_t slen = static_cast<socklen_t>(*optlen);
  Status s = impl_->GetSockOpt(level, name, optval, &slen);
  *optlen = static_cast<int>(slen);
  return s;
}

Status TcpConn::SetSockOpt(int level, int name, const void* optval, int optlen) {
  if (!impl_) return Status::FromErrno(TIN_EBADF);
  return impl_->SetSockOpt(level, name, optval, static_cast<socklen_t>(optlen));
}

Status TcpConn::CloseRead() {
  return impl_ ? impl_->CloseRead() : Status::FromErrno(TIN_EBADF);
}

Status TcpConn::CloseWrite() {
  return impl_ ? impl_->CloseWrite() : Status::FromErrno(TIN_EBADF);
}

void TcpConn::Close() {
  if (impl_) impl_->Close();
}

int64_t TcpConn::TotalReadBytes() const {
  return impl_ ? impl_->TotalReadBytes() : 0;
}

// Factory: single allocation via std::make_shared.
TcpConn MakeTcpConn(std::unique_ptr<NetFD> netfd) {
  return TcpConn(std::make_shared<TcpConnImpl>(std::move(netfd)));
}

}  // namespace tin::net
