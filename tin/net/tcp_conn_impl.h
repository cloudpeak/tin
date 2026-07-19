// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// P2-1 PIMPL: Internal implementation header for TcpConnImpl.
// The public TcpConn class is defined in include/tin/net/tcp_conn.h.
// This file is NOT part of the public API.

#pragma once

#include <memory>

#include "tin/net/sys_socket.h"
#include "tin/time/time.h"
#include "tin/io/io.h"

namespace tin {
namespace net {

class NetFD;
class TcpConn;  // forward-declared; full definition in public header

class TcpConnImpl
  : public std::enable_shared_from_this<TcpConnImpl>
  , public tin::io::IOReadWriter {
 public:
  explicit TcpConnImpl(NetFD* netfd);
  virtual ~TcpConnImpl();

  TcpConnImpl(const TcpConnImpl&) = delete;
  TcpConnImpl& operator=(const TcpConnImpl&) = delete;

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

  bool GetSockOpt(int level, int name, void* optval, socklen_t* optlen);
  bool SetSockOpt(int level, int name, const void* optval, socklen_t optlen);

  void CloseRead();
  void CloseWrite();
  void Close();

  int64_t TotalReadBytes() const {
    return total_read_bytes_;
  }

 private:
  NetFD* netfd_;
  int64_t total_read_bytes_;
};

}  // namespace net
}  // namespace tin
