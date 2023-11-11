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
  using value_type = std::pair<Key, Value>;

  FlatSmallHashMap(HashFn hash_fn = HashFn())
      : FlatSmallHashtable<value_type, Key, HashFn, MapKeyFn<Key, Value>>(
            hash_fn) {}

  FlatSmallHashMap(uint16_t size_hint, HashFn hash_fn = HashFn())
      : FlatSmallHashtable<value_type, Key, HashFn, MapKeyFn<Key, Value>>(
            size_hint, hash_fn) {}

  template <typename InputIt>
  FlatSmallHashMap(InputIt first, InputIt last, HashFn hash_fn = HashFn())
      : FlatSmallHashtable<value_type, Key, HashFn, MapKeyFn<Key, Value>>(
            first, last, hash_fn) {}

  FlatSmallHashMap(std::initializer_list<value_type> init,
                   HashFn hash_fn = HashFn())
      : FlatSmallHashtable<value_type, Key, HashFn, MapKeyFn<Key, Value>>(
            init, hash_fn) {}

  FlatSmallHashMap(const FlatSmallHashMap& other)
      : FlatSmallHashtable<value_type, Key, HashFn, MapKeyFn<Key, Value>>(
            other) {}

  const Value& at(const Key& key) const {
    auto it = this->find(key);
    assert(it != this->end());
    return (*it).second;
  }

  Value& at(const Key& key) {
    auto it = this->lookup(key);
    assert(it != this->end());
    return (*it).second;
  }

  const Value& operator[](const Key& key) const { return at(key); }

  Value& operator[](const Key& key) {
    auto it = this->lookup(key);
    if (it == this->end()) {
      return (*this->insert({key, Value()}).first).second;
    } else {
      return (*it).second;
    }
  }
};

}  // namespace roo_collections
