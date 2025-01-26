#pragma once

#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

template <typename Key, typename Value>
struct MapKeyFn {
  const Key& operator()(const std::pair<Key, Value>& entry) const {
    return entry.first;
  }
};

template <typename Key, typename Value, typename HashFn = std::hash<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
class FlatSmallHashMap
    : public FlatSmallHashtable<std::pair<Key, Value>, Key, HashFn, KeyCmpFn,
                                MapKeyFn<Key, Value>> {
 public:
  using value_type = std::pair<Key, Value>;
  using Base = FlatSmallHashtable<value_type, Key, HashFn, KeyCmpFn,
                                  MapKeyFn<Key, Value>>;

  FlatSmallHashMap(HashFn hash_fn = HashFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(hash_fn, key_cmp_fn) {}

  // Creates a hash map into which you can insert size_hint elements without
  // causing rehashing and memory reallocation.
  FlatSmallHashMap(uint16_t size_hint, HashFn hash_fn = HashFn(),
                   KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(size_hint, hash_fn, key_cmp_fn) {}

  template <typename InputIt>
  FlatSmallHashMap(InputIt first, InputIt last, HashFn hash_fn = HashFn(),
                   KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(first, last, hash_fn, key_cmp_fn) {}

  FlatSmallHashMap(std::initializer_list<value_type> init,
                   HashFn hash_fn = HashFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(init, hash_fn, key_cmp_fn) {}

  FlatSmallHashMap(const FlatSmallHashMap& other) : Base(other) {}

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
