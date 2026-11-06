#pragma once

/// @file
/// @brief Inline, immutable flat hash tables supporting C++17 constant
/// evaluation.
/// @ingroup roo_collections

#if __cplusplus >= 201703L

#include <array>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

/// @brief Default immutable-table hash, constexpr for integers, enums and
/// views.
///
/// Other keys use DefaultHashFn; supply a constexpr custom hash when needed.
/// Pointer keys generally cannot be hashed during constant evaluation. For
/// string content, prefer roo::string_view pointing to static character data.
template <typename Key, typename Enable = void>
struct ImmutableHashFn : public DefaultHashFn<Key> {};

/// @brief Hashes integral and enum keys without relying on std::hash constexpr.
template <typename Key>
struct ImmutableHashFn<
    Key, typename std::enable_if<std::is_integral<Key>::value ||
                                 std::is_enum<Key>::value>::type> {
  /// Returns the key converted to the platform's unsigned hash width.
  constexpr size_t operator()(Key key) const {
    return static_cast<size_t>(key);
  }
};

namespace internal {

/// Copies entries during constant evaluation, including pre-C++20 std::pair.
template <typename Entry>
constexpr void CopyImmutableEntry(Entry& target, const Entry& source) {
  target = source;
}

/// Copies pair fields because std::pair assignment is not constexpr in C++17.
template <typename Key, typename Value>
constexpr void CopyImmutableEntry(std::pair<Key, Value>& target,
                                  const std::pair<Key, Value>& source) {
  CopyImmutableEntry(target.first, source.first);
  CopyImmutableEntry(target.second, source.second);
}

/// Rejects duplicate input in release builds as well as constant evaluation.
[[noreturn]] inline void RejectImmutableDuplicateKey() { std::abort(); }

}  // namespace internal

/// @brief Read-only flat table with inline arrays and no table-owned heap
/// allocation.
///
/// Construction hashes exactly N distinct entries into a fixed-size table.
/// Declaring the result static constexpr enables flash-backed constant storage
/// on targets with memory-mapped flash. Keys, entries and functors must support
/// constant evaluation for constexpr construction; runtime construction is also
/// supported. Entries must be default constructible and copy assignable.
/// Borrowed data, such as string_view contents, must outlive the table.
///
/// @tparam Entry Stored entry type.
/// @tparam Key Lookup key type.
/// @tparam N Number of distinct entries, at most 64000.
/// @tparam HashFn Key hash, identical at compile time and runtime.
/// @tparam KeyFn Extracts a key from an entry.
/// @tparam KeyCmpFn Key equality predicate, consistent with HashFn.
template <typename Entry, typename Key, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>,
          typename KeyCmpFn = std::equal_to<Key>>
class ImmutableFlatSmallHashtable {
  static_assert(N <= 64000, "Immutable table exceeds maximum supported size");
  static constexpr int kCapacityIdx = initialCapacityIdx(N);
  static constexpr uint16_t kSlotCount = kRadkePrimes[kCapacityIdx];

 public:
  /// @brief Forward iterator exposing only const entries.
  class ConstIterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = const Entry*;
    using reference = const Entry&;

    /// Creates a singular iterator.
    constexpr ConstIterator() : table_(nullptr), position_(0) {}

    /// Returns the current entry; requires a dereferenceable iterator.
    constexpr reference operator*() const {
      return table_->entries_[position_];
    }

    /// Returns the current entry address.
    constexpr pointer operator->() const {
      return &table_->entries_[position_];
    }

    /// Advances to the next occupied bucket.
    constexpr ConstIterator& operator++() {
      do {
        ++position_;
      } while (position_ < kSlotCount && table_->states_[position_] == 0);
      return *this;
    }

    /// Advances and returns the previous iterator.
    constexpr ConstIterator operator++(int) {
      ConstIterator previous = *this;
      ++*this;
      return previous;
    }

    /// Returns whether the iterators identify the same table and position.
    constexpr bool operator==(const ConstIterator& other) const {
      return table_ == other.table_ && position_ == other.position_;
    }

    /// Returns whether the iterators differ.
    constexpr bool operator!=(const ConstIterator& other) const {
      return !(*this == other);
    }

   private:
    friend class ImmutableFlatSmallHashtable;

    constexpr ConstIterator(const ImmutableFlatSmallHashtable* table,
                            uint16_t position)
        : table_(table), position_(position) {}

    const ImmutableFlatSmallHashtable* table_;
    uint16_t position_;
  };

  using key_type = Key;
  using value_type = Entry;
  using hasher = HashFn;
  using key_equal = KeyCmpFn;
  using const_iterator = ConstIterator;
  using iterator = ConstIterator;

  /// @brief Creates an empty table; available only when N is zero.
  constexpr ImmutableFlatSmallHashtable(HashFn hash_fn = HashFn(),
                                        KeyFn key_fn = KeyFn(),
                                        KeyCmpFn key_cmp_fn = KeyCmpFn())
      : hash_fn_(hash_fn), key_fn_(key_fn), key_cmp_fn_(key_cmp_fn) {
    static_assert(N == 0, "Nonempty immutable tables require entries");
  }

  /// @brief Creates a table by copying N entries from @p entries.
  /// Duplicate keys fail constant evaluation or abort runtime construction,
  /// including when NDEBUG is defined.
  template <size_t M>
  constexpr ImmutableFlatSmallHashtable(const Entry (&entries)[M],
                                        HashFn hash_fn = HashFn(),
                                        KeyFn key_fn = KeyFn(),
                                        KeyCmpFn key_cmp_fn = KeyCmpFn())
      : hash_fn_(hash_fn), key_fn_(key_fn), key_cmp_fn_(key_cmp_fn) {
    static_assert(M == N, "Immutable table entry count mismatch");
    initialize(entries);
  }

  /// @brief Creates a table from @p entries, including an empty std::array.
  /// Duplicate keys fail constant evaluation or abort runtime construction.
  constexpr ImmutableFlatSmallHashtable(const std::array<Entry, N>& entries,
                                        HashFn hash_fn = HashFn(),
                                        KeyFn key_fn = KeyFn(),
                                        KeyCmpFn key_cmp_fn = KeyCmpFn())
      : hash_fn_(hash_fn), key_fn_(key_fn), key_cmp_fn_(key_cmp_fn) {
    initialize(entries);
  }

  /// Returns the number of entries.
  constexpr uint16_t size() const { return N; }

  /// Returns whether the table has no entries.
  constexpr bool empty() const { return N == 0; }

  /// Returns the fixed entry count; immutable tables never grow.
  constexpr uint16_t capacity() const { return N; }

  /// Returns the number of buckets, including empty buckets.
  constexpr uint16_t ht_len() const { return kSlotCount; }

  /// Returns an iterator to the first entry, or end() for an empty table.
  constexpr const_iterator begin() const {
    uint16_t pos = 0;
    while (pos < kSlotCount && states_[pos] == 0) ++pos;
    return ConstIterator(this, pos);
  }

  /// Returns the past-the-end iterator.
  constexpr const_iterator end() const {
    return ConstIterator(this, kSlotCount);
  }

  /// Returns a const iterator to the first entry.
  constexpr const_iterator cbegin() const { return begin(); }

  /// Returns the const past-the-end iterator.
  constexpr const_iterator cend() const { return end(); }

  /// Finds @p key, returning end() when absent.
  constexpr const_iterator find(const Key& key) const { return findKey(key); }

  /// Finds a heterogeneous key with transparent hash and equality functions.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  constexpr const_iterator find(const K& key) const {
    return findKey(key);
  }

  /// Returns whether @p key is present.
  constexpr bool contains(const Key& key) const { return find(key) != end(); }

  /// Tests heterogeneous membership with transparent hash and equality
  /// functions.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  constexpr bool contains(const K& key) const {
    return find(key) != end();
  }

  /// Compares entries by key and value, independent of bucket order.
  constexpr bool operator==(const ImmutableFlatSmallHashtable& other) const {
    for (const Entry& entry : *this) {
      const_iterator it = other.find(key_fn_(entry));
      if (it == other.end() || !(*it == entry)) return false;
    }
    return true;
  }

  /// Returns whether the tables differ in content.
  constexpr bool operator!=(const ImmutableFlatSmallHashtable& other) const {
    return !(*this == other);
  }

 private:
  /// Searches inline storage using the shared read-only probe algorithm.
  template <typename K>
  constexpr const_iterator findKey(const K& key) const {
    uint32_t hash = hash_fn_(key);
    uint16_t pos = internal::FindFlatSmallBucket(
        entries_.data(), states_.data(), kSlotCount,
        fastmod(hash, kCapacityIdx), hash, key, key_fn_, key_cmp_fn_);
    return ConstIterator(this, pos);
  }

  /// Places entries with the same probe sequence and fingerprints as lookup.
  template <typename Entries>
  constexpr void initialize(const Entries& entries) {
    for (const Entry& entry : entries) {
      const Key& key = key_fn_(entry);
      uint32_t hash = hash_fn_(key);
      internal::FlatSmallHashtableProbe probe(fastmod(hash, kCapacityIdx),
                                              kSlotCount);
      while (states_[probe.position()] != 0) {
        if (key_cmp_fn_(key_fn_(entries_[probe.position()]), key)) {
          internal::RejectImmutableDuplicateKey();
        }
        probe.advance();
      }
      internal::CopyImmutableEntry(entries_[probe.position()], entry);
      states_[probe.position()] = (hash & 0x7F) | 0x80;
    }
  }

  HashFn hash_fn_;
  KeyFn key_fn_;
  KeyCmpFn key_cmp_fn_;
  std::array<Entry, kSlotCount> entries_{};
  std::array<uint8_t, kSlotCount> states_{};
};

/// @brief Builds an immutable table, deducing its entry count from an array.
/// Specify Entry and Key when key extraction changes the key type. Duplicate
/// keys fail constant evaluation or abort runtime construction.
template <typename Entry, typename Key = Entry, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto MakeImmutableFlatSmallHashtable(
    const Entry (&entries)[N], HashFn hash_fn = HashFn(),
    KeyFn key_fn = KeyFn(), KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return ImmutableFlatSmallHashtable<Entry, Key, N, HashFn, KeyFn, KeyCmpFn>(
      entries, hash_fn, key_fn, key_cmp_fn);
}

/// @brief Builds an immutable table from std::array, including zero entries.
/// Duplicate keys fail constant evaluation or abort runtime construction.
template <typename Entry, typename Key = Entry, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto MakeImmutableFlatSmallHashtable(
    const std::array<Entry, N>& entries, HashFn hash_fn = HashFn(),
    KeyFn key_fn = KeyFn(), KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return ImmutableFlatSmallHashtable<Entry, Key, N, HashFn, KeyFn, KeyCmpFn>(
      entries, hash_fn, key_fn, key_cmp_fn);
}

}  // namespace roo_collections

#else

#warning \
    "roo_collections: ImmutableFlatSmallHashtable requires C++17 or later; declarations disabled"

#endif  // __cplusplus >= 201703L
