#pragma once

/// @file
/// @brief Immutable flat maps with inline storage and constexpr construction.
/// @ingroup roo_collections

#if __cplusplus >= 201703L

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
  using Data = typename Base::Data;

  /// Directly constructs the map from pointer-free prepared data.
  explicit constexpr ImmutableFlatSmallHashMap(const Data& data) : Base(data) {}

  /// Consumes prepared data, avoiding additional copies of movable entries.
  explicit constexpr ImmutableFlatSmallHashMap(Data&& data)
      : Base(std::move(data)) {}

  /// Copies inline data and rebinds the common base to this map's arrays.
  constexpr ImmutableFlatSmallHashMap(const ImmutableFlatSmallHashMap&) =
      default;

  /// Preserves independent inline storage in both maps.
  constexpr ImmutableFlatSmallHashMap(ImmutableFlatSmallHashMap&&) = default;

  /// Assigns data while keeping references to this map's base attached.
  constexpr ImmutableFlatSmallHashMap& operator=(
      const ImmutableFlatSmallHashMap&) = default;

  /// Assigns independent inline data from another map.
  constexpr ImmutableFlatSmallHashMap& operator=(ImmutableFlatSmallHashMap&&) =
      default;

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

/// Deduces map key, value, count and policy types from prepared data.
template <typename Key, typename Value, size_t N, typename HashFn,
          typename KeyCmpFn>
ImmutableFlatSmallHashMap(
    const impl::ImmutableFlatSmallHashtableData<
        std::pair<Key, Value>, Key, N, HashFn, MapKeyFn<Key, Value>, KeyCmpFn>&)
    -> ImmutableFlatSmallHashMap<Key, Value, N, HashFn, KeyCmpFn>;

/// Prepares @p entries for direct C++17 constexpr map construction.
/// Returns pointer-free data; construct ImmutableFlatSmallHashMap from it
/// to deduce the entry count and bind the owner's inline storage.
/// Duplicate keys fail constant evaluation or abort runtime construction.
template <typename Key, typename Value, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto WithMapping(const std::pair<Key, Value> (&entries)[N],
                           HashFn hash_fn = HashFn(),
                           KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return WithEntries<std::pair<Key, Value>, Key>(
      entries, std::move_if_noexcept(hash_fn), MapKeyFn<Key, Value>(),
      std::move_if_noexcept(key_cmp_fn));
}

/// Builds pointer-free map data from std::array, including zero pairs.
template <typename Key, typename Value, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto WithMapping(const std::array<std::pair<Key, Value>, N>& entries,
                           HashFn hash_fn = HashFn(),
                           KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return WithEntries<std::pair<Key, Value>, Key>(
      entries, std::move_if_noexcept(hash_fn), MapKeyFn<Key, Value>(),
      std::move_if_noexcept(key_cmp_fn));
}

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

/// Prepares @p entries for a map with transparent string-view lookup.
/// Returns pointer-free data for direct ImmutableFlatSmallHashMap construction.
/// Stored views borrow character data that must outlive the owner.
template <typename Value, size_t N>
constexpr auto WithStringMapping(
    const std::pair<::roo::string_view, Value> (&entries)[N]) {
  return WithMapping<::roo::string_view, Value>(
      entries, TransparentStringHashFn(), TransparentEq());
}

/// Builds pointer-free string-view map data from std::array, including empty.
template <typename Value, size_t N>
constexpr auto WithStringMapping(
    const std::array<std::pair<::roo::string_view, Value>, N>& entries) {
  return WithMapping<::roo::string_view, Value>(
      entries, TransparentStringHashFn(), TransparentEq());
}

}  // namespace roo_collections

#else

#warning \
    "roo_collections: ImmutableFlatSmallHashMap requires C++17 or later; declarations disabled"

#endif  // __cplusplus >= 201703L
