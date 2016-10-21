// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_LIST_H_
#define NET_BASE_ADDRESS_LIST_H_


#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "tin/net/ip_endpoint.h"


struct addrinfo;

namespace tin {
namespace net {

class IPAddress;

class AddressList {
 public:
  AddressList();
  AddressList(const AddressList&);
  ~AddressList();

  // Creates an address list for a single IP literal.
  explicit AddressList(const IPEndPoint& endpoint);

  static AddressList CreateFromIPAddress(const IPAddress& address,
                                         uint16 port);

  static AddressList CreateFromIPAddressList(const IPAddressList& addresses,
      const std::string& canonical_name);

  // Copies the data from |head| and the chained list into an AddressList.
  static AddressList CreateFromAddrinfo(const struct addrinfo* head);

  // Returns a copy of |list| with port on each element set to |port|.
  static AddressList CopyWithPort(const AddressList& list, uint16 port);

  const std::string& canonical_name() const {
    return canonical_name_;
  }

  void set_canonical_name(const std::string& canonical_name) {
    canonical_name_ = canonical_name;
  }

  // Sets canonical name to the literal of the first IP address on the list.
  void SetDefaultCanonicalName();

  // Creates a callback for use with the NetLog that returns a Value
  // representation of the address list.  The callback must be destroyed before
  // |this| is.

  typedef std::vector<IPEndPoint>::iterator iterator;
  typedef std::vector<IPEndPoint>::const_iterator const_iterator;

  size_t size() const {
    return endpoints_.size();
  }
  bool empty() const {
    return endpoints_.empty();
  }
  void clear() {
    endpoints_.clear();
  }
  void reserve(size_t count) {
    endpoints_.reserve(count);
  }
  size_t capacity() const {
    return endpoints_.capacity();
  }
  IPEndPoint& operator[](size_t index) {
    return endpoints_[index];
  }
  const IPEndPoint& operator[](size_t index) const {
    return endpoints_[index];
  }
  IPEndPoint& front() {
    return endpoints_.front();
  }
  const IPEndPoint& front() const {
    return endpoints_.front();
  }
  IPEndPoint& back() {
    return endpoints_.back();
  }
  const IPEndPoint& back() const {
    return endpoints_.back();
  }
  void push_back(const IPEndPoint& val) {
    endpoints_.push_back(val);
  }

  template <typename InputIt>
  void insert(iterator pos, InputIt first, InputIt last) {
    endpoints_.insert(pos, first, last);
  }
  iterator begin() {
    return endpoints_.begin();
  }
  const_iterator begin() const {
    return endpoints_.begin();
  }
  iterator end() {
    return endpoints_.end();
  }
  const_iterator end() const {
    return endpoints_.end();
  }

 private:
  std::vector<IPEndPoint> endpoints_;
  std::string canonical_name_;
};

}  // namespace net
}  // namespace tin

#endif  // NET_BASE_ADDRESS_LIST_H_
