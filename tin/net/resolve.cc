// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#if defined(OS_WIN)
#include <ws2spi.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <memory>
#include <vector>

#include <absl/log/log.h>
#include <absl/log/check.h>

#include "tin/error/error.h"
#include "tin/runtime/util.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/threadpoll.h"
#include "tin/net/ip_endpoint.h"

#include "tin/net/resolve.h"

namespace tin {
namespace net {

using namespace runtime;  // NOLINT

int SyncResolveHostname(const absl::string_view& hostname, AddressFamily af,
                        std::vector<IPAddress>* addresses) {
  int family = ConvertAddressFamily(af);
  addresses->clear();
  struct addrinfo* result = nullptr;
  struct addrinfo hints = { 0 };
  hints.ai_family = family;

  // The behavior of AF_UNSPEC is roughly "get both ipv4 and ipv6", as
  // documented by the various operating systems:
  // Linux: http://man7.org/linux/man-pages/man3/getaddrinfo.3.html
  // Windows: https://msdn.microsoft.com/en-us/library/windows/desktop/
  // ms738520(v=vs.85).aspx
  // Mac: https://developer.apple.com/legacy/library/documentation/Darwin/
  // Reference/ManPages/man3/getaddrinfo.3.html
  // Android (source code, not documentation):
  // https://android.googlesource.com/platform/bionic/+/
  // 7e0bfb511e85834d7c6cb9631206b62f82701d60/libc/netbsd/net/getaddrinfo.c#1657
  hints.ai_flags = AI_ADDRCONFIG;
  int ret = getaddrinfo(hostname.data(), nullptr, &hints, &result);
  if (ret != 0) {
    return ret;
  }
  struct addrinfo* cursor = result;
  for (; cursor; cursor = cursor->ai_next) {
    if (family == AF_UNSPEC || cursor->ai_family == family) {
      IPEndPoint endpoint;
      if (endpoint.FromSockAddr(cursor->ai_addr,
                                static_cast<socklen_t>(cursor->ai_addrlen))) {
        addresses->push_back(endpoint.address());
      }
    }
  }
  freeaddrinfo(result);
  return 0;
}

class ResolveHostnameWork : public GletWork {
 public:
  ResolveHostnameWork(const absl::string_view& hostname,  // NOLINT
                      AddressFamily& family,  // NOLINT
                      std::vector<IPAddress>*& addresses)  // NOLINT
    : result_(0)
    , hostname_(hostname)
    , family_(family)
    , addresses_(addresses) {
  }

  virtual ~ResolveHostnameWork() { }

  virtual void Run() {
    result_ = SyncResolveHostname(hostname_, family_, addresses_);
    Finalize();
  }

  int Result() {
    return result_;
  }

 private:
  int  result_;
  const absl::string_view& hostname_;
  AddressFamily& family_;
  std::vector<IPAddress>*& addresses_;
};

Status ResolveHostname(const absl::string_view& hostname,
                       AddressFamily family,
                       std::vector<IPAddress>* addresses) {
  if (addresses == nullptr) {
    return Status::FromErrno(TIN_EINVAL);
  }
  std::unique_ptr<ResolveHostnameWork> work(
    new ResolveHostnameWork(hostname, family, addresses));
  SubmitGetAddrInfoGletWork(work.get());
  int ret = work->Result();
  if (ret != 0) {
    return Status::FromErrno(TinGetaddrinfoTranslateError(ret));
  }
  return Status::OK();
}

// handy functions.
Result<IPAddress> ResolveHostname(const absl::string_view& hostname,
                                  AddressFamily af) {
  std::vector<IPAddress> addresses;
  Status s = ResolveHostname(hostname, af, &addresses);
  if (!s.ok()) {
    return Result<IPAddress>::Err(s);
  }
  if (addresses.empty()) {
    // must not be empty on success
    LOG(FATAL) << "ResolveHostname return empty list on success";
  }
  return Result<IPAddress>::Ok(addresses.front());
}

Result<IPAddress> ResolveHostname(const absl::string_view& hostname) {
  return ResolveHostname(hostname, ADDRESS_FAMILY_UNSPECIFIED);
}

Result<IPAddress> ResolveHostname4(const absl::string_view& hostname) {
  return ResolveHostname(hostname, ADDRESS_FAMILY_IPV4);
}

Result<IPAddress> ResolveHostname6(const absl::string_view& hostname) {
  return ResolveHostname(hostname, ADDRESS_FAMILY_IPV6);
}

}  // namespace net
}  // namespace tin
