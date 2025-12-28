#pragma once

#include <assert.h>
#include <inttypes.h>

#include <functional>
#include <initializer_list>
#include <memory>

#include "roo_backport.h"
#include "roo_backport/string_view.h"
#include "roo_collections/hash.h"
#include "roo_collections/small_string.h"

#ifdef ARDUINO
#include <WString.h>

// Adding coparators that migt be missing on some platforms (notably RPI 2040).

inline bool operator==(const ::String& a, roo::string_view b) {
  return roo::string_view(a.c_str(), a.length()) == b;
}

inline bool operator==(roo::string_view a, const String& b) {
  return (b == a);
}

#endif

namespace roo_collections {

// Slightly higher than conventional 0.7, mostly so that the default-capacity
// small hashtable (with ht_len 11) can hold 8 elements.
static constexpr float kMaxFillRatio = 0.73;

// Sequence of the largest primes of the format 4n+3, less than 2^k,
// for k = 2 ... 16. When used as hash map capacities, they are known to
// enable quadratic residue search to visit the entire array. Additionally,
// we keep the value '1' at the beginning, as the sentinel that indicates
// an empty hashtable, with no dynamically allocated buffer.
static constexpr uint16_t kRadkePrimes[] = {
    0x1,   0x3,   0x7,   0xb,   0x1f,   0x3b,   0x7f,   0xfb,
    0x1f7, 0x3fb, 0x7f7, 0xffb, 0x1fff, 0x3feb, 0x7fcf, 0xffef};

// These are precalculated (2^48 - 1) / the corresponding radke prime + 1.
// See
// https://lemire.me/blog/2019/02/08/faster-remainders-when-the-divisor-is-a-constant-beating-compilers-and-libdivide/
static constexpr uint64_t kRadkePrimeInverts[] = {
    0x281474976700001, 0xD5C26DD2255556, 0x5B9C78357DB6DC, 0x1745d1745d18,
    0x84210842109,     0x456c797dd4a,    0x20408102041,    0x105197f7d74,
    0x824a4e60b4,      0x4050647d9e,     0x202428adc4,     0x100501907e,
    0x800400201,       0x401506e65,      0x200c44b25,      0x100110122};

// Returns n % kRadkePrimes[idx].
inline uint16_t fastmod(uint32_t n, int idx) {
  uint64_t lowbits = (kRadkePrimeInverts[idx] * n) & 0x0000FFFFFFFFFFFF;
  return (lowbits * kRadkePrimes[idx]) >> 48;
}

inline int initialCapacityIdx(uint16_t size_hint) {
  uint32_t ht_len = (uint32_t)(((float)size_hint) / kMaxFillRatio) + 1;
  for (int radkeIdx = 0; radkeIdx < 15; ++radkeIdx) {
    if (kRadkePrimes[radkeIdx] >= ht_len) return radkeIdx;
  }
  return 15;
}

template <typename Key>
struct DefaultHashFn : public std::hash<Key> {};

template <>
struct DefaultHashFn<::roo::string_view> {
  inline size_t operator()(::roo::string_view val) const {
    return murmur3_32(val.data(), val.size(), 0x92F4E42BUL);
  }
};

template <>
struct DefaultHashFn<std::string> {
  inline size_t operator()(const std::string& val) const {
    return DefaultHashFn<::roo::string_view>()(::roo::string_view(val));
  }
};

template <>
struct DefaultHashFn<const char*> {
  inline size_t operator()(const char* val) const {
    return DefaultHashFn<::roo::string_view>()(::roo::string_view(val));
  }
};

template <size_t N>
struct DefaultHashFn<SmallString<N>> {
  size_t operator()(const SmallString<N>& str) const {
    return DefaultHashFn<::roo::string_view>()(str);
  }
};

#ifdef ARDUINO
template <>
struct DefaultHashFn<::String> {
  size_t operator()(::String val) const {
    return DefaultHashFn<::roo::string_view>()(
        ::roo::string_view(val.c_str(), val.length()));
  }
};
#endif

struct TransparentStringHashFn {
  // Required to denote a transparent hash.
  using is_transparent = void;

  // Hash operations required to be consistent:
  // a == b => hash(a) == hash(b).

  inline size_t operator()(const std::string& val) const {
    return DefaultHashFn<std::string>()(val);
  }
  inline size_t operator()(const char* val) const {
    return DefaultHashFn<const char*>()(val);
  }
  inline size_t operator()(::roo::string_view val) const {
    return DefaultHashFn<::roo::string_view>()(val);
  }
  template <size_t N>
  inline size_t operator()(const SmallString<N>& val) const {
    return DefaultHashFn<::roo::string_view>()(val);
  }

#ifdef ARDUINO
  inline size_t operator()(::String val) const {
    return DefaultHashFn<::String>()(val);
  }
#endif
};

struct TransparentEq {
  // Required to denote a transparent comparator.
  using is_transparent = void;

  template <typename X, typename Y>
  inline bool operator()(const X& x, const Y& y) const {
    return x == y;
  }
};

template <typename _Func, typename _SfinaeType, typename = std::void_t<>>
struct has_is_transparent {};

template <typename _Func, typename _SfinaeType>
struct has_is_transparent<_Func, _SfinaeType,
                          std::void_t<typename _Func::is_transparent>> {
  typedef void type;
};

template <typename _Func, typename _SfinaeType>
using has_is_transparent_t =
    typename has_is_transparent<_Func, _SfinaeType>::type;

// For maps, where Key == Entry.
template <typename Entry>
struct DefaultKeyFn {
  const Entry& operator()(const Entry& entry) const { return entry; }
};

// Memory-conscious small flat hashtable. It can hold up to 64000 elements.
template <typename Entry, typename Key, typename HashFn = DefaultHashFn<Key>,
          typename KeyFn = DefaultKeyFn<Entry>,
          typename KeyCmpFn = std::equal_to<Key>>
class FlatSmallHashtable {
 public:
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
      } while (pos_ < ht_len && ht_->states_[pos_] >= 0);
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

    ConstIterator(
        const FlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn>* ht,
        uint16_t pos)
        : ht_(ht), pos_(pos) {}

    const FlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn>* ht_;
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
      } while (pos_ < ht_len && ht_->states_[pos_] >= 0);
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
    uint16_t pos_;
  };

  using key_type = Key;
  using value_type = Entry;
  using hasher = HashFn;
  using key_equal = KeyCmpFn;
  using iterator = Iterator;
  using const_iterator = ConstIterator;

  template <typename InputIt>
  FlatSmallHashtable(InputIt first, InputIt last, HashFn hash_fn = HashFn(),
                     KeyFn key_fn = KeyFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : FlatSmallHashtable(std::max((int)(last - first), 8), hash_fn, key_fn,
                           key_cmp_fn) {
    for (auto it = first; it != last; ++it) {
      insert(*it);
    }
  }

  FlatSmallHashtable(std::initializer_list<Entry> init,
                     HashFn hash_fn = HashFn(), KeyFn key_fn = KeyFn(),
                     KeyCmpFn key_cmp_fn = KeyCmpFn())
      : FlatSmallHashtable(init.begin(), init.end(), hash_fn, key_fn,
                           key_cmp_fn) {}

  FlatSmallHashtable(HashFn hash_fn = HashFn(), KeyFn key_fn = KeyFn(),
                     KeyCmpFn key_cmp_fn = KeyCmpFn())
      : FlatSmallHashtable(8, hash_fn, key_fn, key_cmp_fn) {}

  FlatSmallHashtable(uint16_t size_hint, HashFn hash_fn = HashFn(),
                     KeyFn key_fn = KeyFn(), KeyCmpFn key_cmp_fn = KeyCmpFn())
      : hash_fn_(hash_fn),
        key_fn_(key_fn),
        key_cmp_fn_(key_cmp_fn),
        capacity_idx_(initialCapacityIdx(size_hint)),
        used_(0),
        erased_(0),
        resize_threshold_(
            capacity_idx_ == 15
                ? 64000
                : (uint16_t)(((float)kRadkePrimes[capacity_idx_]) *
                             kMaxFillRatio)),
        buffer_(capacity_idx_ > 0 ? new Entry[kRadkePrimes[capacity_idx_]]
                                  : nullptr),
        states_(capacity_idx_ > 0 ? new State[kRadkePrimes[capacity_idx_]]
                                  : &dummy_empty_state_) {
    std::fill(&states_[0], &states_[kRadkePrimes[capacity_idx_]], EMPTY);
  }

  FlatSmallHashtable(FlatSmallHashtable&& other)
      : hash_fn_(std::move(other.hash_fn_)),
        key_fn_(std::move(other.key_fn_)),
        key_cmp_fn_(std::move(other.key_cmp_fn_)),
        capacity_idx_(other.capacity_idx_),
        used_(other.used_),
        erased_(other.erased_),
        resize_threshold_(other.resize_threshold_),
        buffer_(other.buffer_),
        states_(capacity_idx_ > 0 ? other.states_ : &dummy_empty_state_) {
    other.capacity_idx_ = 0;
    other.used_ = 0;
    other.erased_ = 0;
    other.buffer_ = nullptr;
    other.states_ = &dummy_empty_state_;
  }

  FlatSmallHashtable(const FlatSmallHashtable& other)
      : hash_fn_(other.hash_fn_),
        key_fn_(other.key_fn_),
        key_cmp_fn_(other.key_cmp_fn_),
        capacity_idx_(other.capacity_idx_),
        used_(other.used_),
        erased_(other.erased_),
        resize_threshold_(other.resize_threshold_),
        buffer_(capacity_idx_ > 0 ? new Entry[kRadkePrimes[capacity_idx_]]
                                  : nullptr),
        states_(capacity_idx_ > 0 ? new State[kRadkePrimes[capacity_idx_]]
                                  : &dummy_empty_state_) {
    if (other.buffer_ != nullptr) {
      std::copy(&other.buffer_[0], &other.buffer_[kRadkePrimes[capacity_idx_]],
                &buffer_[0]);
    }
    std::copy(&other.states_[0], &other.states_[kRadkePrimes[capacity_idx_]],
              &states_[0]);
  }

  ~FlatSmallHashtable() {
    if (capacity_idx_ > 0) delete[] states_;
    delete[] buffer_;
  }

  FlatSmallHashtable& operator=(FlatSmallHashtable&& other) {
    if (this != &other) {
      if (capacity_idx_ > 0) delete[] states_;
      delete[] buffer_;
      hash_fn_ = std::move(other.hash_fn_);
      key_fn_ = std::move(other.key_fn_);
      key_cmp_fn_ = std::move(other.key_cmp_fn_);
      capacity_idx_ = other.capacity_idx_;
      used_ = other.used_;
      erased_ = other.erased_;
      resize_threshold_ = other.resize_threshold_;
      if (capacity_idx_ > 0) {
        buffer_ = other.buffer_;
        states_ = other.states_;
        other.capacity_idx_ = 0;
        other.used_ = 0;
        other.erased_ = 0;
        other.buffer_ = nullptr;
        other.states_ = &dummy_empty_state_;
      } else {
        buffer_ = nullptr;
        states_ = &dummy_empty_state_;
      }
    }
    return *this;
  }

  FlatSmallHashtable& operator=(const FlatSmallHashtable& other) {
    if (this != &other) {
      if (capacity_idx_ > 0) delete[] states_;
      delete[] buffer_;
      hash_fn_ = other.hash_fn_;
      key_fn_ = other.key_fn_;
      key_cmp_fn_ = other.key_cmp_fn_;
      capacity_idx_ = other.capacity_idx_;
      used_ = other.used_;
      erased_ = other.erased_;
      resize_threshold_ = other.resize_threshold_;
      buffer_ =
          capacity_idx_ > 0 ? new Entry[kRadkePrimes[capacity_idx_]] : nullptr;
      states_ = capacity_idx_ > 0 ? new State[kRadkePrimes[capacity_idx_]]
                                  : &dummy_empty_state_;
      if (other.buffer_ != nullptr) {
        std::copy(&other.buffer_[0],
                  &other.buffer_[kRadkePrimes[capacity_idx_]], &buffer_[0]);
      }
      std::copy(&other.states_[0], &other.states_[kRadkePrimes[capacity_idx_]],
                &states_[0]);
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
      if (states_[pos] < 0) break;
    }
    return ConstIterator(this, pos);
  }

  Iterator begin() {
    uint16_t cap = ht_len();
    uint16_t pos = 0;
    for (; pos < cap; ++pos) {
      if (states_[pos] < 0) break;
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
    size_t hash = hash_fn_(key);
    const uint16_t pos = fastmod(hash, capacity_idx_);
    if (states_[pos] == EMPTY) return end();
    if (states_[pos] < 0 && (states_[pos] & 0x7F) == (hash & 0x7F) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
      return ConstIterator(this, pos);
    }
    const uint16_t cap = ht_len();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) return end();
      if (states_[p] < 0 && (states_[p] & 0x7F) == (hash & 0x7F) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
        return ConstIterator(this, p);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  ConstIterator find(const K& key) const {
    size_t hash = hash_fn_(key);
    const uint16_t pos = fastmod(hash, capacity_idx_);
    if (states_[pos] == EMPTY) return end();
    if (states_[pos] < 0 && (states_[pos] & 0x7F) == (hash & 0x7F) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
      return ConstIterator(this, pos);
    }
    const uint16_t cap = ht_len();
    uint32_t p = pos;
    p += (cap - 2);
    int32_t j = 2 - cap;
    while (true) {
      if (p >= cap) p -= cap;
      if (states_[p] == EMPTY) return end();
      if (states_[p] < 0 && (states_[p] & 0x7F) == (hash & 0x7F) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
        return ConstIterator(this, p);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

  bool erase(const Key& key) {
    size_t hash = hash_fn_(key);
    const uint16_t pos = fastmod(hash, capacity_idx_);
    if (states_[pos] == EMPTY) return false;
    if (states_[pos] < 0 && (states_[pos] & 0x7F) == (hash & 0x7F) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
      buffer_[pos] = Entry();
      if (used_ == 1 && erased_ == 0) {
        // Fast path (fast-clear). It is safe to do because there was no
        // rehashing. (It only works when used_ == 1, because otherwise the
        // other items might have been rehashed away from this bucket).
        states_[pos] = EMPTY;
        --used_;
      } else {
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
      if (states_[p] < 0 && (states_[p] & 0x7F) == (hash & 0x7F) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
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

  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  bool erase(const K& key) {
    size_t hash = hash_fn_(key);
    const uint16_t pos = fastmod(hash, capacity_idx_);
    if (states_[pos] == EMPTY) return false;
    if (states_[pos] < 0 && (states_[pos] & 0x7F) == (hash & 0x7F) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
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
      if (states_[p] < 0 && (states_[p] & 0x7F) == (hash & 0x7F) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
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

  Iterator erase(const ConstIterator& itr) {
    if (itr == end()) return end();
    Iterator next(this, itr.pos_);
    ++next;
    erase(key_fn_(*itr));
    return next;
  }

  void clear() {
    if (used_ == 0 && erased_ == 0) return;
    std::fill(&states_[0], &states_[kRadkePrimes[capacity_idx_]], EMPTY);
    for (size_t i = 0; i < kRadkePrimes[capacity_idx_]; ++i) {
      buffer_[i] = Entry();
    }
    // Avoiding std::fill, because it doesn't work for non-copyable entries.
    // std::fill(&buffer_[0], &buffer_[kRadkePrimes[capacity_idx_]], Entry());
    used_ = 0;
    erased_ = 0;
  }

  void compact() {
    int capacity_idx = initialCapacityIdx(size());
    if (capacity_idx == capacity_idx_ && erased_ == 0) return;
    assert(capacity_idx < 15);  // Or, exceeded maximum hashtable size.
    FlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn> newt(
        size(), hash_fn_, key_fn_, key_cmp_fn_);
    for (auto& e : *this) {
      newt.insert(std::move(e));
    }
    *this = std::move(newt);
  }

  bool contains(const Key& key) const { return find(key).pos_ != ht_len(); }

  template <typename K, typename = has_is_transparent_t<HashFn, K>,
            typename = has_is_transparent_t<KeyCmpFn, K>>
  bool contains(const K& key) const {
    return find(key).pos_ != ht_len();
  }

  // Returns {the iterator to the new element, true} if the element was
  // successfuly inserted; {the iterator to an existing element, false} if an
  // entry with the same key has already been in the hashmap.
  std::pair<Iterator, bool> insert(Entry val) {
    Key key = key_fn_(val);
    size_t hash = hash_fn_(key);
    uint16_t pos = fastmod(hash, capacity_idx_);
    // Fast path.
    if (states_[pos] < 0 && (states_[pos] & 0x7F) == (hash & 0x7F) &&
        key_cmp_fn_(key_fn_(buffer_[pos]), key)) {
      return std::make_pair(Iterator(this, pos), false);
    }
    if (used_ >= resize_threshold_) {
      if (empty() && erased_ > 0) {
        // Clearing is faster than rehashing.
        clear();
      } else {
        // Before rehashing see if maybe the entry is already in the hashtable.
        Iterator itr = lookup(key);
        if (itr != end()) {
          return std::make_pair(itr, false);
        }
        // Need to rehash.
        FlatSmallHashtable<Entry, Key, HashFn, KeyFn, KeyCmpFn> newt(
            size() + 1, hash_fn_, key_fn_, key_cmp_fn_);
        // Check if we didn't exceed the maximum hashtable size.
        assert(newt.capacity() >= size() + 1);
        for (auto& e : *this) {
          newt.insert(std::move(e));
        }
        if (newt.capacity() == capacity()) {
          // In this case, prefer to reuse the old storage, and release the new
          // storage, because doing otherwise thrashes the heap. (Experimentally
          // observed on ESP32).
          used_ = newt.used_;
          erased_ = 0;
          memcpy(states_, newt.states_,
                 kRadkePrimes[capacity_idx_] * sizeof(State));
          for (size_t i = 0; i < kRadkePrimes[capacity_idx_]; ++i) {
            buffer_[i] = std::move(newt.buffer_[i]);
          }
        } else {
          *this = std::move(newt);
        }
      }
      pos = fastmod(hash, capacity_idx_);
    }
    // Fast path for not found.
    if (states_[pos] == EMPTY) {
      states_[pos] = (hash & 0x7F) | 0x80;
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
        states_[p] = (hash & 0x7F) | 0x80;
        buffer_[p] = std::move(val);
        ++used_;
        return std::make_pair(Iterator(this, p), true);
      }
      if (states_[p] < 0 && (states_[p] & 0x7F) == (hash & 0x7F) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
        return std::make_pair(Iterator(this, p), false);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

 protected:
  Iterator lookup(const Key& key) {
    size_t hash = hash_fn_(key);
    const uint16_t pos = fastmod(hash, capacity_idx_);
    if (states_[pos] == EMPTY) return end();
    if (states_[pos] < 0 && (states_[pos] & 0x7F) == (hash & 0x7F) &&
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
      if (states_[p] < 0 && (states_[p] & 0x7F) == (hash & 0x7F) &&
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
    const uint16_t pos = fastmod(hash, capacity_idx_);
    if (states_[pos] == EMPTY) return end();
    if (states_[pos] < 0 && (states_[pos] & 0x7F) == (hash & 0x7F) &&
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
      if (states_[p] < 0 && (states_[p] & 0x7F) == (hash & 0x7F) &&
          key_cmp_fn_(key_fn_(buffer_[p]), key)) {
        return Iterator(this, p);
      }
      j += 2;
      assert(j < cap);
      p += (j >= 0 ? j : -j);
    }
  }

 private:
  using State = int8_t;
  static constexpr State EMPTY = 0;
  static constexpr State DELETED = 1;
  // Full items are marked with a bit pattern of the form 0x80 + (hash & 0x7F).

  friend class ConstIterator;
  friend class Iterator;

  HashFn hash_fn_;
  KeyFn key_fn_;
  KeyCmpFn key_cmp_fn_;
  int capacity_idx_;
  uint16_t used_;
  uint16_t erased_;
  uint16_t resize_threshold_;
  Entry* buffer_;
  State* states_;
  State dummy_empty_state_;
};

}  // namespace roo_collections
