#pragma once

/// @file
/// @brief Inline immutable owners sharing a live read-only flat-table base.
/// @ingroup roo_collections

#if __cplusplus >= 201703L

#include <array>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include "roo_collections/impl/immutable_flat_small_hashtable_capacity.h"
#include "roo_collections/read_only_flat_small_hashtable.h"

namespace roo_collections {

/// Default immutable hash policy, identical to the mutable/read-only policy.
/// Integral, enum and string-view keys support constant evaluation. Other key
/// types can supply a constexpr custom policy when needed.
template <typename Key, typename Enable = void>
using ImmutableHashFn = DefaultHashFn<Key, Enable>;

template <typename Entry, typename Key, size_t N, typename HashFn,
          typename KeyFn, typename KeyCmpFn>
class ImmutableFlatSmallHashtable;

namespace impl {

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

}  // namespace impl

namespace impl {

/// Owns a pointer-free prepared table for direct constexpr owner construction.
template <typename Entry, typename Key, size_t N, typename HashFn,
          typename KeyFn, typename KeyCmpFn>
class ImmutableFlatSmallHashtableData {
  static_assert(N <= 64000, "Immutable table exceeds maximum supported size");
  using Capacity = impl::ImmutableFlatSmallHashtableCapacity<N>;
  static constexpr uint16_t kSlotCount = Capacity::kSlotCount;

 public:
  /// Copies and hashes exactly N distinct entries; duplicates are rejected.
  template <size_t M>
  constexpr ImmutableFlatSmallHashtableData(const Entry (&entries)[M],
                                            const HashFn& hash_fn,
                                            const KeyFn& key_fn,
                                            const KeyCmpFn& key_cmp_fn)
      : hash_fn_(hash_fn), key_fn_(key_fn), key_cmp_fn_(key_cmp_fn) {
    static_assert(M == N, "Immutable table entry count mismatch");
    initialize(entries);
  }

  /// Copies and hashes an array, including an empty array.
  constexpr ImmutableFlatSmallHashtableData(const std::array<Entry, N>& entries,
                                            const HashFn& hash_fn,
                                            const KeyFn& key_fn,
                                            const KeyCmpFn& key_cmp_fn)
      : hash_fn_(hash_fn), key_fn_(key_fn), key_cmp_fn_(key_cmp_fn) {
    initialize(entries);
  }

 private:
  template <typename, typename, size_t, typename, typename, typename>
  friend class ::roo_collections::ImmutableFlatSmallHashtable;

  /// Uses the same probe sequence and fingerprints as runtime lookup.
  template <typename Entries>
  constexpr void initialize(const Entries& entries) {
    for (const Entry& entry : entries) {
      const Key& key = key_fn_(entry);
      uint32_t hash = hash_fn_(key);
      internal::FlatSmallHashtableProbe probe(Capacity::kValue.reduceHash(hash),
                                              kSlotCount);
      while (states_[probe.position()] != 0) {
        if (key_cmp_fn_(key_fn_(entries_[probe.position()]), key)) {
          RejectImmutableDuplicateKey();
        }
        probe.advance();
      }
      CopyImmutableEntry(entries_[probe.position()], entry);
      states_[probe.position()] = static_cast<int8_t>((hash & 0x7F) | 0x80);
    }
  }

  HashFn hash_fn_;
  KeyFn key_fn_;
  KeyCmpFn key_cmp_fn_;
  std::array<Entry, kSlotCount> entries_{};
  std::array<int8_t, kSlotCount> states_{};
};

}  // namespace impl

/// @brief Owns an immutable inline table with a common live read-only base.
///
/// Entries must be default constructible and copy assignable. Their operations
/// and the policies must support constant evaluation for constexpr use.
/// Borrowed entry data must outlive access through the owner or its read-only
/// base. Duplicate input keys fail constant evaluation or abort runtime
/// construction. Use WithEntries followed by direct owner construction for
/// portable C++17 constexpr initialization. Ordinary constructors and runtime
/// factories remain available. Actual flash placement is a target/linker
/// property.
///
/// @tparam Entry Stored entry type.
/// @tparam Key Lookup key type.
/// @tparam N Number of distinct entries, at most 64000.
/// @tparam HashFn Hash policy, identical at compile time and runtime.
/// @tparam KeyFn Extracts the lookup key.
/// @tparam KeyCmpFn Equality policy consistent with HashFn.
template <typename Entry, typename Key, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>,
          typename KeyCmpFn = std::equal_to<Key>>
class ImmutableFlatSmallHashtable
    : public ReadOnlyFlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn> {
  using ReadOnly =
      ReadOnlyFlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn>;
  using State = typename ReadOnly::State;
  using ReadOnly::hash_fn_;
  using ReadOnly::key_cmp_fn_;
  using ReadOnly::key_fn_;
  static_assert(N <= 64000, "Immutable table exceeds maximum supported size");
  using Capacity = impl::ImmutableFlatSmallHashtableCapacity<N>;
  static constexpr uint16_t kSlotCount = Capacity::kSlotCount;

 public:
  using read_only_type = ReadOnly;
  using Data = impl::ImmutableFlatSmallHashtableData<Entry, Key, N, HashFn,
                                                     KeyFn, KeyCmpFn>;
  using ReadOnly::operator==;
  using ReadOnly::operator!=;

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

  /// Creates an empty table; requires N to be zero.
  constexpr ImmutableFlatSmallHashtable(HashFn hash_fn = HashFn(),
                                        KeyFn key_fn = KeyFn(),
                                        KeyCmpFn key_cmp_fn = KeyCmpFn())
      : ReadOnly(hash_fn, key_fn, key_cmp_fn) {
    static_assert(N == 0, "Nonempty immutable tables require entries");
    bindStorage();
  }

  /// Copies prepared data into this owner and binds the base to its own arrays.
  /// Direct construction avoids self-referential factory returns in C++17.
  explicit constexpr ImmutableFlatSmallHashtable(const Data& data)
      : ReadOnly(data.hash_fn_, data.key_fn_, data.key_cmp_fn_) {
    copyStorage(data.entries_, data.states_);
    bindStorage();
  }

  /// Consumes prepared storage, moving entries when their move is noexcept.
  /// Copy-only policy and entry types remain supported.
  template <typename E = Entry, typename = typename std::enable_if<
                                    std::is_move_constructible<E>::value>::type>
  explicit constexpr ImmutableFlatSmallHashtable(Data&& data)
      : ReadOnly(std::move_if_noexcept(data.hash_fn_),
                 std::move_if_noexcept(data.key_fn_),
                 std::move_if_noexcept(data.key_cmp_fn_)),
        entries_(std::move_if_noexcept(data.entries_)),
        states_(data.states_) {
    bindStorage();
  }

  /// Creates a table by copying N distinct entries from the input array.
  /// Duplicates fail constant evaluation or abort runtime construction.
  template <size_t M>
  constexpr ImmutableFlatSmallHashtable(const Entry (&entries)[M],
                                        HashFn hash_fn = HashFn(),
                                        KeyFn key_fn = KeyFn(),
                                        KeyCmpFn key_cmp_fn = KeyCmpFn())
      : ImmutableFlatSmallHashtable(Data(
            entries, std::move_if_noexcept(hash_fn),
            std::move_if_noexcept(key_fn), std::move_if_noexcept(key_cmp_fn))) {
  }

  /// Creates a table from std::array, including zero entries.
  /// Duplicates fail constant evaluation or abort runtime construction.
  constexpr ImmutableFlatSmallHashtable(const std::array<Entry, N>& entries,
                                        HashFn hash_fn = HashFn(),
                                        KeyFn key_fn = KeyFn(),
                                        KeyCmpFn key_cmp_fn = KeyCmpFn())
      : ImmutableFlatSmallHashtable(Data(
            entries, std::move_if_noexcept(hash_fn),
            std::move_if_noexcept(key_fn), std::move_if_noexcept(key_cmp_fn))) {
  }

  /// Copies inline arrays and binds this owner's base to the copies.
  constexpr ImmutableFlatSmallHashtable(
      const ImmutableFlatSmallHashtable& other)
      : ReadOnly(other.hash_fn_, other.key_fn_, other.key_cmp_fn_) {
    copyStorage(other.entries_, other.states_);
    bindStorage();
  }

  /// Copies inline storage, preserving valid independent storage in both
  /// owners.
  constexpr ImmutableFlatSmallHashtable(ImmutableFlatSmallHashtable&& other)
      : ImmutableFlatSmallHashtable(
            static_cast<const ImmutableFlatSmallHashtable&>(other)) {}

  /// Assigns inline data while retaining this object's live read-only base.
  constexpr ImmutableFlatSmallHashtable& operator=(
      const ImmutableFlatSmallHashtable& other) {
    if (this != &other) {
      hash_fn_ = other.hash_fn_;
      key_fn_ = other.key_fn_;
      key_cmp_fn_ = other.key_cmp_fn_;
      for (size_t i = 0; i < kSlotCount; ++i) {
        impl::CopyImmutableEntry(entries_[i], other.entries_[i]);
        states_[i] = other.states_[i];
      }
      bindStorage();
    }
    return *this;
  }

  /// Assigns independent inline storage; the source remains valid.
  constexpr ImmutableFlatSmallHashtable& operator=(
      ImmutableFlatSmallHashtable&& other) {
    return *this = static_cast<const ImmutableFlatSmallHashtable&>(other);
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
        Capacity::kValue.reduceHash(hash), hash, key, key_fn_, key_cmp_fn_);
    return ConstIterator(this, pos);
  }

  /// Copies prepared slots without requiring entries to be copy constructible.
  constexpr void copyStorage(const std::array<Entry, kSlotCount>& entries,
                             const std::array<State, kSlotCount>& states) {
    for (size_t i = 0; i < kSlotCount; ++i) {
      impl::CopyImmutableEntry(entries_[i], entries[i]);
      states_[i] = states[i];
    }
  }

  /// Updates the shared base after construction or assignment, never lookup.
  constexpr void bindStorage() {
    ReadOnly::buffer_ = entries_.data();
    ReadOnly::states_ = states_.data();
    ReadOnly::capacity_info_ = &Capacity::kValue;
    ReadOnly::slot_count_ = kSlotCount;
    ReadOnly::dummy_empty_state_ = 0;
    for (uint16_t i = 0; i < kSlotCount && i < 8; ++i) {
      if (states_[i] < 0) ReadOnly::dummy_empty_state_ |= uint16_t{1} << i;
    }
    ReadOnly::used_ = N;
    ReadOnly::erased_ = 0;
    ReadOnly::resize_threshold_ = N;
  }

  std::array<Entry, kSlotCount> entries_{};
  std::array<State, kSlotCount> states_{};
};

/// Deduces an immutable owner from pointer-free prepared table data.
template <typename Entry, typename Key, size_t N, typename HashFn,
          typename KeyFn, typename KeyCmpFn>
ImmutableFlatSmallHashtable(const impl::ImmutableFlatSmallHashtableData<
                            Entry, Key, N, HashFn, KeyFn, KeyCmpFn>&)
    -> ImmutableFlatSmallHashtable<Entry, Key, N, HashFn, KeyFn, KeyCmpFn>;

/// Prepares @p entries for direct C++17 constexpr table construction.
/// Returns pointer-free data; construct ImmutableFlatSmallHashtable from it
/// to deduce the entry count and bind the owner's inline storage.
/// Duplicate keys fail constant evaluation or abort runtime construction.
template <typename Entry, typename Key = Entry, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto WithEntries(const Entry (&entries)[N], HashFn hash_fn = HashFn(),
                           KeyFn key_fn = KeyFn(),
                           KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return impl::ImmutableFlatSmallHashtableData<Entry, Key, N, HashFn, KeyFn,
                                               KeyCmpFn>(
      entries, std::move_if_noexcept(hash_fn), std::move_if_noexcept(key_fn),
      std::move_if_noexcept(key_cmp_fn));
}

/// Builds pointer-free data from std::array, including an empty input.
/// Use the result to directly construct an immutable owner.
template <typename Entry, typename Key = Entry, size_t N,
          typename HashFn = ImmutableHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>,
          typename KeyCmpFn = std::equal_to<Key>>
constexpr auto WithEntries(const std::array<Entry, N>& entries,
                           HashFn hash_fn = HashFn(), KeyFn key_fn = KeyFn(),
                           KeyCmpFn key_cmp_fn = KeyCmpFn()) {
  return impl::ImmutableFlatSmallHashtableData<Entry, Key, N, HashFn, KeyFn,
                                               KeyCmpFn>(
      entries, std::move_if_noexcept(hash_fn), std::move_if_noexcept(key_fn),
      std::move_if_noexcept(key_cmp_fn));
}

/// @brief Returns an immutable owner, deducing its count from an array.
/// For constexpr use on GCC, prefer direct construction from WithEntries;
/// GCC may reject factory returns of objects with pointers into themselves.
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
