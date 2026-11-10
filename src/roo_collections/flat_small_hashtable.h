#pragma once

/// @file
/// @brief Flat, memory-conscious hash table implementation.
/// @ingroup roo_collections

#include "roo_collections/read_only_flat_small_hashtable.h"

namespace roo_collections {

/// @brief Flat, memory-conscious hash table optimized for small collections.
///
/// Uses open addressing with quadratic probing and stores entries in contiguous
/// arrays for low overhead. Maximum supported size is approximately 64k
/// elements.
///
/// @tparam Entry Stored entry type.
/// @tparam Key Key type used for lookup.
/// @tparam HashFn Hash function.
/// @tparam KeyFn Extracts a key from an entry.
/// @tparam KeyCmpFn Key equality predicate.
template <typename Entry, typename Key, typename HashFn = DefaultHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>,
          typename KeyCmpFn = std::equal_to<Key>>
class FlatSmallHashtable
    : public ReadOnlyFlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn> {
  using ReadOnly =
      ReadOnlyFlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn>;
  using State = typename ReadOnly::State;
  using ReadOnly::buffer_;
  using ReadOnly::capacity_info_;
  using ReadOnly::DELETED;
  using ReadOnly::dummy_empty_state_;
  using ReadOnly::EMPTY;
  using ReadOnly::erased_;
  using ReadOnly::hash_fn_;
  using ReadOnly::key_cmp_fn_;
  using ReadOnly::key_fn_;
  using ReadOnly::resize_threshold_;
  using ReadOnly::slot_count_;
  using ReadOnly::states_;
  using ReadOnly::used_;

 public:
  using read_only_type = ReadOnly;
  using ConstIterator = typename ReadOnly::ConstIterator;
  using ReadOnly::capacity;
  using ReadOnly::empty;
  using ReadOnly::find;
  using ReadOnly::ht_len;
  using ReadOnly::size;
  using ReadOnly::operator==;
  using ReadOnly::operator!=;

  /// @brief Mutable forward iterator.
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = Entry*;
    using reference = Entry&;

    Iterator() : Iterator(nullptr, 0) {}

    Entry& operator*() { return ht_->buffer_[pos_]; }
    Entry* operator->() { return &ht_->buffer_[pos_]; }

    operator ConstIterator() const { return ConstIterator(ht_, pos_); }

    Iterator& operator++() {
      pos_ = ht_->nextOccupiedBucket(pos_ + 1);
      return *this;
    }

    Iterator operator++(int n) {
      Iterator itr = *this;
      operator++();
      return itr;
    }

    bool operator==(const Iterator& other) const {
      return ht_ == other.ht_ && pos_ == other.pos_;
    }

    bool operator!=(const Iterator& other) const {
      return ht_ != other.ht_ || pos_ != other.pos_;
    }

   private:
    friend class FlatSmallHashtable;

    Iterator(FlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn>* ht,
             uint16_t pos)
        : ht_(ht), pos_(pos) {}

    FlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn>* ht_;
    // Use a machine-word index to avoid narrowing each increment on ESP32.
    uint32_t pos_;
  };

  using key_type = Key;
  using value_type = Entry;
  using hasher = HashFn;
  using key_equal = KeyCmpFn;
  using iterator = Iterator;
  using const_iterator = ConstIterator;

  /// @brief Constructs a table from an iterator range.
  template <typename InputIt>
  FlatSmallHashtable(InputIt first, InputIt last, HashFn hash_fn = HashFn(),
                     KeyFn key_fn = KeyFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : FlatSmallHashtable(initialCapacityHint(first, last), hash_fn, key_fn,
                           key_cmp_fn) {
    for (auto it = first; it != last; ++it) {
      insert(*it);
    }
  }

  /// @brief Constructs a table from an initializer list.
  FlatSmallHashtable(std::initializer_list<Entry> init,
                     HashFn hash_fn = HashFn(), KeyFn key_fn = KeyFn(),
                     KeyCmpFn key_cmp_fn = KeyCmpFn())
      : FlatSmallHashtable(init.begin(), init.end(), hash_fn, key_fn,
                           key_cmp_fn) {}

  /// @brief Constructs an empty table with default initial capacity.
  FlatSmallHashtable(HashFn hash_fn = HashFn(), KeyFn key_fn = KeyFn(),
                     KeyCmpFn key_cmp_fn = KeyCmpFn())
      : FlatSmallHashtable(8, hash_fn, key_fn, key_cmp_fn) {}

  /// @brief Constructs an empty table sized for `size_hint` elements.
  /// Chooses the smallest static capacity accommodating the hint.
  /// @param size_hint Expected number of items.
  FlatSmallHashtable(uint16_t size_hint, HashFn hash_fn = HashFn(),
                     KeyFn key_fn = KeyFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : ReadOnly(hash_fn, key_fn, key_cmp_fn) {
    int capacity_idx = initialCapacityIdx(size_hint);
    capacity_info_ = &impl::kMutableFlatSmallHashtableCapacities[capacity_idx];
    slot_count_ = capacity_info_->slot_count;
    resize_threshold_ = capacity_info_->mutableCapacity();
    buffer_ = ht_len() > 1 ? new Entry[ht_len()] : nullptr;
    states_ = ht_len() > 1 ? new State[ht_len()] : &dummy_empty_state_;
    std::fill(&states_[0], &states_[ht_len()], State(EMPTY));
  }

  /// @brief Move constructor.
  FlatSmallHashtable(FlatSmallHashtable&& other)
      : ReadOnly(std::move(other.hash_fn_), std::move(other.key_fn_),
                 std::move(other.key_cmp_fn_)) {
    dummy_empty_state_ = other.dummy_empty_state_;
    capacity_info_ = other.capacity_info_;
    slot_count_ = capacity_info_->slot_count;
    used_ = other.used_;
    erased_ = other.erased_;
    resize_threshold_ = other.resize_threshold_;
    buffer_ = other.buffer_;
    states_ = ht_len() > 1 ? other.states_ : &dummy_empty_state_;
    other.resetToEmptySentinel();
  }

  /// @brief Copy constructor.
  FlatSmallHashtable(const FlatSmallHashtable& other)
      : ReadOnly(other.hash_fn_, other.key_fn_, other.key_cmp_fn_) {
    dummy_empty_state_ = other.dummy_empty_state_;
    capacity_info_ = other.capacity_info_;
    slot_count_ = capacity_info_->slot_count;
    used_ = other.used_;
    erased_ = other.erased_;
    resize_threshold_ = other.resize_threshold_;
    buffer_ = ht_len() > 1 ? new Entry[ht_len()] : nullptr;
    states_ = ht_len() > 1 ? new State[ht_len()] : &dummy_empty_state_;
    if (other.buffer_ != nullptr) {
      std::copy(&other.buffer_[0], &other.buffer_[ht_len()], buffer_);
    }
    std::copy(&other.states_[0], &other.states_[ht_len()], states_);
  }

  /// @brief Destructor.
  ~FlatSmallHashtable() {
    if (ht_len() > 1) delete[] states_;
    delete[] buffer_;
  }

  /// @brief Move assignment.
  FlatSmallHashtable& operator=(FlatSmallHashtable&& other) {
    if (this != &other) {
      if (ht_len() > 1) delete[] states_;
      delete[] buffer_;
      hash_fn_ = std::move(other.hash_fn_);
      key_fn_ = std::move(other.key_fn_);
      key_cmp_fn_ = std::move(other.key_cmp_fn_);
      dummy_empty_state_ = other.dummy_empty_state_;
      capacity_info_ = other.capacity_info_;
      slot_count_ = capacity_info_->slot_count;
      used_ = other.used_;
      erased_ = other.erased_;
      resize_threshold_ = other.resize_threshold_;
      buffer_ = other.buffer_;
      states_ = ht_len() > 1 ? other.states_ : &dummy_empty_state_;
      other.resetToEmptySentinel();
    }
    return *this;
  }

  /// @brief Copy assignment.
  FlatSmallHashtable& operator=(const FlatSmallHashtable& other) {
    if (this != &other) {
      if (ht_len() > 1) delete[] states_;
      delete[] buffer_;
      hash_fn_ = other.hash_fn_;
      key_fn_ = other.key_fn_;
      key_cmp_fn_ = other.key_cmp_fn_;
      dummy_empty_state_ = other.dummy_empty_state_;
      capacity_info_ = other.capacity_info_;
      slot_count_ = capacity_info_->slot_count;
      used_ = other.used_;
      erased_ = other.erased_;
      resize_threshold_ = other.resize_threshold_;
      buffer_ = ht_len() > 1 ? new Entry[ht_len()] : nullptr;
      states_ = ht_len() > 1 ? new State[ht_len()] : &dummy_empty_state_;
      if (other.buffer_ != nullptr) {
        std::copy(&other.buffer_[0], &other.buffer_[ht_len()], &buffer_[0]);
      }
      std::copy(&other.states_[0], &other.states_[ht_len()], &states_[0]);
    }
    return *this;
  }

  /// @brief Equality comparison by key/value content.
  bool operator==(const FlatSmallHashtable& other) const {
    if (other.size() != size()) return false;
    // Note: maps with different capacities or different insert/erase history
    // may have different iteration order, thus we need to use lookup on one of
    // them.
    for (const auto& e : *this) {
      auto itr = other.find(key_fn_(e));
      if (itr == other.end()) return false;
      if (*itr != e) return false;
    }
    return true;
  }

  bool operator!=(const FlatSmallHashtable& other) const {
    return !(*this == other);
  }

  using ReadOnly::begin;
  using ReadOnly::end;

  /// @brief Returns a mutable iterator to the first element.
  Iterator begin() {
    uint16_t pos = this->nextOccupiedBucket(0);
    return Iterator(this, pos);
  }

  /// @brief Returns iterator past the end.
  Iterator end() { return Iterator(this, ht_len()); }

  /// @brief Removes an entry by key.
  /// @return `true` if an entry was removed.
  bool erase(const Key& key) {
    size_t hash = hash_fn_(key);
    const uint16_t pos = capacity_info_->reduceHash(hash);
    if (states_[pos] == EMPTY) return false;
    if (states_[pos] == static_cast<State>((hash & 0x7F) | 0x80) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
      buffer_[pos] = Entry();
      if (used_ == 1 && erased_ == 0) {
        // Fast path (fast-clear). It is safe to do because there was no
        // rehashing. (It only works when used_ == 1, because otherwise the
        // other items might have been rehashed away from this bucket).
        states_[pos] = EMPTY;
        dummy_empty_state_ = EMPTY;
        --used_;
      } else {
        if (pos < 8) dummy_empty_state_ &= ~(uint16_t{1} << pos);
        states_[pos] = DELETED;
        ++erased_;
      }
      return true;
    }
    const uint16_t cap = ht_len();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) return false;
      if (states_[p] == static_cast<State>((hash & 0x7F) | 0x80) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
        states_[p] = DELETED;
        if (p < 8) dummy_empty_state_ &= ~(uint16_t{1} << p);
        buffer_[p] = Entry();
        ++erased_;
        return true;
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

  /// @brief Heterogeneous key overload of `erase`.
  /// @return `true` if an entry was removed.
  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  bool erase(const K& key) {
    size_t hash = hash_fn_(key);
    const uint16_t pos = capacity_info_->reduceHash(hash);
    if (states_[pos] == EMPTY) return false;
    if (states_[pos] == static_cast<State>((hash & 0x7F) | 0x80) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
      states_[pos] = DELETED;
      if (pos < 8) dummy_empty_state_ &= ~(uint16_t{1} << pos);
      buffer_[pos] = Entry();
      ++erased_;
      return true;
    }
    const uint16_t cap = ht_len();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) return false;
      if (states_[p] == static_cast<State>((hash & 0x7F) | 0x80) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
        states_[p] = DELETED;
        if (p < 8) dummy_empty_state_ &= ~(uint16_t{1} << p);
        buffer_[p] = Entry();
        ++erased_;
        return true;
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

  /// @brief Removes the entry at `itr` and returns iterator to the next entry.
  Iterator erase(const ConstIterator& itr) {
    if (itr == end()) return end();
    Iterator next(this, itr.pos_);
    ++next;
    erase(key_fn_(*itr));
    return next;
  }

  /// @brief Removes all entries while preserving current allocated capacity.
  void clear() {
    if (used_ == 0 && erased_ == 0) return;
    std::fill(&states_[0], &states_[ht_len()], State(EMPTY));
    for (size_t i = 0; i < ht_len(); ++i) {
      buffer_[i] = Entry();
    }
    // Avoiding std::fill, because it doesn't work for non-copyable entries.
    // std::fill(&buffer_[0], &buffer_[ht_len()], Entry());
    used_ = 0;
    erased_ = 0;
    dummy_empty_state_ = 0;
  }

  /// @brief Rebuilds the table to remove tombstones and shrink capacity.
  /// Chooses the smallest static capacity accommodating the current size.
  void compact() {
    int capacity_idx = initialCapacityIdx(size());
    if (kRadkePrimes[capacity_idx] == ht_len() && erased_ == 0) return;
    FlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn> newt(
        size(), hash_fn_, key_fn_, key_cmp_fn_);
    for (auto& e : *this) {
      newt.insert(std::move(e));
    }
    *this = std::move(newt);
  }

  /// @brief Inserts `val` if key is not present.
  /// @return Pair of iterator and insertion flag.
  std::pair<Iterator, bool> insert(Entry val) {
    // The argument owns the entry until placement, so borrowing avoids a
    // second owning-key copy (and allocation for std::string keys).
    const Key& key = key_fn_(val);
    uint32_t hash = hash_fn_(key);
    uint16_t pos = capacity_info_->reduceHash(hash);
    if (states_[pos] == EMPTY && used_ < resize_threshold_) {
      return insertAt(pos, val, hash);
    }
    if (states_[pos] == static_cast<State>((hash & 0x7F) | 0x80) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
      return std::make_pair(Iterator(this, pos), false);
    }

    const uint16_t cap = ht_len();
    uint16_t deleted = cap;
    internal::FlatSmallHashtableProbe probe(pos, cap);
    for (uint32_t visited = 0; visited < cap; ++visited) {
      pos = probe.position();
      if (states_[pos] == EMPTY) break;
      if (states_[pos] == DELETED) {
        if (deleted == cap) deleted = pos;
      } else if ((states_[pos] & 0x7F) == (hash & 0x7F) &&
                 key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
        return std::make_pair(Iterator(this, pos), false);
      }
      if (visited + 1 < cap) probe.advance();
    }
    // Probe through tombstones before reusing one: an equal key may follow it.
    if (deleted != cap) pos = deleted;
    assert(states_[pos] == EMPTY || states_[pos] == DELETED);
    if (size() >= resize_threshold_ ||
        (states_[pos] == EMPTY && used_ >= tombstoneLimit())) {
      rebuildForInsert();
      return insert(std::move(val));
    }
    return insertAt(pos, val, hash);
  }

 protected:
  Iterator lookup(const Key& key) {
    size_t hash = hash_fn_(key);
    const uint16_t pos = capacity_info_->reduceHash(hash);
    if (states_[pos] == EMPTY) return end();
    if (states_[pos] == static_cast<State>((hash & 0x7F) | 0x80) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
      return Iterator(this, pos);
    }
    const uint16_t cap = ht_len();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) return end();
      if (states_[p] == static_cast<State>((hash & 0x7F) | 0x80) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
        return Iterator(this, p);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  Iterator lookup(const K& key) {
    size_t hash = hash_fn_(key);
    const uint16_t pos = capacity_info_->reduceHash(hash);
    if (states_[pos] == EMPTY) return end();
    if (states_[pos] == static_cast<State>((hash & 0x7F) | 0x80) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
      return Iterator(this, pos);
    }
    const uint16_t cap = ht_len();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) return end();
      if (states_[p] == static_cast<State>((hash & 0x7F) | 0x80) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
        return Iterator(this, p);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

 private:
  /// Places an absent key, distinguishing a recycled tombstone from new use.
  std::pair<Iterator, bool> insertAt(uint16_t pos, Entry& val, uint32_t hash) {
    bool recycled = states_[pos] == DELETED;
    buffer_[pos] = std::move(val);
    states_[pos] = (hash & 0x7F) | 0x80;
    if (pos < 8) dummy_empty_state_ |= uint16_t{1} << pos;
    if (recycled) {
      --erased_;
    } else {
      ++used_;
    }
    return std::make_pair(Iterator(this, pos), true);
  }

  /// Lets tombstones occupy about half the spare buckets before compaction.
  /// Retaining an empty bucket guarantees that absent-key lookup terminates.
  uint16_t tombstoneLimit() const {
    uint32_t cap = ht_len();
    return std::min<uint32_t>(cap - 1, (resize_threshold_ + cap + 1) / 2);
  }

  /// Grows for live entries or reclaims tombstones without shrinking storage.
  void rebuildForInsert() {
    if (empty() && erased_ > 0) {
      clear();
      return;
    }
    uint32_t size_hint = capacity();
    if (size() >= capacity()) {
      size_hint =
          std::min<uint32_t>(64000, std::max<uint32_t>(1, 2u * capacity()));
    }
    FlatSmallHashtable newt(static_cast<uint16_t>(size_hint), hash_fn_, key_fn_,
                            key_cmp_fn_);
    assert(newt.capacity() >= size() + 1);
    for (auto& entry : *this) newt.insert(std::move(entry));
    if (newt.capacity() == capacity()) {
      // Keep the original arrays to avoid allocator churn when only cleaning.
      used_ = newt.used_;
      erased_ = 0;
      dummy_empty_state_ = newt.dummy_empty_state_;
      memcpy(states_, newt.states_, ht_len() * sizeof(State));
      for (size_t i = 0; i < ht_len(); ++i) {
        buffer_[i] = std::move(newt.buffer_[i]);
      }
    } else {
      *this = std::move(newt);
    }
  }

  void resetToEmptySentinel() {
    capacity_info_ = &impl::kMutableFlatSmallHashtableCapacities[0];
    slot_count_ = capacity_info_->slot_count;
    used_ = 0;
    erased_ = 0;
    resize_threshold_ = 0;
    buffer_ = nullptr;
    dummy_empty_state_ = EMPTY;
    states_ = &dummy_empty_state_;
  }

  friend class Iterator;
};

}  // namespace roo_collections
