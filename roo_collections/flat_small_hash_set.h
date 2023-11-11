#pragma once

#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

template <typename Key, typename HashFn = DefaultHashFn<Key>>
using FlatSmallHashSet =
    FlatSmallHashtable<Key, Key, HashFn, DefaultKeyFn<Key>>;

}  // namespace roo_collections
