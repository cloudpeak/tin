// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// P2-1 PIMPL: TCPListener's implementation (TCPListenerImpl) is hidden
// behind a forward-declared Impl class.

#ifndef TIN_NET_LISTENER_H_
#define TIN_NET_LISTENER_H_

#include <cstdint>
#include <memory>

#include "tin/net/tcp_conn.h"

namespace tin::net {

class TCPListenerImpl;

class TCPListener {
 public:
  TCPListener() = default;
  ~TCPListener() = default;
  TCPListener(const TCPListener& other) = default;
  TCPListener& operator=(const TCPListener& other) = default;

  void SetDeadline(int64_t t);
  TcpConn Accept();
  void Close();

  bool IsValid() const { return impl_ != nullptr; }

 private:
  friend TCPListener ListenTcp(const class IPAddress&, uint16_t, int);
  friend TCPListener MakeTcpListener(TCPListenerImpl* impl);
  explicit TCPListener(std::shared_ptr<TCPListenerImpl> impl)
    : impl_(std::move(impl)) {}

  std::shared_ptr<TCPListenerImpl> impl_;
};

}  // namespace tin::net

#endif  // TIN_NET_LISTENER_H_
