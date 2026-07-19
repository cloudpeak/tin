// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// P2-1 PIMPL: TcpConn's implementation (TcpConnImpl) is hidden behind a
// forward-declared Impl class. Users no longer see NetFD, sys_socket.h,
// or io::IOReadWriter. All methods are explicit forwarding wrappers.
//
// NOTE: socklen_t is replaced with int in the public API to avoid leaking
// platform socket headers. The implementation translates int ↔ socklen_t
// internally.

#ifndef TIN_NET_TCP_CONN_H_
#define TIN_NET_TCP_CONN_H_

#include <cstdint>
#include <memory>

namespace tin {
namespace net {

// PIMPL: forward-declared implementation. Defined in tcp_conn_impl.h (internal).
class TcpConnImpl;

class TcpConn {
 public:
  TcpConn() = default;
  ~TcpConn() = default;
  TcpConn(const TcpConn& other) = default;
  TcpConn& operator=(const TcpConn& other) = default;

  // Explicit forwarding methods (replaces operator->()).
  // note: Read full or partial on success, or read partial on failure.
  // return value: indicate n bytes read. n >= 0.
  // detail error, see tin::GetErrorCode()
  int Read(void* buf, int nbytes);
  int Write(const void* buf, int nbytes);

  void SetDeadline(int64_t t);
  void SetReadDeadline(int64_t t);
  void SetWriteDeadline(int64_t t);

  bool SetKeepAlive(bool enable, int sec);
  void SetLinger(int sec);
  void SetNoDelay(bool no_delay);
  void SetReadBuffer(int bytes);
  void SetWriteBuffer(int bytes);

  // SockOpt: optval/optlen use int instead of socklen_t to avoid leaking
  // platform headers. The implementation casts internally.
  bool GetSockOpt(int level, int name, void* optval, int* optlen);
  bool SetSockOpt(int level, int name, const void* optval, int optlen);

  void CloseRead();
  void CloseWrite();
  void Close();

  int64_t TotalReadBytes() const;

  // Returns true if this TcpConn holds a valid connection.
  bool IsValid() const { return impl_ != nullptr; }

 private:
  friend TcpConn MakeTcpConn(class NetFD* netfd);
  explicit TcpConn(std::shared_ptr<TcpConnImpl> impl)
    : impl_(std::move(impl)) {}

  std::shared_ptr<TcpConnImpl> impl_;
};

// Factory: creates a TcpConn from a NetFD. Defined in tcp_conn.cc.
// Declared here so dialer.cc / listener.cc can call it.
TcpConn MakeTcpConn(class NetFD* netfd);

}  // namespace net
}  // namespace tin

#endif  // TIN_NET_TCP_CONN_H_
