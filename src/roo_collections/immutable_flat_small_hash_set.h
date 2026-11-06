#pragma once

/// @file
/// @brief Immutable flat sets with inline storage and constexpr construction.
/// @ingroup roo_collections

#if __cplusplus >= 201703L

#include "roo_collections/immutable_flat_small_hashtable.h"

namespace roo_collections {

/// @brief Read-only set of N distinct keys stored in inline arrays.
///
/// Supports C++17 constant evaluation with suitable keys and functors. Keys
/// must be default constructible and copy assignable. Duplicate keys fail
/// constant evaluation or abort runtime construction. Borrowed data must
/// outlive the set.
///
/// @tparam Key Stored key type.
/// @tparam N Number of distinct keys, at most 64000.
/// @tparam HashFn Key hash function, identical at compile time and runtime.
/// @tparam KeyCmpFn Key equality predicate, consistent with HashFn.
template <typename Key, size_t N, typename HashFn = ImmutableHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
using ImmutableFlatSmallHashSet =
    ImmutableFlatSmallHashtable<Key, Key, N, HashFn, DefaultKeyFn<Key>,
                                KeyCmpFn>;

/// Builds an immutable set, deducing its count from an array of keys.
/// Duplicate keys fail constant evaluation or abort runtime construction.
template <typename Key, size_t N, typename HashFn = ImmutableHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto MakeImmutableFlatSmallHashSet(const Key (&keys)[N],
                                             HashFn hash_fn = HashFn(),
                                             KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return ImmutableFlatSmallHashSet<Key, N, HashFn, KeyCmpFn>(
      keys, hash_fn, DefaultKeyFn<Key>(), key_cmp_fn);
}

/// Builds an immutable set from std::array, including zero keys.
/// Duplicate keys fail constant evaluation or abort runtime construction.
template <typename Key, size_t N, typename HashFn = ImmutableHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto MakeImmutableFlatSmallHashSet(const std::array<Key, N>& keys,
                                             HashFn hash_fn = HashFn(),
                                             KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return ImmutableFlatSmallHashSet<Key, N, HashFn, KeyCmpFn>(
      keys, hash_fn, DefaultKeyFn<Key>(), key_cmp_fn);
}

/// String-view immutable set accepting heterogeneous string lookups.
/// Stored views borrow character data that must outlive the set.
template <size_t N>
using ImmutableFlatSmallStringHashSet =
    ImmutableFlatSmallHashSet<::roo::string_view, N, TransparentStringHashFn,
                              TransparentEq>;

/// Builds an immutable string-view set from N keys.
/// Stored views borrow character data; duplicates are rejected.
template <size_t N>
constexpr auto MakeImmutableFlatSmallStringHashSet(
    const ::roo::string_view (&keys)[N]) {
  return ImmutableFlatSmallStringHashSet<N>(keys);
}

/// Builds an immutable string-view set from std::array, including empty.
/// Stored views borrow character data; duplicates are rejected.
template <size_t N>
constexpr auto MakeImmutableFlatSmallStringHashSet(
    const std::array<::roo::string_view, N>& keys) {
  return ImmutableFlatSmallStringHashSet<N>(keys);
}

}  // namespace roo_collections

#else

#warning \
    "roo_collections: ImmutableFlatSmallHashSet requires C++17 or later; declarations disabled"

#endif  // __cplusplus >= 201703L
