// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tin/net/sys_socket.h"
#include "tin/error/error.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/net/pollops.h"
#include "tin/net/net.h"
#include "tin/net/sockaddr_storage.h"
#include "tin/net/ip_address.h"

#include "tin/net/netfd_common.h"

namespace tin {
namespace net {

NetFDCommon::NetFDCommon(uintptr_t sysfd,
                         AddressFamily family,
                         int sotype,
                         const std::string& net)
  : sysfd_(sysfd)
  , family_(family)
  , sotype_(sotype)
  , net_(net) {
}

NetFDCommon::~NetFDCommon() {
  // note: do never ever call Destroy here(destructor), it is implemented in
  // sub-class NetFD as virtual.
}

int NetFDCommon::Close() {
  if (!fdmu_.IncrefAndClose()) {
    return TIN_ECLOSE_INTR;
  }
  // unblock pending reader and writer
  pd_.Evict();
  Decref();
  return 0;
}

int NetFDCommon::Incref() {
  if (!fdmu_.Incref()) {
    return TIN_ECLOSE_INTR;
  }
  return 0;
}

void NetFDCommon::Decref() {
  if (fdmu_.Deref()) {
    Destroy();
  }
}

int NetFDCommon::ReadLock() {
  if (!fdmu_.RWLock(true)) {
    return TIN_ECLOSE_INTR;
  }
  return 0;
}

void NetFDCommon::ReadUnlock() {
  if (fdmu_.RWUnlock(true)) {
    Destroy();
  }
}

int NetFDCommon::WriteLock() {
  if (!fdmu_.RWLock(false)) {
    return TIN_ECLOSE_INTR;
  }
  return 0;
}

void NetFDCommon::WriteUnlock() {
  if (fdmu_.RWUnlock(false)) {
    Destroy();
  }
}

int NetFDCommon::SetDeadline(int64_t t) {
  return SetDeadlineImpl(t, 'r' + 'w');
}

int NetFDCommon::SetReadDeadline(int64_t t) {
  return SetDeadlineImpl(t, 'r');
}

int NetFDCommon::SetWriteDeadline(int64_t t) {
  return SetDeadlineImpl(t, 'w');
}

int NetFDCommon::SetDeadlineImpl(int64_t t, int mode) {
  int64_t now = MonoNow();
  int64_t d = now + t;
  // test overflow.
  if (std::numeric_limits<int64_t>::max() - now < t) {
    d = std::numeric_limits<int64_t>::max();
  }
  if (t == 0) {
    d = 0;
  }
  int err = Incref();
  if (err != 0)
    return err;
  tin::runtime::pollops::SetDeadline(pd_.Desc(), d, mode);
  Decref();
  return 0;
}

}  // namespace net
}  // namespace tin
