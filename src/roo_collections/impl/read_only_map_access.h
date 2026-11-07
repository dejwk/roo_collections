#pragma once

/// @file
/// @brief Adds const mapped-value access to map-shaped read-only tables.

#include "roo_collections/impl/flat_small_hashtable_common.h"

namespace roo_collections {
namespace impl {

/// Leaves non-map tables without mapped-value accessors.
template <typename Table, typename KeyFn, typename HashFn, typename KeyCmpFn>
class ReadOnlyMapAccess {};

/// Supplies map accessors without adding state or runtime dispatch.
template <typename Table, typename Key, typename Value, typename HashFn,
          typename KeyCmpFn>
class ReadOnlyMapAccess<Table, MapKeyFn<Key, Value>, HashFn, KeyCmpFn> {
 public:
  using mapped_type = Value;

  /// Returns the mapped value for an existing key; asserts in debug builds
  /// when absent. The returned reference borrows the backing table's storage.
  ROO_COLLECTIONS_CONSTEXPR14 const Value& at(const Key& key) const {
    return valueFor(key);
  }

  /// Returns an existing mapped value using transparent heterogeneous lookup.
  /// Asserts in debug builds when the key is absent.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  ROO_COLLECTIONS_CONSTEXPR14 const Value& at(const K& key) const {
    return valueFor(key);
  }

  /// Returns an existing mapped value; never inserts a missing key.
  ROO_COLLECTIONS_CONSTEXPR14 const Value& operator[](const Key& key) const {
    return at(key);
  }

  /// Returns an existing mapped value using transparent lookup; never inserts.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  ROO_COLLECTIONS_CONSTEXPR14 const Value& operator[](const K& key) const {
    return at(key);
  }

 private:
  /// Resolves a key through the table's statically bound const lookup.
  template <typename K>
  ROO_COLLECTIONS_CONSTEXPR14 const Value& valueFor(const K& key) const {
    const Table& table = static_cast<const Table&>(*this);
    typename Table::ConstIterator it = table.find(key);
    assert(it != table.end());
    return it->second;
  }
};

}  // namespace impl
}  // namespace roo_collections
