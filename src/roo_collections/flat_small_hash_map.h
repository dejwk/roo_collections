#pragma once

#include <functional>

#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

template <typename Key, typename Value>
struct MapKeyFn {
  const Key& operator()(const std::pair<Key, Value>& entry) const {
    return entry.first;
  }
};

template <typename Key, typename Value, typename HashFn = DefaultHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
class FlatSmallHashMap
    : public FlatSmallHashtable<std::pair<Key, Value>, Key, HashFn,
                                MapKeyFn<Key, Value>, KeyCmpFn> {
 public:
  using value_type = std::pair<Key, Value>;
  using Base = FlatSmallHashtable<value_type, Key, HashFn, MapKeyFn<Key, Value>,
                                  KeyCmpFn>;

  FlatSmallHashMap(HashFn hash_fn = HashFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  // Creates a hash map into which you can insert size_hint elements without
  // causing rehashing and memory reallocation.
  FlatSmallHashMap(uint16_t size_hint, HashFn hash_fn = HashFn(),
                   KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(size_hint, hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  template <typename InputIt>
  FlatSmallHashMap(InputIt first, InputIt last, HashFn hash_fn = HashFn(),
                   KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(first, last, hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  FlatSmallHashMap(std::initializer_list<value_type> init,
                   HashFn hash_fn = HashFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(init, hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  FlatSmallHashMap(const FlatSmallHashMap& other) : Base(other) {}

  const Value& at(const Key& key) const {
    auto it = this->find(key);
    assert(it != this->end());
    return (*it).second;
  }

  template <typename K,
            typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  const Value& at(const K& key) const {
    auto it = this->find(key);
    assert(it != this->end());
    return (*it).second;
  }

  Value& at(const Key& key) {
    auto it = this->lookup(key);
    assert(it != this->end());
    return (*it).second;
  }

  template <typename K,
            typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  Value& at(const K& key) {
    auto it = this->lookup(key);
    assert(it != this->end());
    return (*it).second;
  }

  const Value& operator[](const Key& key) const { return at(key); }

  template <typename K,
            typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  const Value& operator[](const K& key) const {
    return at(key);
  }

  Value& operator[](const Key& key) {
    auto it = this->lookup(key);
    if (it == this->end()) {
      return (*this->insert({key, Value()}).first).second;
    } else {
      return (*it).second;
    }
  }

  template <typename K,
            typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  Value& operator[](const K& key) {
    auto it = this->lookup(key);
    if (it == this->end()) {
      return (*this->insert({Key(key), Value()}).first).second;
    } else {
      return (*it).second;
    }
  }
};

// Convenience specialization for strings, that can accept std::string, const
// char*, string_view, and Arduino String in the lookup functions.
template <typename Value>
using FlatSmallStringHashMap =
    FlatSmallHashMap<std::string, Value, TransparentStringHashFn, std::equal_to<void>>;

}  // namespace roo_collections
