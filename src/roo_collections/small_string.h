#pragma once

/// @file
/// @brief Fixed-capacity string utility type.
/// @ingroup roo_collections

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "roo_backport.h"
#include "roo_backport/string_view.h"
#include "roo_collections/hash.h"

namespace roo_collections {

/// @brief Fixed-capacity string stored inline (no heap allocation).
///
/// Intended for short, bounded identifiers and other small keys/values where a
/// constant memory footprint is preferred over unbounded growth.
///
/// @tparam N Capacity of the internal storage buffer, in bytes.
template <size_t N>
class SmallString {
 public:
  /// @brief Maximum storage capacity (including the trailing '\0').
  static constexpr size_t kCapacity = N;

  /// @brief Creates an empty string.
  SmallString() { data_[0] = 0; }

  /// @brief Constructs from a C string.
  /// @param str Null-terminated source string.
  SmallString(const char* str) { strncpy(data_, str, N); }

  /// @brief Constructs from `std::string`.
  /// @param str Source string.
  SmallString(const std::string& str) { strncpy(data_, str.c_str(), N); }

  /// @brief Constructs from `roo::string_view`.
  /// @param str Source view.
  SmallString(const roo::string_view& str) {
    strncpy(data_, str.data(), std::min(N, str.size() + 1));
  }

  /// @brief Copy constructor.
  SmallString(const SmallString& other) { strncpy(data_, other.data_, N); }

  /// @brief Copy assignment.
  SmallString& operator=(const SmallString& other) {
    strncpy(data_, other.data_, N);
    return *this;
  }

  /// @brief Assigns from C string.
  SmallString& operator=(const char* other) {
    strncpy(data_, other, N);
    return *this;
  }

  /// @brief Assigns from `roo::string_view`.
  SmallString& operator=(roo::string_view other) {
    strncpy(data_, other.data(), std::min(N, other.size() + 1));
    return *this;
  }

  /// @brief Returns the string length.
  /// @return Length in characters.
  size_t length() const { return strlen(data_); }

  /// @brief Returns pointer to null-terminated character data.
  const char* c_str() const { return data_; }

  /// @brief Checks whether the string is empty.
  /// @return `true` when empty.
  bool empty() const { return data_[0] == 0; }

  /// @brief Equality comparison.
  bool operator==(const SmallString& other) const {
    return strcmp(data_, other.data_) == 0;
  }

  /// @brief Inequality comparison.
  bool operator!=(const SmallString& other) const { return !operator==(other); }

  /// @brief Implicit conversion to `roo::string_view`.
  operator roo::string_view() const {
    return roo::string_view(data_, length());
  }

 private:
  char data_[N];
};

}  // namespace roo_collections
