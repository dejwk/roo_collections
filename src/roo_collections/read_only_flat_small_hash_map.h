#pragma once

/// @file
/// @brief Common read-only flat-container interface.
/// @ingroup roo_collections

#include "roo_collections/read_only_flat_small_hashtable.h"

namespace roo_collections {

/// @brief Common const interface for maps with matching key/value/policy types.
/// Borrows the live owner; references survive rehashing, while entry iterators
/// and references follow the owner's invalidation rules. This alias exposes
/// const at()/operator[] through its statically selected map-access mixin.
template <typename Key, typename Value, typename HashFn = DefaultHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
using ReadOnlyFlatSmallHashMap =
    ReadOnlyFlatSmallHashtable<std::pair<Key, Value>, Key, HashFn,
                               MapKeyFn<Key, Value>, KeyCmpFn>;

}  // namespace roo_collections
