// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "tin/net/sys_socket.h"
#include "tin/error/error.h"
#include "tin/time/time.h"
#include "tin/runtime/runtime.h"
#include "tin/net/netfd.h"

#include "tin/net/listener.h"

namespace tin {
namespace net {

TCPListenerImpl::TCPListenerImpl(NetFD* netfd, int backlog)
  : netfd_(netfd) {
}

TCPListenerImpl::~TCPListenerImpl() {
  delete netfd_;
}

void TCPListenerImpl::SetDeadline(int64 t) {
  int err = netfd_->SetDeadline(t);
  SetErrorCode(TinTranslateSysError(err));
}

void TCPListenerImpl::Close() {
  int err = netfd_->Close();
  SetErrorCode(TinTranslateSysError(err));
}

TcpConn TCPListenerImpl::Accept() {
  NetFD* newfd = NULL;
  TcpConnImpl* conn = NULL;
  int err = netfd_->Accept(&newfd);
  if (err == 0) {
    conn = new TcpConnImpl(newfd);
  }
  SetErrorCode(TinTranslateSysError(err));
  return MakeTcpConn(conn);
}

}  // namespace net
}  // namespace tin
