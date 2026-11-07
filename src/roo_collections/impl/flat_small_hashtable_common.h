#pragma once

/// @file
/// @brief Shared capacity arithmetic and hash/key policies for flat tables.

#include <assert.h>
#include <inttypes.h>

#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

#include "roo_backport.h"
#include "roo_backport/string_view.h"
#include "roo_collections/hash.h"
#include "roo_collections/impl/config.h"
#include "roo_collections/small_string.h"

#ifdef ARDUINO
#include <WString.h>

// Adding coparators that migt be missing on some platforms (notably RPI 2040).

inline bool operator==(const ::String& a, roo::string_view b) {
  return roo::string_view(a.c_str(), a.length()) == b;
}

inline bool operator==(roo::string_view a, const String& b) { return (b == a); }

#endif

namespace roo_collections {

// Slightly higher than conventional 0.7, mostly so that the default-capacity
// small hashtable (with ht_len 11) can hold 8 elements.
static constexpr float kMaxFillRatio = 0.73;

namespace impl {

/// Shares a bucket count and its precomputed reducer with read-only clients.
/// Owners keep only a pointer to this immutable descriptor. The bucket count
/// must be nonzero; one denotes the empty-table sentinel.
struct FlatSmallHashtableCapacity {
  /// Precomputes the 48-bit reciprocal for @p slots.
  constexpr explicit FlatSmallHashtableCapacity(uint16_t slots)
      : reciprocal(((uint64_t{1} << 48) - 1) / slots + 1), slot_count(slots) {}

  /// Reduces a 32-bit hash without division, including the one-slot sentinel.
  ROO_COLLECTIONS_CONSTEXPR14 uint16_t reduceHash(uint32_t hash) const {
    uint64_t lowbits = (reciprocal * hash) & 0x0000FFFFFFFFFFFF;
    return (lowbits * slot_count) >> 48;
  }

  /// Returns the mutable entry limit, retaining the final-capacity exception.
  constexpr uint16_t mutableCapacity() const {
    return slot_count == 65519 ? 64000 : uint32_t(slot_count) * 73u / 100u;
  }

  uint64_t reciprocal;
  uint16_t slot_count;
};

/// Keeps one shared capacity table across translation units, including C++11.
/// Template static members provide shared storage before inline variables are
/// available, and expand each prime into its compile-time reciprocal once.
template <uint16_t... Slots>
struct FlatSmallHashtableCapacityTable {
  static constexpr uint16_t kPrimes[] = {Slots...};

  static constexpr FlatSmallHashtableCapacity kCapacities[] = {
      FlatSmallHashtableCapacity(Slots)...};
};

template <uint16_t... Slots>
constexpr uint16_t FlatSmallHashtableCapacityTable<Slots...>::kPrimes[];

template <uint16_t... Slots>
constexpr FlatSmallHashtableCapacity
    FlatSmallHashtableCapacityTable<Slots...>::kCapacities[];

// Largest primes congruent to 3 modulo 4 strictly below 2^(k/4), for k = 1..64.
// Repeated small primes are omitted; one is the no-allocation empty sentinel.
// Immutable tables continue to select their exact prime independently.
using MutableFlatSmallHashtableCapacityTable = FlatSmallHashtableCapacityTable<
    1, 3, 7, 11, 19, 23, 31, 43, 47, 59, 71, 83, 107, 127, 151, 179, 211, 251,
    283, 359, 419, 503, 607, 719, 859, 1019, 1187, 1447, 1699, 2039, 2423, 2887,
    3407, 4091, 4831, 5791, 6883, 8191, 9739, 11579, 13763, 16363, 19483, 23167,
    27551, 32719, 38959, 46327, 55103, 65519>;

static constexpr const auto& kMutableFlatSmallHashtableCapacities =
    MutableFlatSmallHashtableCapacityTable::kCapacities;

}  // namespace impl

static constexpr const auto& kRadkePrimes =
    impl::MutableFlatSmallHashtableCapacityTable::kPrimes;

static constexpr int kRadkePrimeCount =
    sizeof(kRadkePrimes) / sizeof(kRadkePrimes[0]);

// Returns n % kRadkePrimes[idx] using the shared precomputed reciprocal.
ROO_COLLECTIONS_CONSTEXPR14 uint16_t fastmod(uint32_t n, int idx) {
  return impl::kMutableFlatSmallHashtableCapacities[idx].reduceHash(n);
}

// Finds the tightest candidate meeting the 73% target, clamping to the final
// high-occupancy capacity for large hints. All intermediate arithmetic is wide.
ROO_COLLECTIONS_CONSTEXPR14 int initialCapacityIdx(uint16_t size_hint) {
  uint32_t required = (100u * size_hint + 72u) / 73u;
  int first = 0;
  int last = kRadkePrimeCount - 1;
  while (first < last) {
    int middle = first + (last - first) / 2;
    if (kRadkePrimes[middle] < required) {
      first = middle + 1;
    } else {
      last = middle;
    }
  }
  return first;
}

template <typename InputIt>
inline uint16_t initialCapacityHint(InputIt first, InputIt last,
                                    std::input_iterator_tag) {
  return 8;
}

template <typename ForwardIt>
inline uint16_t initialCapacityHint(ForwardIt first, ForwardIt last,
                                    std::forward_iterator_tag) {
  size_t count = std::distance(first, last);
  if (count > 65535) count = 65535;
  if (count < 8) count = 8;
  return count;
}

template <typename InputIt>
inline uint16_t initialCapacityHint(InputIt first, InputIt last) {
  return initialCapacityHint(
      first, last, typename std::iterator_traits<InputIt>::iterator_category());
}

/// Default hash policy; integral and enum keys use Murmur3's fmix32 mixer.
/// Integral and enum keys support constant evaluation in C++14+.
template <typename Key, typename Enable = void>
struct DefaultHashFn : public std::hash<Key> {};

/// Mixes integral and enum keys into a 32-bit hash, independent of size_t
/// width. Keys up to 32 bits convert to uint32_t before mixing. For wider keys,
/// the mixed upper 32 bits are XORed into the lower word before final mixing.
/// These hashes intentionally differ from std::hash.
template <typename Key>
struct DefaultHashFn<Key,
                     typename std::enable_if<std::is_integral<Key>::value ||
                                             std::is_enum<Key>::value>::type> {
  /// Mixes @p key, including the upper word of 64-bit integral/enum keys.
  ROO_COLLECTIONS_CONSTEXPR14 size_t operator()(Key key) const noexcept {
    uint32_t value = static_cast<uint32_t>(key);
    if (sizeof(Key) > sizeof(uint32_t)) {
      value ^= Murmur3Fmix32(
          static_cast<uint32_t>(static_cast<uint64_t>(key) >> 32));
    }
    return Murmur3Fmix32(value);
  }
};

template <>
struct DefaultHashFn<::roo::string_view> {
  ROO_COLLECTIONS_CONSTEXPR14 size_t operator()(::roo::string_view val) const {
    return Murmur3_32(val.data(), val.size(), 0x92F4E42BUL);
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
  ROO_COLLECTIONS_CONSTEXPR14 size_t operator()(const char* val) const {
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
  ROO_COLLECTIONS_CONSTEXPR14 size_t operator()(const char* val) const {
    return DefaultHashFn<const char*>()(val);
  }
  ROO_COLLECTIONS_CONSTEXPR14 size_t operator()(::roo::string_view val) const {
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

  /// Compares string content directly, avoiding implicit Arduino String copies.
  ROO_COLLECTIONS_CONSTEXPR14 bool operator()(::roo::string_view x,
                                              const char* y) const {
    return x == ::roo::string_view(y);
  }

  /// Compares C strings to views without implicit owning-string conversions.
  ROO_COLLECTIONS_CONSTEXPR14 bool operator()(const char* x,
                                              ::roo::string_view y) const {
    return ::roo::string_view(x) == y;
  }

  template <typename X, typename Y>
  constexpr bool operator()(const X& x, const Y& y) const {
    return x == y;
  }
};

namespace internal {

// std::void_t is available only from C++17 onward.
template <typename...>
using VoidT = void;

}  // namespace internal

template <typename _Func, typename _SfinaeType, typename = void>
struct has_is_transparent {};

template <typename _Func, typename _SfinaeType>
struct has_is_transparent<_Func, _SfinaeType,
                          internal::VoidT<typename _Func::is_transparent>> {
  typedef void type;
};

template <typename _Func, typename _SfinaeType>
using has_is_transparent_t =
    typename has_is_transparent<_Func, _SfinaeType>::type;

// For maps, where Key == Entry.
template <typename Entry>
struct DefaultKeyFn {
  constexpr const Entry& operator()(const Entry& entry) const { return entry; }
};

/// Extracts the lookup key from a map entry.
template <typename Key, typename Value>
struct MapKeyFn {
  /// Returns the entry's key without copying it.
  constexpr const Key& operator()(const std::pair<Key, Value>& entry) const {
    return entry.first;
  }
};

}  // namespace roo_collections
