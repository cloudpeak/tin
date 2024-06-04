// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <windows.h>
#include <WinSock2.h>
#include <string>

#include "build/build_config.h"
#include "cstdint"

#ifndef _In_reads_bytes_
#define _In_reads_bytes_(s)
#endif
#ifndef _In_reads_bytes_opt_
#define _In_reads_bytes_opt_(s)
#endif

namespace tin {

typedef BOOL(WINAPI* sGetQueuedCompletionStatusEx)
(HANDLE CompletionPort,
 LPOVERLAPPED_ENTRY lpCompletionPortEntries,
 ULONG ulCount,
 PULONG ulNumEntriesRemoved,
 DWORD dwMilliseconds,
 BOOL fAlertable);

typedef BOOL(WINAPI* sCancelIoEx)
(HANDLE hFile,
 LPOVERLAPPED lpOverlapped);

typedef BOOL(WINAPI* sSetFileCompletionNotificationModes)
(HANDLE FileHandle,
 UCHAR Flags);

typedef
BOOL
(PASCAL FAR* LPFN_CONNECTEX) (
  _In_ SOCKET s,
  _In_reads_bytes_(namelen) const struct sockaddr FAR* name,
  _In_ int namelen,
  _In_reads_bytes_opt_(dwSendDataLength) PVOID lpSendBuffer,
  _In_ DWORD dwSendDataLength,
  _Out_ LPDWORD lpdwBytesSent,
  _Inout_ LPOVERLAPPED lpOverlapped
);


extern sGetQueuedCompletionStatusEx pGetQueuedCompletionStatusEx;
extern sSetFileCompletionNotificationModes pSetFileCompletionNotificationModes;
extern sCancelIoEx pCancelIoEx;
extern LPFN_CONNECTEX pConnectEx;
// determines if CancelIoEx API is present
extern bool flag_cancelioex_avaiable;
extern bool global_skip_sync_notificaton;
extern bool hasLoadSetFileCompletionNotificationModes;

bool CanUseConnectEx(const std::string& net);

typedef uintptr_t syshandle;

}  // namespace tin

