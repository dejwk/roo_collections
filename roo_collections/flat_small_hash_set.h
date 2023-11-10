#pragma once

#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

template <typename Key, typename HashFn = DefaultHashFn>
using FlatSmallHashSet = FlatSmallHashtable<Key, Key, HashFn, DefaultKeyFn>;

}  // namespace roo_collections
