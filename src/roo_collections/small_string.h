#pragma once

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "roo_backport/string_view.h"
#include "roo_collections/hash.h"

namespace roo_collections {

// String with statically limited capacity and a constant footprint. Intended
// for small, bounded-length identifiers. Can be used as a map key. Does not
// allocate any storage on the heap. Implemented as a thin wrapper over a static
// char array.
template <size_t N>
class SmallString {
 public:
  static constexpr size_t kCapacity = N;

  SmallString() { data_[0] = 0; }

  SmallString(const char* str) { strncpy(data_, str, N); }
  SmallString(const std::string& str) { strncpy(data_, str.c_str(), N); }

  SmallString(const roo::string_view& str) {
    strncpy(data_, str.data(), std::min(N, str.size() + 1));
  }

  SmallString(const SmallString& other) { strncpy(data_, other.data_, N); }

  SmallString& operator=(const SmallString& other) {
    strncpy(data_, other.data_, N);
    return *this;
  }

  SmallString& operator=(const char* other) {
    strncpy(data_, other, N);
    return *this;
  }

  SmallString& operator=(roo::string_view other) {
    strncpy(data_, other.data(), std::min(N, other.size() + 1));
    return *this;
  }

  size_t length() const { return strlen(data_); }

  const char* c_str() const { return data_; }
  bool empty() const { return data_[0] == 0; }

  bool operator==(const SmallString& other) const {
    return strcmp(data_, other.data_) == 0;
  }

  bool operator!=(const SmallString& other) const { return !operator==(other); }

  operator roo::string_view() const {
    return roo::string_view(data_, length());
  }

 private:
  char data_[N];
};

}  // namespace roo_collections
