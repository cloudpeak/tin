// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>

#include <absl/strings/string_view.h>
#include "tin/net/address_family.h"
#include "tin/net/ip_address.h"

namespace tin {
namespace net {

int ResolveHostname(const absl::string_view& hostname, AddressFamily af,
                    std::vector<IPAddress>* addresses);

IPAddress ResolveHostname4(const absl::string_view& hostname);

IPAddress ResolveHostname6(const absl::string_view& hostname);

IPAddress ResolveHostname(const absl::string_view& hostname);

IPAddress ResolveHostname(const absl::string_view& hostname);

IPAddress ResolveHostname(const absl::string_view& hostname, AddressFamily af);

}  // namespace net
}  // namespace tin

