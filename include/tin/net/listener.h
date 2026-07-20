// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// P2-1 PIMPL: TcpListener's implementation (TcpListenerImpl) is hidden
// behind a forward-declared Impl class.

#ifndef TIN_NET_LISTENER_H_
#define TIN_NET_LISTENER_H_

#include <cstdint>
#include <memory>

#include "tin/net/tcp_conn.h"
#include "tin/result.h"

namespace tin::net {

class TcpListenerImpl;

class TcpListener {
 public:
  TcpListener() = default;
  ~TcpListener() = default;
  TcpListener(const TcpListener& other) = default;
  TcpListener& operator=(const TcpListener& other) = default;

  Status SetDeadline(int64_t t);
  Result<TcpConn> Accept();
  Status Close();

  bool IsValid() const { return impl_ != nullptr; }

 private:
  friend Result<TcpListener> ListenTcp(const class IpAddress&,
                                       uint16_t, int);
  explicit TcpListener(std::shared_ptr<TcpListenerImpl> impl)
    : impl_(std::move(impl)) {}

  std::shared_ptr<TcpListenerImpl> impl_;
};

// Deprecated alias for backward compatibility. Use TcpListener instead.
using TCPListener = TcpListener;

}  // namespace tin::net

#endif  // TIN_NET_LISTENER_H_
