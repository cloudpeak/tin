// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once


#include "base/memory/ref_counted.h"
#include "tin/time/time.h"
#include "tin/io/io.h"

namespace tin {
namespace net {

class NetFD;

class TcpConnImpl
  : public base::RefCountedThreadSafe<TcpConnImpl>
  , public tin::io::IOReadWriter {
 public:
  explicit TcpConnImpl(NetFD* netfd);

  virtual ~TcpConnImpl();

  // note: Read full or partial on success, or read partial on failure.
  // return value : indicate n bytes written. n >= 0.
  // don't handle error based on return value.
  // detail error, see tin::GetErrorCode()
  int Read(void* buf, int nbytes);

  // note: Write full on success, or write partial on failure.
  // return value : indicate n bytes written. n >= 0.
  // don't handle error based on return value.
  // detail error, see tin::GetErrorCode()
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
  DISALLOW_COPY_AND_ASSIGN(TcpConnImpl);
};

class TcpConn
  : public scoped_refptr<TcpConnImpl> {
 public:
  explicit TcpConn(TcpConnImpl* t)
    : scoped_refptr<TcpConnImpl>(t) {
  }
};

inline TcpConn MakeTcpConn(TcpConnImpl* conn) {
  return TcpConn(conn);
}

}  // namespace net
}  // namespace tin



