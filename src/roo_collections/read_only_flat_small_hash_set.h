#pragma once

/// @file
/// @brief Common read-only flat-container interface.
/// @ingroup roo_collections

#include "roo_collections/read_only_flat_small_hashtable.h"

namespace roo_collections {

/// @brief Common const membership/iteration interface for matching set types.
/// Borrows the live owner; references survive rehashing, while entry iterators
/// and references follow the owner's invalidation rules.
template <typename Key, typename HashFn = DefaultHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
using ReadOnlyFlatSmallHashSet =
    ReadOnlyFlatSmallHashtable<Key, Key, HashFn, DefaultKeyFn<Key>, KeyCmpFn>;

}  // namespace roo_collections
