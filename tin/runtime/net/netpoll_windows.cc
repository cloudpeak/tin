// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <WinSock2.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include "tin/platform/platform_win.h"
#include "tin/runtime/runtime.h"

#include "tin/runtime/net/NetPoll.h"


namespace tin::runtime {

struct PollDescriptor;

struct NetOP {
  // used by windows
  OVERLAPPED ol;
  // used by NetPoll
  PollDescriptor* pd;
  int32_t mode;
  int32_t eno;
  uint32_t qty;
};

struct overlappedEntry {
  uintptr_t key;
  NetOP* op;
  uintptr_t internal;
  uint32_t qty;
};

HANDLE iocphandle = INVALID_HANDLE_VALUE;

void NetPollInit() {
  iocphandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0xFFFFFFFF);
  if (iocphandle == 0) {
    LOG(FATAL) << "NetPoll: failed to create iocp handle";
  }
}

void NetPollShutdown() {
  if (!NetPollInited())
    return;
  PostQueuedCompletionStatus(iocphandle, 0, nullptr, nullptr);
}

void NetPollPreDeinit() {
  CloseHandle(iocphandle);
  iocphandle = INVALID_HANDLE_VALUE;
}

int32_t NetPollOpen(uintptr_t fd, PollDescriptor* pd) {
  if (CreateIoCompletionPort((HANDLE)fd, iocphandle, 0, 0) == 0) {
    return static_cast<int32_t>(GetLastError());
  }
  return 0;
}

int32_t NetPollClose(uintptr_t fd) {
  // nothing to do
  return 0;
}

void NetPollArm(PollDescriptor* pd, int mode) {
  LOG(FATAL) << "unused";
}

void handlecompletion(G** gpp, NetOP* op, DWORD error_no, uint32_t qty) {
  if (op == nullptr) {
    LOG(FATAL) << "NetPoll: GetQueuedCompletionStatus returned op == nil";
  }
  int32_t mode = op->mode;
  if (mode != 'r' && mode != 'w') {
    LOG(FATAL) << "NetPoll: GetQueuedCompletionStatus returned invalid mode";
  }
  op->eno = error_no;
  op->qty = qty;
  NetPollReady(gpp, op->pd, mode);
}

G* NetPoll(bool block) {
  overlappedEntry entries[64];
  DWORD qty, flags, i;
  ULONG_PTR  key = 0;
  ULONG n;
  DWORD error_no;
  NetOP* op;
  G* gp = nullptr;
  DWORD wait = 0;
  if (block) {
    wait = INFINITE;
  }

  bool shutdown_flag = false;

  if (pGetQueuedCompletionStatusEx != nullptr) {
    n = 64;

    if (pGetQueuedCompletionStatusEx(iocphandle,
                                     (LPOVERLAPPED_ENTRY)&entries[0],
                                     n, &n, wait, 0) == 0) {
      error_no = GetLastError();
      if (!block && error_no == WAIT_TIMEOUT) {
        return nullptr;
      }
      LOG(INFO) << "NetPoll: GetQueuedCompletionStatusEx failed";
      return nullptr;
    }
    for (i = 0; i < n; i++) {
      op = entries[i].op;
      error_no = 0;
      qty = 0;
      if (op != nullptr) {
        if (WSAGetOverlappedResult(op->pd->fd,
                                   (LPWSAOVERLAPPED)op,
                                   (LPDWORD)&qty,
                                   0,
                                   (LPDWORD)&flags) == 0) {
          error_no = GetLastError();
        }
        handlecompletion(&gp, op, error_no, qty);
      } else {
        shutdown_flag = true;
      }
    }
  } else {
    op = nullptr;
    error_no = 0;
    qty = 0;
    if (GetQueuedCompletionStatus(iocphandle,
                                  &qty,
                                  &key,
                                  reinterpret_cast<LPOVERLAPPED*>(&op),
                                  wait) == 0) {
      error_no = GetLastError();
      if (!block && error_no == WAIT_TIMEOUT) {
        return nullptr;
      }
      if (op == nullptr) {
        LOG(INFO) << "NetPoll: GetQueuedCompletionStatus failed";
        return nullptr;
      }
    }
    if (op != nullptr) {
      handlecompletion(&gp, op, error_no, qty);
    } else {
      shutdown_flag = true;
    }
  }

  if (shutdown_flag) {
    PostQueuedCompletionStatus(iocphandle, 0, nullptr, nullptr);
  }
  return gp;
}

}  // namespace tin::runtime
