// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "tin/time/time.h"
#include "tin/net/tcp_conn.h"

namespace tin {
namespace net {

class NetFD;

class TCPListenerImpl
  : public base::RefCountedThreadSafe<TCPListenerImpl> {
 public:
  TCPListenerImpl(NetFD* netfd, int backlog);
  ~TCPListenerImpl();

  void SetDeadline(int64 t);
  TcpConn Accept();
  void Close();

 private:
  NetFD* netfd_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TCPListenerImpl);
};

class TCPListener
  : public scoped_refptr<TCPListenerImpl> {
 public:
  explicit TCPListener(TCPListenerImpl* t)
    : scoped_refptr<TCPListenerImpl>(t) {
  }
};

inline TCPListener MakeTcpListener(TCPListenerImpl* listener) {
  return TCPListener(listener);
}

}  // namespace net
}  // namespace tin


