// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <WinSock2.h>
#include <MSWSock.h>
#include "tin/platform/platform.h"
#include "tin/platform/platform_win.h"

namespace tin {

bool    flag_cancelioex_avaiable;  // determines if CancelIoEx API is present
bool    global_skip_sync_notificaton;
bool    hasLoadSetFileCompletionNotificationModes;

bool CanUseConnectEx(const std::string& net) {
  if (net == "udp" ||
      net == "udp4" ||
      net == "udp6" ||
      net == "ip" ||
      net == "ip4" ||
      net == "ip6") {
    return false;
  }
  return pConnectEx != NULL;
}

static BOOL TinGetExtensionFunction(SOCKET socket, GUID guid,
                                       void** target) {
  int result;
  DWORD bytes;

  result = WSAIoctl(socket,
                    SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guid,
                    sizeof(guid),
                    static_cast<void*>(target),
                    sizeof(*target),
                    &bytes,
                    NULL,
                    NULL);

  if (result == SOCKET_ERROR) {
    *target = NULL;
    return FALSE;
  } else {
    return TRUE;
  }
}

bool PlatformInit() {
  HMODULE ntdll_module;
  HMODULE kernel32_module;

  ntdll_module = GetModuleHandleA("ntdll.dll");
  if (ntdll_module == NULL) {
    return false;
  }

  kernel32_module = GetModuleHandleA("kernel32.dll");
  if (kernel32_module == NULL) {
    return false;
  }

  pGetQueuedCompletionStatusEx = (sGetQueuedCompletionStatusEx)GetProcAddress(
                                   kernel32_module,
                                   "GetQueuedCompletionStatusEx");

  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);

  pCancelIoEx = (sCancelIoEx)
                GetProcAddress(kernel32_module, "CancelIoEx");

  flag_cancelioex_avaiable = (pCancelIoEx != NULL);
  {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    GUID guid = WSAID_CONNECTEX;
    TinGetExtensionFunction(s, guid, (void**)&pConnectEx);  // NOLINT
    closesocket(s);
  }

  pSetFileCompletionNotificationModes =
    (sSetFileCompletionNotificationModes)
    GetProcAddress(kernel32_module, "SetFileCompletionNotificationModes");

  if (pSetFileCompletionNotificationModes) {
    hasLoadSetFileCompletionNotificationModes = true;
    // It's not safe to use FILE_SKIP_COMPLETION_PORT_ON_SUCCESS
    // if non IFS providers are installed:
    // http://support.microsoft.com/kb/2568167
    global_skip_sync_notificaton = true;

    int32_t protos[2] = { IPPROTO_TCP, 0 };
    WSAPROTOCOL_INFO  buf[32];
    DWORD len = sizeof(buf);
    int n = WSAEnumProtocols(&protos[0], &buf[0], &len);
    if (n == SOCKET_ERROR) {
      global_skip_sync_notificaton = false;
    } else {
      for (int i = 0; i < n; i++) {
        if ((buf[i].dwServiceFlags1 & XP1_IFS_HANDLES) == 0) {
          global_skip_sync_notificaton = false;
          break;
        }
      }
    }
  }
  return true;
}

void PlatformDeinit() {
}

sGetQueuedCompletionStatusEx pGetQueuedCompletionStatusEx = NULL;
sSetFileCompletionNotificationModes pSetFileCompletionNotificationModes = NULL;
sCancelIoEx pCancelIoEx = NULL;
LPFN_CONNECTEX pConnectEx = NULL;

}  // namespace tin
