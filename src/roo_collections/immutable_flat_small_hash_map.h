#pragma once

/// @file
/// @brief Immutable flat maps with inline storage and constexpr construction.
/// @ingroup roo_collections

#include "roo_collections/flat_small_hash_map.h"
#include "roo_collections/immutable_flat_small_hashtable.h"

namespace roo_collections {

/// @brief Read-only key/value map stored entirely in inline arrays.
///
/// Supports C++17 constant evaluation with suitable keys, values and functors.
/// Entries must be default constructible and copy assignable. Borrowed
/// key/value data must outlive the map. Duplicate keys fail constant evaluation
/// or abort runtime construction. All access returns const entries or mapped
/// values.
///
/// @tparam Key Stored key type.
/// @tparam Value Mapped value type.
/// @tparam N Number of distinct entries, at most 64000.
/// @tparam HashFn Key hash function, identical at compile time and runtime.
/// @tparam KeyCmpFn Key equality predicate, consistent with HashFn.
template <typename Key, typename Value, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
class ImmutableFlatSmallHashMap
    : public ImmutableFlatSmallHashtable<std::pair<Key, Value>, Key, N, HashFn,
                                         MapKeyFn<Key, Value>, KeyCmpFn> {
 public:
  using mapped_type = Value;
  using Base =
      ImmutableFlatSmallHashtable<std::pair<Key, Value>, Key, N, HashFn,
                                  MapKeyFn<Key, Value>, KeyCmpFn>;
  using value_type = typename Base::value_type;

  /// Creates an empty map; requires N to be zero.
  constexpr ImmutableFlatSmallHashMap(HashFn hash_fn = HashFn(),
                                      KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  /// Creates a map by copying N pairs from @p entries.
  /// Duplicate keys fail constant evaluation or abort runtime construction.
  template <size_t M>
  constexpr ImmutableFlatSmallHashMap(const value_type (&entries)[M],
                                      HashFn hash_fn = HashFn(),
                                      KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(entries, hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  /// Creates a map from @p entries, including an empty std::array.
  /// Duplicate keys fail constant evaluation or abort runtime construction.
  constexpr ImmutableFlatSmallHashMap(const std::array<value_type, N>& entries,
                                      HashFn hash_fn = HashFn(),
                                      KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(entries, hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  /// Returns the mapped value for @p key, which must be present.
  /// Asserts in debug builds if the key is absent.
  constexpr const Value& at(const Key& key) const {
    typename Base::const_iterator it = this->find(key);
    assert(it != this->end());
    return it->second;
  }

  /// Returns a mapped value using transparent heterogeneous lookup.
  /// The key must be present; asserts in debug builds if absent.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  constexpr const Value& at(const K& key) const {
    typename Base::const_iterator it = this->find(key);
    assert(it != this->end());
    return it->second;
  }

  /// Returns a const mapped value for an existing key; never inserts.
  constexpr const Value& operator[](const Key& key) const { return at(key); }

  /// Returns a const mapped value using transparent lookup; never inserts.
  /// The key must be present.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  constexpr const Value& operator[](const K& key) const {
    return at(key);
  }
};

/// Builds an immutable map, deducing its count from an array of pairs.
/// Duplicate keys fail constant evaluation or abort runtime construction.
template <typename Key, typename Value, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto MakeImmutableFlatSmallHashMap(
    const std::pair<Key, Value> (&entries)[N], HashFn hash_fn = HashFn(),
    KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return ImmutableFlatSmallHashMap<Key, Value, N, HashFn, KeyCmpFn>(
      entries, hash_fn, key_cmp_fn);
}

/// Builds an immutable map from std::array, including zero pairs.
/// Duplicate keys fail constant evaluation or abort runtime construction.
template <typename Key, typename Value, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto MakeImmutableFlatSmallHashMap(
    const std::array<std::pair<Key, Value>, N>& entries,
    HashFn hash_fn = HashFn(), KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return ImmutableFlatSmallHashMap<Key, Value, N, HashFn, KeyCmpFn>(
      entries, hash_fn, key_cmp_fn);
}

/// String-view-keyed immutable map accepting heterogeneous string lookups.
/// Stored views borrow character data that must outlive the map.
template <typename Value, size_t N>
using ImmutableFlatSmallStringHashMap =
    ImmutableFlatSmallHashMap<::roo::string_view, Value, N,
                              TransparentStringHashFn, TransparentEq>;

/// Builds a string-view-keyed immutable map from N pairs.
/// Stored views borrow character data; duplicates are rejected.
template <typename Value, size_t N>
constexpr auto MakeImmutableFlatSmallStringHashMap(
    const std::pair<::roo::string_view, Value> (&entries)[N]) {
  return ImmutableFlatSmallStringHashMap<Value, N>(entries);
}

/// Builds a string-view-keyed immutable map from std::array, including empty.
/// Stored views borrow character data; duplicates are rejected.
template <typename Value, size_t N>
constexpr auto MakeImmutableFlatSmallStringHashMap(
    const std::array<std::pair<::roo::string_view, Value>, N>& entries) {
  return ImmutableFlatSmallStringHashMap<Value, N>(entries);
}

}  // namespace roo_collections
