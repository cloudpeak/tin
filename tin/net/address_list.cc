// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tin/net/address_list.h"

#include <utility>

#include <absl/log/log.h>
#include <absl/log/check.h>
#include "tin/net/sys_addrinfo.h"

namespace tin {
namespace net {


AddressList::AddressList() {}

AddressList::~AddressList() {}

AddressList::AddressList(const IPEndPoint& endpoint) {
  push_back(endpoint);
}

// static
AddressList AddressList::CreateFromIPAddress(const IPAddress& address,
    uint16_t port) {
  return AddressList(IPEndPoint(address, port));
}

// static
AddressList AddressList::CreateFromIPAddressList(
  const IPAddressList& addresses,
  const std::string& canonical_name) {
  AddressList list;
  list.set_canonical_name(canonical_name);
  for (IPAddressList::const_iterator iter = addresses.begin();
       iter != addresses.end(); ++iter) {
    list.push_back(IPEndPoint(*iter, 0));
  }
  return list;
}

// static
AddressList AddressList::CreateFromAddrinfo(const struct addrinfo* head) {
  DCHECK(head);
  AddressList list;
  if (head->ai_canonname)
    list.set_canonical_name(std::string(head->ai_canonname));
  for (const struct addrinfo* ai = head; ai; ai = ai->ai_next) {
    IPEndPoint ipe;
    // NOTE: Ignoring non-INET* families.
    if (ipe.FromSockAddr(ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen)))
      list.push_back(ipe);
    else
      DLOG(WARNING) << "Unknown family found in addrinfo: " << ai->ai_family;
  }
  return list;
}

// static
AddressList AddressList::CopyWithPort(const AddressList& list, uint16_t port) {
  AddressList out;
  out.set_canonical_name(list.canonical_name());
  for (size_t i = 0; i < list.size(); ++i)
    out.push_back(IPEndPoint(list[i].address(), port));
  return out;
}

void AddressList::SetDefaultCanonicalName() {
  DCHECK(!empty());
  set_canonical_name(front().ToStringWithoutPort());
}

}  // namespace net
}  // namespace tin
