// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <string>
#include "base/strings/string_piece.h"
#include "tin/net/fd_mutex.h"
#include "tin/net/poll_desc.h"
#include "tin/net/address_list.h"
#include "tin/net/ip_endpoint.h"
#include "tin/net/sockaddr_storage.h"

namespace tin {
namespace net {

const uintptr_t kInvalidSocket = uintptr_t(~0);

class NetFDCommon {
 public:
  NetFDCommon(uintptr_t sysfd,
              AddressFamily family,
              int sotype,
              const std::string& net);

  virtual ~NetFDCommon();

  int Incref();

  int ReadLock();

  void ReadUnlock();

  int WriteLock();

  void WriteUnlock();

  virtual void Destroy() = 0;

  int Close();

  int SetDeadline(int64 t);

  int SetReadDeadline(int64 t);

  int SetWriteDeadline(int64 t);

  int SetDeadlineImpl(int64 t, int mode);

  void Decref();

  PollDesc* Pd() {
    return &pd_;
  }

  uintptr_t SysFd() {
    return sysfd_;
  }

  int IntFd() const {
    return static_cast<int>(sysfd_);
  }

 protected:
  FdMutex fdmu_;
  uintptr_t sysfd_;
  AddressFamily family_;
  int sotype_;
  int is_connected_;
  std::string  net_;
  PollDesc pd_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetFDCommon);
};
}  // namespace net
}  // namespace tin





