// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once


#include "tin/time/time.h"
#include "tin/net/tcp_conn.h"

namespace tin::net {

class NetFD;

class TCPListenerImpl
  : public std::enable_shared_from_this<TCPListenerImpl> {
 public:
  TCPListenerImpl(NetFD* netfd, int backlog);
  ~TCPListenerImpl();

  TCPListenerImpl(const TCPListenerImpl&) = delete;
  TCPListenerImpl& operator=(const TCPListenerImpl&) = delete;

  void SetDeadline(int64_t t);
  TcpConn Accept();
  void Close();

 private:
  NetFD* netfd_;
};


class TCPListener {
public:
  explicit TCPListener(TCPListenerImpl* conn)
          : impl_(conn) {
  }
  TCPListener(const TCPListener& other) = default;

  TCPListenerImpl*  operator->() {
    return impl_.get();
  }

private:
  std::shared_ptr<TCPListenerImpl> impl_;
};

inline TCPListener MakeTcpListener(TCPListenerImpl* listener) {
  return TCPListener(listener);
}

} // namespace tin::net



