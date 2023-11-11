#pragma once

#include <assert.h>
#include <inttypes.h>

#include <functional>
#include <memory>

namespace roo_collections {

// Slightly higher than conventional 0.7, mostly so that the smallest hashtable
// (with ht_len 11) can still hold 8 elements.
static constexpr float kMaxFillRatio = 0.73;

// Sequence of the largest primes of the format 4n+3, less than 2^k,
// for k = 4 ... 16. When used as hash map capacities, they are known to
// enable quadratic residue search to visit the entire array.
static constexpr uint16_t kRadkePrimes[] = {
    0xb,   0x1f,  0x3b,   0x7f,   0xfb,   0x1f7, 0x3fb,
    0x7f7, 0xffb, 0x1fff, 0x3feb, 0x7fcf, 0xffef};

// These are precalculated (2^48 - 1) / the corresponding radke prime + 1.
// See
// https://lemire.me/blog/2019/02/08/faster-remainders-when-the-divisor-is-a-constant-beating-compilers-and-libdivide/
static constexpr uint64_t kRadkePrimeInverts[] = {
    0x1745d1745d18, 0x84210842109, 0x456c797dd4a, 0x20408102041, 0x105197f7d74,
    0x824a4e60b4,   0x4050647d9e,  0x202428adc4,  0x100501907e,  0x800400201,
    0x401506e65,    0x200c44b25,   0x100110122};

// Returns n % kRadkePrimes[idx].
inline uint16_t fastmod(uint32_t n, int idx) {
  uint64_t lowbits = (kRadkePrimeInverts[idx] * n) & 0x0000FFFFFFFFFFFF;
  return (lowbits * kRadkePrimes[idx]) >> 48;
}

inline int initialCapacityIdx(uint16_t size_hint) {
  uint32_t ht_len = (uint32_t)(((float)size_hint) / kMaxFillRatio) + 1;
  for (int radkeIdx = 0; radkeIdx < 12; ++radkeIdx) {
    if (kRadkePrimes[radkeIdx] >= ht_len) return radkeIdx;
  }
  return 12;
}

template <typename Key>
struct DefaultHashFn : public std::hash<Key> {};

// For maps, where Key == Entry.
template <typename Entry>
struct DefaultKeyFn {
  const Entry& operator()(const Entry& entry) const { return entry; }
};

// Memory-conscious small flat hashtable. It can hold up to 64000 elements.
template <typename Entry, typename Key, typename HashFn = DefaultHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>>
class FlatSmallHashtable {
 public:
  enum State { EMPTY, DELETED, FULL };

  class ConstIterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = const Entry;
    using pointer = const Entry*;
    using reference = const Entry&;

    ConstIterator() : ConstIterator(nullptr, 0) {}

    const Entry& operator*() const { return ht_->buffer_[pos_]; }
    const Entry* operator->() const { return &ht_->buffer_[pos_]; }

    ConstIterator& operator++() {
      uint16_t ht_len = ht_->ht_len();
      do {
        ++pos_;
      } while (pos_ < ht_len && ht_->states_[pos_] != FULL);
      return *this;
    }

    ConstIterator operator++(int n) {
      ConstIterator itr = *this;
      operator++();
      return itr;
    }

    bool operator==(const ConstIterator& other) const {
      return ht_ == other.ht_ && pos_ == other.pos_;
    }

    bool operator!=(const ConstIterator& other) const {
      return ht_ != other.ht_ || pos_ != other.pos_;
    }

   private:
    friend class FlatSmallHashtable;

    ConstIterator(const FlatSmallHashtable<Entry, Key, HashFn, KeyFn>* ht,
                  uint16_t pos)
        : ht_(ht), pos_(pos) {}

    const FlatSmallHashtable<Entry, Key, HashFn, KeyFn>* ht_;
    uint16_t pos_;
  };

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
      uint16_t ht_len = ht_->ht_len();
      do {
        ++pos_;
      } while (pos_ < ht_len && ht_->states_[pos_] != FULL);
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

    Iterator(FlatSmallHashtable<Entry, Key, HashFn, KeyFn>* ht, uint16_t pos)
        : ht_(ht), pos_(pos) {}

    FlatSmallHashtable<Entry, Key, HashFn, KeyFn>* ht_;
    uint16_t pos_;
  };

  template <typename InputIt>
  FlatSmallHashtable(InputIt first, InputIt last, HashFn hash_fn = HashFn(),
                     KeyFn key_fn = KeyFn())
      : FlatSmallHashtable(std::max((int)(last - first), 8), hash_fn, key_fn) {
    for (auto it = first; it != last; ++it) {
      insert(*it);
    }
  }

  FlatSmallHashtable(std::initializer_list<Entry> init,
                     HashFn hash_fn = HashFn(), KeyFn key_fn = KeyFn())
      : FlatSmallHashtable(init.begin(), init.end(), hash_fn, key_fn) {}

  FlatSmallHashtable(HashFn hash_fn = HashFn(), KeyFn key_fn = KeyFn())
      : FlatSmallHashtable(8, hash_fn, key_fn) {}

  FlatSmallHashtable(uint16_t size_hint, HashFn hash_fn = HashFn(),
                     KeyFn key_fn = KeyFn())
      : hash_fn_(hash_fn),
        key_fn_(key_fn),
        capacity_idx_(initialCapacityIdx(size_hint)),
        used_(0),
        erased_(0),
        resize_threshold_(
            capacity_idx_ == 12
                ? 64000
                : (uint16_t)(((float)kRadkePrimes[capacity_idx_]) *
                             kMaxFillRatio)),
        buffer_(new Entry[kRadkePrimes[capacity_idx_]]),
        states_(new State[kRadkePrimes[capacity_idx_]]) {
    std::fill(&states_[0], &states_[kRadkePrimes[capacity_idx_]], EMPTY);
  }

  FlatSmallHashtable(FlatSmallHashtable&& other) = default;

  FlatSmallHashtable(const FlatSmallHashtable& other)
      : hash_fn_(other.hash_fn_),
        key_fn_(other.key_fn_),
        capacity_idx_(other.capacity_idx_),
        used_(other.used_),
        erased_(other.erased_),
        resize_threshold_(other.resize_threshold_),
        buffer_(new Entry[kRadkePrimes[capacity_idx_]]),
        states_(new State[kRadkePrimes[capacity_idx_]]) {
    std::copy(&other.buffer_[0], &other.buffer_[kRadkePrimes[capacity_idx_]],
              &buffer_[0]);
    std::copy(&other.states_[0], &other.states_[kRadkePrimes[capacity_idx_]],
              &states_[0]);
  }

  FlatSmallHashtable& operator=(FlatSmallHashtable&& other) = default;

  FlatSmallHashtable& operator=(const FlatSmallHashtable& other) {
    if (this != &other) {
      hash_fn_ = other.hash_fn_;
      key_fn_ = other.key_fn_;
      capacity_idx_ = other.capacity_idx_;
      used_ = other.used_;
      erased_ = other.erased_;
      resize_threshold_ = other.resize_threshold_;
      std::cout.flush();
      buffer_.reset(new Entry[kRadkePrimes[capacity_idx_]]);
      states_.reset(new State[kRadkePrimes[capacity_idx_]]);
      std::copy(&other.buffer_[0], &other.buffer_[kRadkePrimes[capacity_idx_]],
                &buffer_[0]);
      std::copy(&other.states_[0], &other.states_[kRadkePrimes[capacity_idx_]],
                &states_[0]);
      std::cout.flush();
    }
    return *this;
  }

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

  bool operator!=(const FlatSmallHashtable& other) { return !(*this == other); }

  uint16_t ht_len() const { return kRadkePrimes[capacity_idx_]; }

  ConstIterator begin() const {
    uint16_t cap = ht_len();
    uint16_t pos = 0;
    for (; pos < cap; ++pos) {
      if (states_[pos] == FULL) break;
    }
    return ConstIterator(this, pos);
  }

  Iterator begin() {
    uint16_t cap = ht_len();
    uint16_t pos = 0;
    for (; pos < cap; ++pos) {
      if (states_[pos] == FULL) break;
    }
    return Iterator(this, pos);
  }

  Iterator end() { return Iterator(this, ht_len()); }
  ConstIterator end() const { return ConstIterator(this, ht_len()); }

  // Returns the number of elements in the hashtable.
  uint16_t size() const { return used_ - erased_; }

  // Returns true if the hashtable has no elements.
  bool empty() const { return used_ == erased_; }

  // Returns the number of elements that the hashtable can hold before it needs
  // to be rehashed.
  uint16_t capacity() const { return resize_threshold_; }

  ConstIterator find(const Key& key) const {
    const uint16_t pos = fastmod(hash_fn_(key), capacity_idx_);
    if (states_[pos] == EMPTY) return end();
    if (states_[pos] == FULL && key_fn_(buffer_[pos]) == key) {
      return ConstIterator(this, pos);
    }
    const uint16_t cap = ht_len();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) return end();
      if (states_[p] == FULL && key_fn_(buffer_[p]) == key) {
        return ConstIterator(this, p);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

  bool erase(const Key& key) {
    const uint16_t pos = fastmod(hash_fn_(key), capacity_idx_);
    if (states_[pos] == EMPTY) return false;
    if (states_[pos] == FULL && key_fn_(buffer_[pos]) == key) {
      states_[pos] = DELETED;
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
      if (states_[p] == FULL && key_fn_(buffer_[p]) == key) {
        states_[p] = DELETED;
        buffer_[p] = Entry();
        ++erased_;
        return true;
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

  bool erase(const ConstIterator& itr) {
    return erase(key_fn_(*itr));
  }

  void clear() {
    if (used_ == 0 && erased_ == 0) return;
    std::fill(&states_[0], &states_[kRadkePrimes[capacity_idx_]], EMPTY);
    std::fill(&buffer_[0], &buffer_[kRadkePrimes[capacity_idx_]], Entry());
    used_ = 0;
    erased_ = 0;
  }

  void compact() {
    int capacity_idx = initialCapacityIdx(size());
    if (capacity_idx == capacity_idx_ && erased_ == 0) return;
    assert(capacity_idx < 12);  // Or, exceeded maximum hashtable size.
    FlatSmallHashtable<Entry, Key, HashFn, KeyFn> newt(size(), hash_fn_,
                                                       key_fn_);
    for (const auto& e : *this) {
      newt.insert(e);
    }
    *this = std::move(newt);
  }

  bool contains(const Key& key) const { return find(key).pos_ != ht_len(); }

  // Returns {the iterator to the new element, true} if the element was
  // successfuly inserted; {the iterator to an existing element, false} if an
  // entry with the same key has already been in the hashmap.
  std::pair<Iterator, bool> insert(Entry val) {
    Key key = key_fn_(val);
    uint16_t pos = fastmod(hash_fn_(key), capacity_idx_);
    // Fast path.
    if (states_[pos] == FULL &&
        (buffer_[pos] == val || key_fn_(buffer_[pos]) == key)) {
      return std::make_pair(Iterator(this, pos), false);
    }
    if (used_ >= resize_threshold_) {
      // Before rehashing see if maybe the entry is already in the hashtable.
      Iterator itr = lookup(key);
      if (itr != end()) {
        return std::make_pair(itr, false);
      }
      // Need to rehash.
      assert(capacity_idx_ < 12);  // Or, exceeded maximum hashtable size.
      FlatSmallHashtable<Entry, Key, HashFn, KeyFn> newt(ht_len(), hash_fn_,
                                                         key_fn_);
      for (const auto& e : *this) {
        newt.insert(e);
      }
      *this = std::move(newt);
      // Retry the fast path.
      pos = fastmod(hash_fn_(key), capacity_idx_);
      if (states_[pos] == FULL &&
          (buffer_[pos] == val || key_fn_(buffer_[pos]) == key)) {
        return std::make_pair(Iterator(this, pos), false);
      }
    }
    // Fast path for not found.
    if (states_[pos] == EMPTY) {
      states_[pos] = FULL;
      buffer_[pos] = std::move(val);
      ++used_;
      return std::make_pair(Iterator(this, pos), true);
    }
    const uint16_t cap = ht_len();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) {
        // We can insert here.
        states_[p] = FULL;
        buffer_[p] = std::move(val);
        ++used_;
        return std::make_pair(Iterator(this, p), true);
      }
      if (states_[p] == FULL &&
          (buffer_[p] == val || key_fn_(buffer_[p]) == key)) {
        return std::make_pair(Iterator(this, p), false);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

 protected:
  Iterator lookup(const Key& key) {
    const uint16_t pos = fastmod(hash_fn_(key), capacity_idx_);
    if (states_[pos] == EMPTY) return end();
    if (states_[pos] == FULL && key_fn_(buffer_[pos]) == key) {
      return Iterator(this, pos);
    }
    const uint16_t cap = ht_len();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) return end();
      if (states_[p] == FULL && key_fn_(buffer_[p]) == key) {
        return Iterator(this, p);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

 private:
  friend class ConstIterator;
  friend class Iterator;

  HashFn hash_fn_;
  KeyFn key_fn_;
  int capacity_idx_;
  uint16_t used_;
  uint16_t erased_;
  uint16_t resize_threshold_;
  std::unique_ptr<Entry[]> buffer_;
  std::unique_ptr<State[]> states_;
};

}  // namespace roo_collections

#ifdef ARDUINO

#include "WString.h"

namespace roo_collections {

// A simple specialization for the Arduino String.
template <>
struct DefaultHashFn<String> {
  size_t operator()(const String& s) const noexcept {
    size_t h = 0;
    unsigned int size = s.length();
    const char* data = s.c_str();
    while (size-- > 0) {
      h = ((h << 5) + h) ^ *data++;
    }
    return h;
  }
};

}  // namespace roo_collections

#endif
