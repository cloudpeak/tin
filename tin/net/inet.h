// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <string>

namespace tin {
namespace net {

int InetNToP(int af, const void* src, char* dst, size_t size);

int INetPToN(int af, const char* src, void* dst);

// c++ version.
bool InetNToP(bool ipv4, const void* src, std::string* dst);

bool INetPToN(bool ipv4, const char* src, void* dst);

}  // namespace net
}  // namespace tin
