// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_NET_RESOLVE_H_
#define TIN_NET_RESOLVE_H_
#include <vector>

#include <absl/strings/string_view.h>
#include "tin/net/address_family.h"
#include "tin/net/ip_address.h"
#include "tin/result.h"

namespace tin::net {

// Resolves hostname into a list of IP addresses. Returns Status.
Status ResolveHostname(const absl::string_view& hostname, AddressFamily af,
                       std::vector<IpAddress>* addresses);

// Convenience functions that return the first resolved address.
Result<IpAddress> ResolveHostname4(const absl::string_view& hostname);

Result<IpAddress> ResolveHostname6(const absl::string_view& hostname);

Result<IpAddress> ResolveHostname(const absl::string_view& hostname);

Result<IpAddress> ResolveHostname(const absl::string_view& hostname,
                                  AddressFamily af);

}  // namespace tin::net
#endif  // TIN_NET_RESOLVE_H_
