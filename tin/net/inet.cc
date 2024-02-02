// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include "build/build_config.h"
#if defined(OS_WIN)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "cstdint"
#include "base/strings/string_util.h"
#include "base/sys_byteorder.h"
#include "tin/error/error.h"

#include "tin/net/inet.h"

namespace tin {
namespace net {

const int kInetAddrStrLen = 16;
const int kInetAddrStrLen6 = 46;

namespace {
struct Inet6Addr {
  union {
    uint8       Byte[16];
    uint16      Word[8];
  } u;
};
}

int INetNToP4(const unsigned char* src, char* dst, size_t size) {
  static const char fmt[] = "%u.%u.%u.%u";
  char tmp[kInetAddrStrLen];
  int l;

  l = base::snprintf(tmp, sizeof(tmp), fmt, src[0], src[1], src[2], src[3]);
  if (l <= 0 || (size_t) l >= size) {
    return TIN_ENOSPC;
  }
  base::strlcpy(dst, tmp, size);
  dst[size - 1] = '\0';
  return 0;
}

static int INetNToP6(const unsigned char* src, char* dst, size_t size) {
  /*
   * Note that int32_t and int16 need only be "at least" large enough
   * to contain a value of the specified size.  On some systems, like
   * Crays, there is no such thing as an integer variable with 16 bits.
   * Keep this in mind if you think this function should have been coded
   * to use pointer overlays.  All the world's not a VAX.
   */
  char tmp[kInetAddrStrLen6], *tp;
  struct {
    int base, len;
  } best, cur;
  unsigned int words[sizeof(Inet6Addr) / sizeof(uint16)];
  int i;

  /*
   * Preprocess:
   *  Copy the input (bytewise) array into a wordwise array.
   *  Find the longest run of 0x00's in src[] for :: shorthanding.
   */
  memset(words, '\0', sizeof words);
  for (i = 0; i < (int) sizeof(Inet6Addr); i++)  // NOLINT
    words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
  best.base = -1;
  best.len = 0;
  cur.base = -1;
  cur.len = 0;
  for (i = 0; i < (int) arraysize(words); i++) {    // NOLINT
    if (words[i] == 0) {
      if (cur.base == -1)
        cur.base = i, cur.len = 1;
      else
        cur.len++;
    } else {
      if (cur.base != -1) {
        if (best.base == -1 || cur.len > best.len)
          best = cur;
        cur.base = -1;
      }
    }
  }
  if (cur.base != -1) {
    if (best.base == -1 || cur.len > best.len)
      best = cur;
  }
  if (best.base != -1 && best.len < 2)
    best.base = -1;

  /*
   * Format the result.
   */
  tp = tmp;
  for (i = 0; i < static_cast<int>(arraysize(words)); i++) {
    /* Are we inside the best run of 0x00's? */
    if (best.base != -1 && i >= best.base &&
        i < (best.base + best.len)) {
      if (i == best.base)
        *tp++ = ':';
      continue;
    }
    /* Are we following an initial run of 0x00s or any real hex? */
    if (i != 0)
      *tp++ = ':';
    /* Is this address an encapsulated IPv4? */
    if (i == 6 && best.base == 0 && (best.len == 6 ||
                                     (best.len == 7 && words[7] != 0x0001) ||
                                     (best.len == 5 && words[5] == 0xffff))) {
      int err = INetNToP4(src + 12, tp, sizeof tmp - (tp - tmp));
      if (err)
        return err;
      tp += strlen(tp);
      break;
    }
    tp += sprintf(tp, "%x", words[i]);  // NOLINT
  }
  /* Was it a trailing run of 0x00's? */
  if (best.base != -1 && (best.base + best.len) == arraysize(words))
    *tp++ = ':';
  *tp++ = '\0';

  /*
   * Check for overflow, copy, and we're done.
   */
  if ((size_t)(tp - tmp) > size) {
    return TIN_ENOSPC;
  }
  base::strlcpy(dst, tmp, kInetAddrStrLen6);
  return 0;
}

// const unsigned char* src, char* dst, size_t size
int InetNToP(int af, const void* src, char* dst, size_t size) {
  switch (af) {
  case AF_INET:
    return (INetNToP4(static_cast<const unsigned char*>(src), dst, size));
  case AF_INET6:
    return (INetNToP6(static_cast<const unsigned char*>(src), dst, size));
  default:
    return TIN_EAFNOSUPPORT;
  }
  /* NOTREACHED */
}


bool InetNToP(bool ipv4, const void* src, std::string* dst) {
  char tmp[kInetAddrStrLen6];
  int ret = InetNToP(ipv4 ? AF_INET : AF_INET6, src, tmp, kInetAddrStrLen6);
  if (ret == 0) {
    dst->resize(ipv4 ? kInetAddrStrLen : kInetAddrStrLen6);
    *dst = tmp;
  }
  return ret == 0;
}

static int INetPToN4(const char* src, unsigned char* dst) {
  static const char digits[] = "0123456789";
  int saw_digit, octets, ch;
  unsigned char tmp[sizeof(struct in_addr)], *tp;

  saw_digit = 0;
  octets = 0;
  *(tp = tmp) = 0;
  while ((ch = *src++) != '\0') {
    const char* pch;

    if ((pch = strchr(digits, ch)) != NULL) {
      unsigned int nw = static_cast<unsigned int>(*tp * 10 + (pch - digits));

      if (saw_digit && *tp == 0)
        return TIN_EINVAL;
      if (nw > 255)
        return TIN_EINVAL;
      *tp = nw;
      if (!saw_digit) {
        if (++octets > 4)
          return TIN_EINVAL;
        saw_digit = 1;
      }
    } else if (ch == '.' && saw_digit) {
      if (octets == 4)
        return TIN_EINVAL;
      *++tp = 0;
      saw_digit = 0;
    } else {
      return TIN_EINVAL;
    }
  }
  if (octets < 4)
    return TIN_EINVAL;
  memcpy(dst, tmp, sizeof(struct in_addr));
  return 0;
}

static int INetPToN6(const char* src, unsigned char* dst) {
  static const char xdigits_l[] = "0123456789abcdef",
                                  xdigits_u[] = "0123456789ABCDEF";
  unsigned char tmp[sizeof(Inet6Addr)], *tp, *endp, *colonp;
  const char* xdigits, *curtok;
  int ch, seen_xdigits;
  unsigned int val;

  memset((tp = tmp), '\0', sizeof tmp);
  endp = tp + sizeof tmp;
  colonp = NULL;
  /* Leading :: requires some special handling. */
  if (*src == ':')
    if (*++src != ':')
      return TIN_EINVAL;
  curtok = src;
  seen_xdigits = 0;
  val = 0;
  while ((ch = *src++) != '\0') {
    const char* pch;

    if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
      pch = strchr((xdigits = xdigits_u), ch);
    if (pch != NULL) {
      val <<= 4;
      val |= (pch - xdigits);
      if (++seen_xdigits > 4)
        return TIN_EINVAL;
      continue;
    }
    if (ch == ':') {
      curtok = src;
      if (!seen_xdigits) {
        if (colonp)
          return TIN_EINVAL;
        colonp = tp;
        continue;
      } else if (*src == '\0') {
        return TIN_EINVAL;
      }
      if (tp + sizeof(uint16) > endp)
        return EINVAL;
      *tp++ = (unsigned char) (val >> 8) & 0xff;
      *tp++ = (unsigned char) val & 0xff;
      seen_xdigits = 0;
      val = 0;
      continue;
    }
    if (ch == '.' && ((tp + sizeof(struct in_addr)) <= endp)) {
      int err = INetPToN4(curtok, tp);
      if (err == 0) {
        tp += sizeof(struct in_addr);
        seen_xdigits = 0;
        break;  /*%< '\\0' was seen by inet_pton4(). */
      }
    }
    return TIN_EINVAL;
  }
  if (seen_xdigits) {
    if (tp + sizeof(uint16) > endp)
      return EINVAL;
    *tp++ = (unsigned char) (val >> 8) & 0xff;
    *tp++ = (unsigned char) val & 0xff;
  }
  if (colonp != NULL) {
    /*
     * Since some memmove()'s erroneously fail to handle
     * overlapping regions, we'll do the shift by hand.
     */
    const int n = static_cast<const int>(tp - colonp);
    int i;

    if (tp == endp)
      return EINVAL;
    for (i = 1; i <= n; i++) {
      endp[- i] = colonp[n - i];
      colonp[n - i] = 0;
    }
    tp = endp;
  }
  if (tp != endp)
    return TIN_EINVAL;
  memcpy(dst, tmp, sizeof tmp);
  return 0;
}


int INetPToN(int af, const char* src, void* dst) {
  if (src == NULL || dst == NULL)
    return TIN_EINVAL;

  switch (af) {
  case AF_INET:
    return (INetPToN4(src, static_cast<unsigned char*>(dst)));
  case AF_INET6: {
    int len;
    char tmp[kInetAddrStrLen6], *s;
    const char* p;
    s = (char*) src;  // NOLINT
    p = strchr(src, '%');
    if (p != NULL) {
      s = tmp;
      len = static_cast<int>(p - src);
      if (len > kInetAddrStrLen6 - 1)
        return TIN_EINVAL;
      memcpy(s, src, len);
      s[len] = '\0';
    }
    return INetPToN6(s,  static_cast<unsigned char*>(dst));
  }
  default:
    return TIN_EAFNOSUPPORT;
  }
  /* NOTREACHED */
}


bool INetPToN(bool ipv4, const char* src, void* dst) {
  return INetPToN(ipv4 ? AF_INET : AF_INET6, src, dst) == 0;
}

}  // namespace net
}  // namespace tin
