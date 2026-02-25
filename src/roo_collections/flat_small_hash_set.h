#pragma once

/// @file
/// @brief Flat, memory-conscious hash set aliases.
/// @ingroup roo_collections

#include <functional>

#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

/// @brief Flat, memory-conscious hash set optimized for small collections.
///
/// Stores keys in a single backing array and provides average constant-time
/// lookup/insert/erase.
///
/// @tparam Key Stored key type.
/// @tparam HashFn Hash function type.
/// @tparam KeyCmpFn Equality predicate type.
template <typename Key, typename HashFn = DefaultHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
using FlatSmallHashSet =
    FlatSmallHashtable<Key, Key, HashFn, DefaultKeyFn<Key>, KeyCmpFn>;

/// @brief String-specialized flat hash set with heterogeneous lookup support.
///
/// Accepts `std::string`, `const char*`, `roo::string_view`, and Arduino
/// `String` (when available) for lookup operations.
using FlatSmallStringHashSet =
    FlatSmallHashtable<std::string, std::string, TransparentStringHashFn,
                       TransparentEq, DefaultKeyFn<std::string>>;

}  // namespace roo_collections
