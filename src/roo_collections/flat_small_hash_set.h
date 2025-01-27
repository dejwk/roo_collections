#pragma once

#include <functional>

#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

template <typename Key, typename HashFn = DefaultHashFn<Key>>
using FlatSmallHashSet =
    FlatSmallHashtable<Key, Key, HashFn, DefaultKeyFn<Key>>;

// Convenience specialization for strings, that can accept std::string, const
// char*, string_view, and Arduino String in the lookup functions.
using FlatSmallStringHashSet =
    FlatSmallHashtable<std::string, std::string, TransparentStringHashFn,
                       TransparentEq, DefaultKeyFn<std::string>>;

}  // namespace roo_collections
