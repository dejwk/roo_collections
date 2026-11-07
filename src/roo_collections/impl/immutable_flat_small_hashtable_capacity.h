#pragma once

/// @file
/// @brief Compile-time bucket selection for immutable flat tables.

#include "roo_collections/impl/flat_small_hashtable_common.h"

namespace roo_collections {
namespace impl {

/// Tests primality across the full 16-bit range using trial division.
constexpr bool IsPrime16(uint16_t n) {
  if (n < 2) return false;
  if ((n & 1u) == 0) return n == 2;
  for (uint32_t divisor = 3; divisor * divisor <= n; divisor += 2) {
    if (n % divisor == 0) return false;
  }
  return true;
}

/// Finds the smallest 16-bit prime congruent to 3 modulo 4 at least @p minimum.
/// Returns zero if none exists. Widened arithmetic prevents search wraparound.
constexpr uint16_t NextRadkePrime16(uint32_t minimum) {
  for (uint32_t prime = minimum | 3u; prime <= 65535u; prime += 4) {
    if (IsPrime16(static_cast<uint16_t>(prime))) {
      return static_cast<uint16_t>(prime);
    }
  }
  return 0;
}

/// Selects fixed storage from the entry count without runtime prime search.
/// Targets at most 75% occupancy. Counts above 49139 deliberately retain the
/// existing 64000-entry exception in 65519 buckets; empty tables use one slot.
template <size_t N>
struct ImmutableFlatSmallHashtableCapacity {
  static_assert(N <= 64000, "Immutable table exceeds maximum supported size");
  static constexpr uint32_t kRequiredSlots = (4u * uint32_t(N) + 2u) / 3u;
  static constexpr uint16_t kMaximumSlots = 65519;
  static constexpr uint16_t kSlotCount =
      N == 0
          ? 1
          : NextRadkePrime16(kRequiredSlots > kMaximumSlots ? kMaximumSlots
                                                            : kRequiredSlots);
  static_assert(kSlotCount != 0, "Required capacity exceeds 16-bit range");
  static_assert(kSlotCount > N, "Immutable tables require an empty bucket");
  static constexpr FlatSmallHashtableCapacity kValue{kSlotCount};
};

}  // namespace impl
}  // namespace roo_collections
