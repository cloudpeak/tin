// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DEPRECATED: This header is a kitchen-sink aggregate kept for backwards
// compatibility. New code should include only the specific tin headers it
// needs, for example:
//
//   #include "tin/tin.h"       // lifecycle API (Runtime, Run, ...)
//   #include "tin/net/tcp.h"   // TcpConn, ListenTcp, DialTcp
//   #include "tin/runtime.h"   // Spawn, Sched, LockOSThread
//   #include "tin/time.h"      // kSecond, Now, NanoSleep
//   #include "tin/error.h"     // GetErrorCode, GetErrorStr, TIN_*
//
// See docs/api-design-review.md for the recommended include pattern.

#pragma once

#ifndef _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
#define _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
#endif

#include <absl/log/log.h>
#include <absl/log/check.h>

#include "tin/tin.h"
#include "tin/config.h"
#include "tin/error.h"
#include "tin/time.h"
#include "tin/runtime.h"
#include "tin/net/tcp.h"
#include "tin/net/netfd.h"
#include "tin/sync/atomic.h"
#include "tin/sync/atomic_flag.h"
#include "tin/sync/mutex.h"
#include "tin/sync/wait_group.h"
#include "tin/communication/chan.h"
#include "tin/io/io.h"

#include <thread>
