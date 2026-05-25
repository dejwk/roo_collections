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
  static_assert(N > 0, "SmallString capacity must be positive");

  /// @brief Maximum storage capacity (including the trailing '\0').
  static constexpr size_t kCapacity = N;

  /// @brief Creates an empty string.
  SmallString() { data_[0] = 0; }

  /// @brief Constructs from a C string.
  /// @param str Null-terminated source string.
  SmallString(const char* str) { assign(str, strlen(str)); }

  /// @brief Constructs from `std::string`.
  /// @param str Source string.
  SmallString(const std::string& str) { assign(str.data(), str.size()); }

  /// @brief Constructs from `roo::string_view`.
  /// @param str Source view.
  SmallString(const roo::string_view& str) { assign(str.data(), str.size()); }

  /// @brief Copy constructor.
  SmallString(const SmallString& other) { assign(other.data_, other.length()); }

  /// @brief Copy assignment.
  SmallString& operator=(const SmallString& other) {
    assign(other.data_, other.length());
    return *this;
  }

  /// @brief Assigns from C string.
  SmallString& operator=(const char* other) {
    assign(other, strlen(other));
    return *this;
  }

  /// @brief Assigns from `roo::string_view`.
  SmallString& operator=(roo::string_view other) {
    assign(other.data(), other.size());
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
  void assign(const char* str, size_t len) {
    len = std::min(len, N - 1);
    memcpy(data_, str, len);
    data_[len] = 0;
  }

  char data_[N];
};

}  // namespace roo_collections
