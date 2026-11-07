#pragma once

/// @file
/// @brief Common live read-only interface for heap and inline flat tables.
/// @ingroup roo_collections

#include "roo_collections/impl/flat_small_hashtable_common.h"
#include "roo_collections/impl/flat_small_hashtable_lookup.h"
#include "roo_collections/impl/read_only_map_access.h"

namespace roo_collections {

template <typename Entry, typename Key, typename HashFn, typename KeyFn,
          typename KeyCmpFn>
class FlatSmallHashtable;

/// @brief Borrows a live table through const lookup and iteration.
///
/// Both mutable and immutable owners derive from this type. References to the
/// base observe subsequent owner updates, including rehashing. Iterators and
/// entry references retain the owning container's invalidation rules. The owner
/// and any borrowed entry data must outlive access through this interface.
/// Construction, assignment and destruction are protected: use pointers or
/// references, and destroy the concrete owner rather than this base.
///
/// @tparam Entry Stored entry type.
/// @tparam Key Lookup key type.
/// @tparam HashFn Key hash policy.
/// @tparam KeyFn Extracts the lookup key from an entry.
/// @tparam KeyCmpFn Key equality policy.
template <typename Entry, typename Key, typename HashFn = DefaultHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>,
          typename KeyCmpFn = std::equal_to<Key>>
class ReadOnlyFlatSmallHashtable
    : public impl::ReadOnlyMapAccess<
          ReadOnlyFlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn>,
          KeyFn, HashFn, KeyCmpFn> {
 public:
  /// @brief Constant forward iterator.
  class ConstIterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = const Entry;
    using pointer = const Entry*;
    using reference = const Entry&;

    /// Creates a singular iterator.
    constexpr ConstIterator() : ConstIterator(nullptr, 0) {}

    /// Returns the current entry; requires a dereferenceable iterator.
    constexpr const Entry& operator*() const { return ht_->buffer_[pos_]; }

    /// Returns the current entry address.
    constexpr const Entry* operator->() const { return &ht_->buffer_[pos_]; }

    /// Advances to the next occupied bucket.
    ROO_COLLECTIONS_CONSTEXPR14 ConstIterator& operator++() {
      uint16_t ht_len = ht_->ht_len();
      do {
        ++pos_;
      } while (pos_ < ht_len && ht_->states_[pos_] >= 0);
      return *this;
    }

    /// Advances and returns the previous iterator.
    ROO_COLLECTIONS_CONSTEXPR14 ConstIterator operator++(int) {
      ConstIterator itr = *this;
      operator++();
      return itr;
    }

    /// Compares owner identity and bucket position.
    constexpr bool operator==(const ConstIterator& other) const {
      return ht_ == other.ht_ && pos_ == other.pos_;
    }

    /// Returns whether the iterators differ.
    constexpr bool operator!=(const ConstIterator& other) const {
      return ht_ != other.ht_ || pos_ != other.pos_;
    }

   private:
    friend class ReadOnlyFlatSmallHashtable;
    template <typename, typename, typename, typename, typename>
    friend class FlatSmallHashtable;

    constexpr ConstIterator(const ReadOnlyFlatSmallHashtable<
                                Entry, Key, HashFn, KeyFn, KeyCmpFn>* ht,
                            uint16_t pos)
        : ht_(ht), pos_(pos) {}

    const ReadOnlyFlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn>* ht_;
    uint16_t pos_;
  };

  using key_type = Key;
  using value_type = Entry;
  using hasher = HashFn;
  using key_equal = KeyCmpFn;
  using iterator = ConstIterator;
  using const_iterator = ConstIterator;

  /// @brief Equality comparison by key/value content.
  ROO_COLLECTIONS_CONSTEXPR14 bool operator==(
      const ReadOnlyFlatSmallHashtable& other) const {
    if (other.size() != size()) return false;
    // Note: maps with different capacities or different insert/erase history
    // may have different iteration order, thus we need to use lookup on one of
    // them.
    for (const auto& e : *this) {
      auto itr = other.find(key_fn_(e));
      if (itr == other.end()) return false;
      if (!(*itr == e)) return false;
    }
    return true;
  }

  /// Returns whether the tables differ in key/value content.
  ROO_COLLECTIONS_CONSTEXPR14 bool operator!=(
      const ReadOnlyFlatSmallHashtable& other) const {
    return !(*this == other);
  }

  /// @brief Returns the internal bucket array length.
  constexpr uint16_t ht_len() const { return kRadkePrimes[capacity_idx_]; }

  /// @brief Returns an iterator to the first element.
  ROO_COLLECTIONS_CONSTEXPR14 ConstIterator begin() const {
    uint16_t cap = ht_len();
    uint16_t pos = 0;
    for (; pos < cap; ++pos) {
      if (states_[pos] < 0) break;
    }
    return ConstIterator(this, pos);
  }

  /// @brief Returns const iterator past the end.
  ROO_COLLECTIONS_CONSTEXPR14 ConstIterator end() const {
    return ConstIterator(this, ht_len());
  }

  /// @brief Returns the number of stored elements.
  constexpr uint16_t size() const { return used_ - erased_; }

  /// @brief Returns whether the table is empty.
  constexpr bool empty() const { return used_ == erased_; }

  /// @brief Returns the number of elements insertable before rehashing.
  constexpr uint16_t capacity() const { return resize_threshold_; }

  /// @brief Finds `key` and returns a const iterator to the matching entry.
  /// @return `end()` when not found.
  ROO_COLLECTIONS_CONSTEXPR14 ConstIterator find(const Key& key) const {
    uint32_t hash = hash_fn_(key);
    uint16_t pos = internal::FindFlatSmallBucket(
        buffer_, states_, ht_len(), fastmod(hash, capacity_idx_), hash, key,
        key_fn_, key_cmp_fn_);
    return ConstIterator(this, pos);
  }

  /// @brief Heterogeneous lookup overload of `find`.
  /// @return `end()` when not found.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  ROO_COLLECTIONS_CONSTEXPR14 ConstIterator find(const K& key) const {
    uint32_t hash = hash_fn_(key);
    uint16_t pos = internal::FindFlatSmallBucket(
        buffer_, states_, ht_len(), fastmod(hash, capacity_idx_), hash, key,
        key_fn_, key_cmp_fn_);
    return ConstIterator(this, pos);
  }

  /// Returns a const iterator to the first entry.
  ROO_COLLECTIONS_CONSTEXPR14 ConstIterator cbegin() const { return begin(); }

  /// Returns the const past-the-end iterator.
  ROO_COLLECTIONS_CONSTEXPR14 ConstIterator cend() const { return end(); }

  /// Tests membership without exposing mutable storage.
  ROO_COLLECTIONS_CONSTEXPR14 bool contains(const Key& key) const {
    return find(key) != end();
  }

  /// Tests membership using transparent hash and equality policies.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  ROO_COLLECTIONS_CONSTEXPR14 bool contains(const K& key) const {
    return find(key) != end();
  }

 protected:
  using State = int8_t;
  static constexpr State EMPTY = 0;
  static constexpr State DELETED = 1;
  // Occupied states use 0x80 | (hash & 0x7F).

  /// Copies policies; concrete owners bind storage before publishing it.
  constexpr ReadOnlyFlatSmallHashtable(const HashFn& hash_fn,
                                       const KeyFn& key_fn,
                                       const KeyCmpFn& key_cmp_fn)
      : hash_fn_(hash_fn), key_fn_(key_fn), key_cmp_fn_(key_cmp_fn) {}

  /// Moves policies when ownership is transferred between mutable tables.
  constexpr ReadOnlyFlatSmallHashtable(HashFn&& hash_fn, KeyFn&& key_fn,
                                       KeyCmpFn&& key_cmp_fn)
      : hash_fn_(std::move(hash_fn)),
        key_fn_(std::move(key_fn)),
        key_cmp_fn_(std::move(key_cmp_fn)) {}

  ReadOnlyFlatSmallHashtable(const ReadOnlyFlatSmallHashtable&) = default;

  ReadOnlyFlatSmallHashtable& operator=(const ReadOnlyFlatSmallHashtable&) =
      default;

  ~ReadOnlyFlatSmallHashtable() = default;

  // Keep the original mutable-table field order and a single copy of storage
  // pointers/counters. Only owners may mutate them; public access is const.
  HashFn hash_fn_;
  KeyFn key_fn_;
  KeyCmpFn key_cmp_fn_;
  int capacity_idx_ = 0;
  uint16_t used_ = 0;
  uint16_t erased_ = 0;
  uint16_t resize_threshold_ = 0;
  Entry* buffer_ = nullptr;
  State* states_ = nullptr;
  State dummy_empty_state_ = EMPTY;
};

}  // namespace roo_collections
