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

namespace tin {
namespace net {

// ---------------------------------------------------------------------------
// TcpConnImpl — full implementation (internal).
// ---------------------------------------------------------------------------

TcpConnImpl::TcpConnImpl(NetFD* netfd)
  : netfd_(netfd) ,
    total_read_bytes_(0) {
}

TcpConnImpl::~TcpConnImpl() {
  delete netfd_;
}

int TcpConnImpl::Read(void* buf, int nbytes) {
  LOG_IF(FATAL, nbytes == 0) << "Read on zero buffer.";
  int nread = 0;
  int err = netfd_->Read(buf, nbytes, &nread);
  tin::SetErrorCode(TinTranslateSysError(err));
  if (nread > 0) {
    total_read_bytes_ += total_read_bytes_;
  }
  return nread;
}

int TcpConnImpl::Write(const void* buf, int nbytes) {
  LOG_IF(FATAL, nbytes == 0) << "Write on zero buffer.";
  int nwritten = 0;
  int err = netfd_->Write(buf, nbytes, &nwritten);
  tin::SetErrorCode(TinTranslateSysError(err));
  return nwritten;
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

bool TcpConnImpl::SetKeepAlive(bool enable, int sec) {
  int err = netfd_->SetTCPKeepAlive(enable, sec);
  err = TinTranslateSysError(err);
  tin::SetErrorCode(err);
  return err == 0;
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

bool TcpConnImpl::GetSockOpt(int level,
                             int name,
                             void* optval,
                             socklen_t* optlen) {
  int err = netfd_->GetSockOpt(level, name, optval, optlen);
  err = TinTranslateSysError(err);
  tin::SetErrorCode(err);
  return err == 0;
}

bool TcpConnImpl::SetSockOpt(int level,
                             int name,
                             const void* optval,
                             socklen_t optlen) {
  int err = netfd_->SetSockOpt(level, name, optval, optlen);
  err = TinTranslateSysError(err);
  tin::SetErrorCode(err);
  return err == 0;
}

void TcpConnImpl::CloseRead() {
  int err = netfd_->CloseRead();
  tin::SetErrorCode(err);
}

void TcpConnImpl::CloseWrite() {
  int err = netfd_->CloseWrite();
  tin::SetErrorCode(err);
}

void TcpConnImpl::Close() {
  netfd_->Close();
}

// ---------------------------------------------------------------------------
// TcpConn — PIMPL forwarding methods (public API).
// ---------------------------------------------------------------------------

int TcpConn::Read(void* buf, int nbytes) {
  return impl_ ? impl_->Read(buf, nbytes) : 0;
}

int TcpConn::Write(const void* buf, int nbytes) {
  return impl_ ? impl_->Write(buf, nbytes) : 0;
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

bool TcpConn::SetKeepAlive(bool enable, int sec) {
  return impl_ ? impl_->SetKeepAlive(enable, sec) : false;
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

bool TcpConn::GetSockOpt(int level, int name, void* optval, int* optlen) {
  if (!impl_) return false;
  socklen_t slen = static_cast<socklen_t>(*optlen);
  bool ok = impl_->GetSockOpt(level, name, optval, &slen);
  *optlen = static_cast<int>(slen);
  return ok;
}

bool TcpConn::SetSockOpt(int level, int name, const void* optval, int optlen) {
  if (!impl_) return false;
  return impl_->SetSockOpt(level, name, optval, static_cast<socklen_t>(optlen));
}

void TcpConn::CloseRead() {
  if (impl_) impl_->CloseRead();
}

void TcpConn::CloseWrite() {
  if (impl_) impl_->CloseWrite();
}

void TcpConn::Close() {
  if (impl_) impl_->Close();
}

int64_t TcpConn::TotalReadBytes() const {
  return impl_ ? impl_->TotalReadBytes() : 0;
}

// Factory: single allocation via std::make_shared.
TcpConn MakeTcpConn(NetFD* netfd) {
  return TcpConn(std::make_shared<TcpConnImpl>(netfd));
}

}  // namespace net
}  // namespace tin
