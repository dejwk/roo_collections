#pragma once

#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

template <typename Key, typename Value>
struct MapKeyFn {
  const Key& operator()(const std::pair<Key, Value>& entry) const {
    return entry.first;
  }
};

template <typename Key, typename Value, typename HashFn = DefaultHashFn<Key>>
class FlatSmallHashMap
    : public FlatSmallHashtable<std::pair<Key, Value>, Key, HashFn,
                                MapKeyFn<Key, Value>> {
 public:
  class Ref {
   public:
    operator const Value&() const { return (*target_.find(key_)).second; }

    const Value& operator=(Value val) {
      auto itr = target_.lookup(key_);
      if (itr == target_.end()) {
        return (*target_.insert(std::make_pair(key_, std::move(val))).first)
            .second;
      } else {
        (*itr).second = std::move(val);
        return (*itr).second;
      }
    }

   private:
    friend class FlatSmallHashMap<Key, Value, HashFn>;

    Ref(FlatSmallHashMap<Key, Value, HashFn>& target, Key key)
        : target_(target), key_(std::move(key)) {}
    FlatSmallHashMap<Key, Value, HashFn>& target_;
    Key key_;
  };

  const Value& operator[](const Key& key) const {
    auto it = find(key);
    assert(it.second);
    return it.first;
  }

  Ref operator[](const Key& key) { return Ref(*this, key); }
};

}  // namespace roo_collections
