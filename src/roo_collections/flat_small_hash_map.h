#pragma once

/// @file
/// @brief Flat, memory-conscious hash map built on FlatSmallHashtable.
/// @ingroup roo_collections

#include <functional>

#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

template <typename Key, typename Value>
struct MapKeyFn {
  const Key& operator()(const std::pair<Key, Value>& entry) const {
    return entry.first;
  }
};

template <typename Key, typename T>
struct KeyCovert {
  Key operator()(const T& val) const { return Key(val); }
};

template <>
struct KeyCovert<std::string, ::roo::string_view> {
  std::string operator()(const ::roo::string_view& val) const {
    return std::string(val.data(), val.size());
  }
};

/// @brief Flat, memory-conscious hash map optimized for small collections.
///
/// Uses `FlatSmallHashtable` as the underlying storage and provides a map-like
/// interface over key-value pairs.
///
/// @tparam Key Key type.
/// @tparam Value Mapped value type.
/// @tparam HashFn Hash function type.
/// @tparam KeyCmpFn Key equality predicate type.
template <typename Key, typename Value, typename HashFn = DefaultHashFn<Key>,
          typename KeyCmpFn = std::equal_to<Key>>
class FlatSmallHashMap
    : public FlatSmallHashtable<std::pair<Key, Value>, Key, HashFn,
                                MapKeyFn<Key, Value>, KeyCmpFn> {
 public:
  using mapped_type = Value;

  using Base = FlatSmallHashtable<std::pair<Key, Value>, Key, HashFn,
                                  MapKeyFn<Key, Value>, KeyCmpFn>;

  using key_type = typename Base::key_type;
  using value_type = typename Base::value_type;
  using hasher = typename Base::hasher;
  using key_equal = typename Base::key_equal;
  using iterator = typename Base::iterator;
  using const_iterator = typename Base::const_iterator;

  /// @brief Creates an empty hash map.
  FlatSmallHashMap(HashFn hash_fn = HashFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  /// @brief Creates a hash map with capacity for approximately `size_hint`
  /// elements without rehashing.
  /// @param size_hint Expected number of inserted items.
  FlatSmallHashMap(uint16_t size_hint, HashFn hash_fn = HashFn(),
                   KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(size_hint, hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  /// @brief Builds a map from an iterator range.
  template <typename InputIt>
  FlatSmallHashMap(InputIt first, InputIt last, HashFn hash_fn = HashFn(),
                   KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(first, last, hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  /// @brief Builds a map from an initializer list.
  FlatSmallHashMap(std::initializer_list<value_type> init,
                   HashFn hash_fn = HashFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : Base(init, hash_fn, MapKeyFn<Key, Value>(), key_cmp_fn) {}

  /// @brief Copy constructor.
  FlatSmallHashMap(const FlatSmallHashMap& other) : Base(other) {}

  /// @brief Returns a const reference to the mapped value for `key`.
  ///
  /// Asserts in debug builds if `key` is not present.
  const Value& at(const Key& key) const {
    auto it = this->find(key);
    assert(it != this->end());
    return (*it).second;
  }

  /// @brief Returns a const reference to the mapped value for `key`.
  ///
  /// Heterogeneous overload enabled for transparent hash/equality functions.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  const Value& at(const K& key) const {
    auto it = this->find(key);
    assert(it != this->end());
    return (*it).second;
  }

  /// @brief Returns a mutable reference to the mapped value for `key`.
  ///
  /// Asserts in debug builds if `key` is not present.
  Value& at(const Key& key) {
    auto it = this->lookup(key);
    assert(it != this->end());
    return (*it).second;
  }

  /// @brief Returns a mutable reference to the mapped value for `key`.
  ///
  /// Heterogeneous overload enabled for transparent hash/equality functions.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  Value& at(const K& key) {
    auto it = this->lookup(key);
    assert(it != this->end());
    return (*it).second;
  }

  /// @brief Finds `key` and returns an iterator to the entry, or `end()`.
  typename Base::Iterator find(const Key& key) { return Base::lookup(key); }

  /// @brief Heterogeneous lookup overload of `find`.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  typename Base::Iterator find(const K& key) {
    return Base::lookup(key);
  }

  /// @brief Finds `key` and returns a const iterator to the entry, or
  /// `end()`.
  typename Base::ConstIterator find(const Key& key) const {
    return Base::find(key);
  }

  /// @brief Heterogeneous const lookup overload of `find`.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  typename Base::ConstIterator find(const K& key) const {
    return Base::find(key);
  }

  /// @brief Returns a const reference to the value for `key`.
  const Value& operator[](const Key& key) const { return at(key); }

  /// @brief Heterogeneous const lookup overload of `operator[]`.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  const Value& operator[](const K& key) const {
    return at(key);
  }

  /// @brief Returns a mutable reference to the value for `key`.
  ///
  /// Inserts a default-constructed value when `key` is not present.
  Value& operator[](const Key& key) {
    auto it = this->lookup(key);
    if (it == this->end()) {
      return (*this->insert(std::make_pair(key, Value())).first).second;
    } else {
      return (*it).second;
    }
  }

  /// @brief Heterogeneous mutable overload of `operator[]`.
  ///
  /// Inserts a default-constructed value when `key` is not present.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  Value& operator[](const K& key) {
    auto it = this->lookup(key);
    if (it == this->end()) {
      return (*this->insert(std::make_pair(KeyCovert<Key, K>()(key), Value()))
                   .first)
          .second;
    } else {
      return (*it).second;
    }
  }
};

/// @brief String-specialized flat hash map with heterogeneous lookup support.
///
/// Accepts `std::string`, `const char*`, `roo::string_view`, and Arduino
/// `String` (when available) for lookup operations.
template <typename Value>
using FlatSmallStringHashMap =
    FlatSmallHashMap<std::string, Value, TransparentStringHashFn,
                     TransparentEq>;

}  // namespace roo_collections
