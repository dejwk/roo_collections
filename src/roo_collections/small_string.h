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
/// The object occupies N bytes. The last byte stores the length, leaving
/// room for zero to N - 1 payload bytes. Embedded zero bytes are allowed.
/// Use view() to access the payload, which has no trailing terminator.
/// @tparam N Total inline storage size, from 1 to 256 bytes.
template <size_t N>
class SmallString {
 public:
  static_assert(N > 0, "SmallString capacity must be positive");

  static_assert(N <= 256, "SmallString capacity must not exceed 256 bytes");

  /// @brief Total inline storage capacity, including the length byte.
  static constexpr size_t kCapacity = N;

  /// @brief Creates an empty string.
  SmallString() { storage_[N - 1] = 0; }

  /// @brief Constructs from a C string.
  /// @param str Null-terminated source string.
  SmallString(const char* str) { assign(str, std::strlen(str)); }

  /// @brief Constructs from `std::string`, preserving embedded zero bytes.
  /// @param str Source string.
  SmallString(const std::string& str) { assign(str.data(), str.size()); }

  /// @brief Constructs from `roo::string_view`, preserving embedded zero bytes.
  /// @param str Source view.
  SmallString(const roo::string_view& str) { assign(str.data(), str.size()); }

  /// @brief Copy constructor.
  SmallString(const SmallString& other) { copyFrom(other); }

  /// @brief Copy assignment.
  SmallString& operator=(const SmallString& other) {
    if (this != &other) {
      copyFrom(other);
    }
    return *this;
  }

  /// @brief Assigns from another capacity, preserving embedded zero bytes.
  /// Copies the first N - 1 bytes if the source is longer.
  template <size_t M>
  SmallString& operator=(const SmallString<M>& other) {
    return *this = other.view();
  }

  /// @brief Assigns from C string.
  SmallString& operator=(const char* other) {
    assign(other, std::strlen(other));
    return *this;
  }

  /// @brief Assigns from `roo::string_view`, preserving embedded zero bytes.
  SmallString& operator=(roo::string_view other) {
    assign(other.data(), other.size());
    return *this;
  }

  /// @brief Returns the string length.
  /// @return Length in characters.
  size_t length() const { return storage_[N - 1]; }

  /// @brief Borrows the current payload, including any embedded zero bytes.
  /// The returned view is valid until the string is modified or destroyed.
  roo::string_view view() const {
    return roo::string_view(reinterpret_cast<const char*>(storage_), length());
  }

  /// @brief Checks whether the string is empty.
  /// @return `true` when empty.
  bool empty() const { return length() == 0; }

  /// @brief Equality comparison.
  bool operator==(const SmallString& other) const {
    return length() == other.length() &&
           std::memcmp(storage_, other.storage_, length()) == 0;
  }

  /// @brief Inequality comparison.
  bool operator!=(const SmallString& other) const { return !operator==(other); }

  /// @brief Implicit conversion to `roo::string_view`.
  operator roo::string_view() const { return view(); }

 private:
  // Copies the occupied payload and cached length.
  void copyFrom(const SmallString& other) {
    const unsigned char len = other.storage_[N - 1];
    std::memcpy(storage_, other.storage_, len);
    storage_[N - 1] = len;
  }

  // The source can be a view into our existing character buffer.
  void assign(const char* str, size_t len) {
    len = std::min(len, N - 1);
    if (len != 0) std::memmove(storage_, str, len);
    storage_[N - 1] = static_cast<unsigned char>(len);
  }

  unsigned char storage_[N];
};

}  // namespace roo_collections
