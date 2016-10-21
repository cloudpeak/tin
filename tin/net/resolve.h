// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string_piece.h"
#include "tin/net/address_family.h"
#include "tin/net/ip_address.h"

namespace tin {
namespace net {

int ResolveHostname(const base::StringPiece& hostname, AddressFamily af,
                    std::vector<IPAddress>* addresses);

IPAddress ResolveHostname4(const base::StringPiece& hostname);

IPAddress ResolveHostname6(const base::StringPiece& hostname);

IPAddress ResolveHostname(const base::StringPiece& hostname);

IPAddress ResolveHostname(const base::StringPiece& hostname);

IPAddress ResolveHostname(const base::StringPiece& hostname, AddressFamily af);

}  // namespace net
}  // namespace tin

