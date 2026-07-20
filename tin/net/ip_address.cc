// Copyright (c) 2015 The Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <climits>
#include <cstdint>
#include <utility>

#include <absl/base/macros.h>
#include <absl/log/log.h>
#include <absl/log/check.h>
#include <absl/strings/string_view.h>
#include "absl/strings/str_format.h"
#include "absl/strings/str_cat.h"

#include "tin/net/inet.h"
#include "tin/net/ip_address.h"


namespace {

// The prefix for IPv6 mapped IPv4 addresses.
// https://tools.ietf.org/html/rfc4291#section-2.5.5.2
const uint8_t kIPv4MappedPrefix[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};

// Note that this function assumes:
// * |ip_address| is at least |prefix_length_in_bits| (bits) long;
// * |ip_prefix| is at least |prefix_length_in_bits| (bits) long.
bool IpAddressPrefixCheck(const std::vector<uint8_t>& ip_address,
                          const uint8_t* ip_prefix,
                          size_t prefix_length_in_bits) {
  // Compare all the bytes that fall entirely within the prefix.
  size_t num_entire_bytes_in_prefix = prefix_length_in_bits / 8;
  for (size_t i = 0; i < num_entire_bytes_in_prefix; ++i) {
    if (ip_address[i] != ip_prefix[i])
      return false;
  }

  // In case the prefix was not a multiple of 8, there will be 1 byte
  // which is only partially masked.
  size_t remaining_bits = prefix_length_in_bits % 8;
  if (remaining_bits != 0) {
    uint8_t mask = 0xFF << (8 - remaining_bits);
    size_t i = num_entire_bytes_in_prefix;
    if ((ip_address[i] & mask) != (ip_prefix[i] & mask))
      return false;
  }
  return true;
}

// Returns true if |ip_address| matches any of the reserved IPv4 ranges. This
// method operates on a blacklist of reserved IPv4 ranges. Some ranges are
// consolidated.
// Sources for info:
// www.iana.org/assignments/ipv4-address-space/ipv4-address-space.xhtml
// www.iana.org/assignments/iana-ipv4-special-registry/
// iana-ipv4-special-registry.xhtml

namespace {
struct ReservedIPv4Range {
  const uint8_t address[4];
  size_t prefix_length_in_bits;
};
}
bool IsReservedIPv4(const std::vector<uint8_t>& ip_address) {
  // Different IP versions have different range reservations.
  DCHECK_EQ(tin::net::IpAddress::kIPv4AddressSize, ip_address.size());
  static const ReservedIPv4Range kReservedIPv4Ranges[] = {
    {{0, 0, 0, 0}, 8},     {{10, 0, 0, 0}, 8},      {{100, 64, 0, 0}, 10},
    {{127, 0, 0, 0}, 8},   {{169, 254, 0, 0}, 16},  {{172, 16, 0, 0}, 12},
    {{192, 0, 2, 0}, 24},  {{192, 88, 99, 0}, 24},  {{192, 168, 0, 0}, 16},
    {{198, 18, 0, 0}, 15}, {{198, 51, 100, 0}, 24}, {{203, 0, 113, 0}, 24},
    {{224, 0, 0, 0}, 3}
  };

  for (auto kReservedIPv4Range : kReservedIPv4Ranges) {
    if (IpAddressPrefixCheck(ip_address, kReservedIPv4Range.address,
                             kReservedIPv4Range.prefix_length_in_bits)) {
      return true;
    }
  }

  return false;
}

// Returns true if |ip_address| matches any of the reserved IPv6 ranges. This
// method operates on a whitelist of non-reserved IPv6 ranges. All IPv6
// addresses outside these ranges are reserved.
// Sources for info:
// www.iana.org/assignments/ipv6-address-space/ipv6-address-space.xhtml

namespace {
struct PublicIPv6Range {
  const uint8_t address_prefix[2];
  size_t prefix_length_in_bits;
};
}

bool IsReservedIPv6(const std::vector<uint8_t>& ip_address) {
  // Different IP versions have different range reservations.
  DCHECK_EQ(tin::net::IpAddress::kIPv6AddressSize, ip_address.size());
  static const PublicIPv6Range kPublicIPv6Ranges[] = {
    // 2000::/3  -- Global Unicast
    {{0x20, 0}, 3},
    // ff00::/8  -- Multicast
    {{0xff, 0}, 8},
  };

  for (auto kPublicIPv6Range : kPublicIPv6Ranges) {
    if (IpAddressPrefixCheck(ip_address, kPublicIPv6Range.address_prefix,
                             kPublicIPv6Range.prefix_length_in_bits)) {
      return false;
    }
  }

  return true;
}

bool ParseIPLiteralToBytes(const absl::string_view& ip_literal,
                           std::vector<uint8_t>* bytes) {
  // |ip_literal| could be either an IPv4 or an IPv6 literal. If it contains
  // a colon however, it must be an IPv6 address.
  bool ipv4 = ip_literal.find(':') == absl::string_view::npos;
  bytes->resize(ipv4 ? 4 : 16);  // 128 bits.
  return tin::net::INetPToN(ipv4, ip_literal.data(), bytes->data());
}

}  // namespace
namespace tin::net {

IpAddress::IpAddress() {}

IpAddress::IpAddress(const std::vector<uint8_t>& address)
  : ip_address_(address) {}

IpAddress::IpAddress(const uint8_t* address, size_t address_len)
  : ip_address_(address, address + address_len) {}

IpAddress::IpAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  ip_address_.reserve(4);
  ip_address_.push_back(b0);
  ip_address_.push_back(b1);
  ip_address_.push_back(b2);
  ip_address_.push_back(b3);
}

IpAddress::IpAddress(uint8_t b0,
                     uint8_t b1,
                     uint8_t b2,
                     uint8_t b3,
                     uint8_t b4,
                     uint8_t b5,
                     uint8_t b6,
                     uint8_t b7,
                     uint8_t b8,
                     uint8_t b9,
                     uint8_t b10,
                     uint8_t b11,
                     uint8_t b12,
                     uint8_t b13,
                     uint8_t b14,
                     uint8_t b15) {
  const uint8_t address[] = {b0, b1, b2,  b3,  b4,  b5,  b6,  b7,
                           b8, b9, b10, b11, b12, b13, b14, b15
                          };
  ip_address_.assign(address, address + ABSL_ARRAYSIZE(address));
}


IpAddress::IpAddress(const IpAddress& other)
  : ip_address_(other.ip_address_) {}

IpAddress::~IpAddress() {}

bool IpAddress::IsIPv4() const {
  return ip_address_.size() == kIPv4AddressSize;
}

bool IpAddress::IsIPv6() const {
  return ip_address_.size() == kIPv6AddressSize;
}

bool IpAddress::IsValid() const {
  return IsIPv4() || IsIPv6();
}

bool IpAddress::IsReserved() const {
  if (IsIPv4()) {
    return IsReservedIPv4(ip_address_);
  } else if (IsIPv6()) {
    return IsReservedIPv6(ip_address_);
  }
  return false;
}

bool IpAddress::IsZero() const {
  for (unsigned i = 0; i < ip_address_.size(); ++i) {
    if (ip_address_[i] != 0)
      return false;
  }

  return !empty();
}

bool IpAddress::IsIPv4MappedIPv6() const {
  return IsIPv6() && IpAddressStartsWith(*this, kIPv4MappedPrefix);
}

bool IpAddress::AssignFromIPLiteral(const absl::string_view& ip_literal) {
  std::vector<uint8_t> number;

  if (!ParseIPLiteralToBytes(ip_literal, &number))
    return false;

  std::swap(number, ip_address_);
  return true;
}

// static
IpAddress IpAddress::IPv4Localhost() {
  static const uint8_t kLocalhostIPv4[] = {127, 0, 0, 1};
  return IpAddress(kLocalhostIPv4);
}

// static
IpAddress IpAddress::IPv6Localhost() {
  static const uint8_t kLocalhostIPv6[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0, 0, 1
                                        };
  return IpAddress(kLocalhostIPv6);
}

// static
IpAddress IpAddress::AllZeros(size_t num_zero_bytes) {
  return IpAddress(std::vector<uint8_t>(num_zero_bytes));
}

// static
IpAddress IpAddress::IPv4AllZeros() {
  return AllZeros(kIPv4AddressSize);
}

// static
IpAddress IpAddress::IPv6AllZeros() {
  return AllZeros(kIPv6AddressSize);
}

bool IpAddress::operator==(const IpAddress& that) const {
  return ip_address_ == that.ip_address_;
}

bool IpAddress::operator!=(const IpAddress& that) const {
  return ip_address_ != that.ip_address_;
}

bool IpAddress::operator<(const IpAddress& that) const {
  // Sort IPv4 before IPv6.
  if (ip_address_.size() != that.ip_address_.size()) {
    return ip_address_.size() < that.ip_address_.size();
  }

  return ip_address_ < that.ip_address_;
}

std::string IpAddress::ToString() const {
  std::string str;
  InetNToP(IsIPv4(), &ip_address_[0], &str);
  return str;
}

std::string IpAddressToStringWithPort(const IpAddress& address, uint16_t port) {
  std::string address_str = address.ToString();
  if (address_str.empty())
    return address_str;

  if (address.IsIPv6()) {
    // Need to bracket IPv6 addresses since they contain colons.
    return absl::StrCat("[", address_str, "]:", port);
  }
  return absl::StrCat(address_str, ":", port);
}

std::string IpAddressToPackedString(const IpAddress& address) {
  return std::string(reinterpret_cast<const char*>(
                       &address.bytes()[0]), address.size());
}

IpAddress ConvertIPv4ToIPv4MappedIPv6(const IpAddress& address) {
  DCHECK(address.IsIPv4());
  // IPv4-mapped addresses are formed by:
  // <80 bits of zeros>  + <16 bits of ones> + <32-bit IPv4 address.
  std::vector<uint8_t> bytes;
  bytes.reserve(16);
  bytes.insert(bytes.end(), kIPv4MappedPrefix,
               kIPv4MappedPrefix + ABSL_ARRAYSIZE(kIPv4MappedPrefix));
  bytes.insert(bytes.end(), address.bytes().begin(), address.bytes().end());
  return IpAddress(bytes);
}

IpAddress ConvertIPv4MappedIPv6ToIPv4(const IpAddress& address) {
  DCHECK(address.IsIPv4MappedIPv6());

  return IpAddress(std::vector<uint8_t>(
                     address.bytes().begin() + ABSL_ARRAYSIZE(kIPv4MappedPrefix),
                     address.bytes().end()));
}

bool IpAddressMatchesPrefix(const IpAddress& ip_address,
                            const IpAddress& ip_prefix,
                            size_t prefix_length_in_bits) {
  // Both the input IP address and the prefix IP address should be either IPv4
  // or IPv6.
  DCHECK(ip_address.IsValid());
  DCHECK(ip_prefix.IsValid());

  DCHECK_LE(prefix_length_in_bits, ip_prefix.size() * 8);

  // In case we have an IPv6 / IPv4 mismatch, convert the IPv4 addresses to
  // IPv6 addresses in order to do the comparison.
  if (ip_address.size() != ip_prefix.size()) {
    if (ip_address.IsIPv4()) {
      return IpAddressMatchesPrefix(ConvertIPv4ToIPv4MappedIPv6(ip_address),
                                    ip_prefix, prefix_length_in_bits);
    }
    return IpAddressMatchesPrefix(ip_address,
                                  ConvertIPv4ToIPv4MappedIPv6(ip_prefix),
                                  96 + prefix_length_in_bits);
  }

  return IpAddressPrefixCheck(ip_address.bytes(),
                              &ip_prefix.bytes()[0],
                              prefix_length_in_bits);
}

bool ParseURLHostnameToAddress(const absl::string_view& hostname,
                               IpAddress* ip_address) {
  if (hostname.size() >= 2 && hostname.front() == '[' &&
      hostname.back() == ']') {
    // Strip the square brackets that surround IPv6 literals.
    absl::string_view ip_literal =
            absl::string_view(hostname).substr(1, hostname.size() - 2);
    return ip_address->AssignFromIPLiteral(ip_literal) && ip_address->IsIPv6();
  }

  return ip_address->AssignFromIPLiteral(hostname) && ip_address->IsIPv4();
}

unsigned CommonPrefixLength(const IpAddress& a1, const IpAddress& a2) {
  DCHECK_EQ(a1.size(), a2.size());
  for (size_t i = 0; i < a1.size(); ++i) {
    unsigned diff = a1.bytes()[i] ^ a2.bytes()[i];
    if (!diff)
      continue;
    for (unsigned j = 0; j < CHAR_BIT; ++j) {
      if (diff & (1 << (CHAR_BIT - 1)))
        return static_cast<unsigned>(i * CHAR_BIT + j);
      diff <<= 1;
    }
  }
  return static_cast<unsigned>(a1.size() * CHAR_BIT);
}

unsigned MaskPrefixLength(const IpAddress& mask) {
  std::vector<uint8_t> all_ones(mask.size(), 0xFF);
  return CommonPrefixLength(mask, IpAddress(all_ones));
}

}  // namespace tin::net

