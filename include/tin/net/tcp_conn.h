// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// P2-1 PIMPL: TcpConn's implementation (TcpConnImpl) is hidden behind a
// forward-declared Impl class. Users no longer see NetFD, sys_socket.h,
// or io::IoReadWriter. All methods are explicit forwarding wrappers.
//
// NOTE: socklen_t is replaced with int in the public API to avoid leaking
// platform socket headers. The implementation translates int ? socklen_t
// internally.

#ifndef TIN_NET_TCP_CONN_H_
#define TIN_NET_TCP_CONN_H_

#include <cstdint>
#include <memory>

#include "tin/result.h"

namespace tin::net {

// PIMPL: forward-declared implementation. Defined in tcp_conn_impl.h (internal).
class TcpConnImpl;

class TcpConn {
 public:
  TcpConn() = default;
  ~TcpConn() = default;
  TcpConn(const TcpConn& other) = default;
  TcpConn& operator=(const TcpConn& other) = default;

  // Reads up to nbytes into buf. Returns the number of bytes read (>= 0).
  // On error, the Result's status() carries the error code.
  // If some data was read before an error (e.g. EOF), Ok(n) is returned
  // and the error surfaces on the next call to Read().
  Result<size_t> Read(void* buf, int nbytes);
  Result<size_t> Write(const void* buf, int nbytes);

  // t: absolute deadline in nanoseconds since epoch (0 = no deadline).
  // The deadline applies to both Read and Write operations.
  void SetDeadline(int64_t t);
  // t: absolute deadline in nanoseconds since epoch (0 = no deadline).
  void SetReadDeadline(int64_t t);
  // t: absolute deadline in nanoseconds since epoch (0 = no deadline).
  void SetWriteDeadline(int64_t t);

  Status SetKeepAlive(bool enable, int sec);
  void SetLinger(int sec);
  void SetNoDelay(bool no_delay);
  void SetReadBuffer(int bytes);
  void SetWriteBuffer(int bytes);

  // SockOpt: optval/optlen use int instead of socklen_t to avoid leaking
  // platform headers. The implementation casts internally.
  Status GetSockOpt(int level, int name, void* optval, int* optlen);
  Status SetSockOpt(int level, int name, const void* optval, int optlen);

  Status CloseRead();
  Status CloseWrite();
  void Close();

  int64_t TotalReadBytes() const;

  // Returns true if this TcpConn holds a valid connection.
  bool IsValid() const { return impl_ != nullptr; }

 private:
  friend TcpConn MakeTcpConn(std::unique_ptr<class NetFD> netfd);
  explicit TcpConn(std::shared_ptr<TcpConnImpl> impl)
    : impl_(std::move(impl)) {}

  std::shared_ptr<TcpConnImpl> impl_;
};

}  // namespace tin::net

#endif  // TIN_NET_TCP_CONN_H_
