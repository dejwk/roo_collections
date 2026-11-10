#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "roo_collections/flat_small_hash_map.h"
#include "roo_collections/flat_small_hash_set.h"

namespace roo_collections {
namespace {

// Verifies the candidates are exactly the distinct largest Radke primes below
// each requested quarter-power boundary, using an independent primality sieve.
TEST(FlatSmallHashtableCapacity, QuarterPowerCandidates) {
  std::vector<bool> prime(65536, true);
  prime[0] = false;
  prime[1] = false;
  for (uint32_t divisor = 2; divisor * divisor < prime.size(); ++divisor) {
    if (!prime[divisor]) continue;
    for (uint32_t multiple = divisor * divisor; multiple < prime.size();
         multiple += divisor) {
      prime[multiple] = false;
    }
  }
  std::vector<uint16_t> expected{1};
  for (int k = 1; k <= 64; ++k) {
    uint32_t first = 0;
    uint32_t last = 65536;
    while (first + 1 < last) {
      uint32_t middle = first + (last - first) / 2;
      uint64_t square = uint64_t(middle) * middle;
      // For 16-bit candidates the fourth power fits uint64_t; every candidate
      // is below 2^(64/4), so avoid shifting by 64 at the final boundary.
      if (k == 64 || square * square < (uint64_t{1} << k)) {
        first = middle;
      } else {
        last = middle;
      }
    }
    int candidate = static_cast<int>(first);
    candidate -= (candidate + 1) % 4;
    while (candidate >= 3 && !prime[candidate]) candidate -= 4;
    if (candidate >= 3 && candidate != expected.back()) {
      expected.push_back(candidate);
    }
  }
  ASSERT_EQ(expected.size(), static_cast<size_t>(kRadkePrimeCount));
  for (int idx = 0; idx < kRadkePrimeCount; ++idx) {
    EXPECT_EQ(kRadkePrimes[idx], expected[idx]) << idx;
    EXPECT_EQ(impl::kMutableFlatSmallHashtableCapacities[idx].slot_count,
              expected[idx]);
  }
}

// Verifies every possible hint selects the smallest sufficient candidate,
// including the sentinel, saturation, and the final high-occupancy exception.
TEST(FlatSmallHashtableCapacity, EveryHintSelectsTightestCapacity) {
  int previous = 0;
  for (uint32_t hint = 0; hint <= UINT16_MAX; ++hint) {
    int idx = initialCapacityIdx(hint);
    ASSERT_GE(idx, previous);
    ASSERT_LT(idx, kRadkePrimeCount);
    const auto& capacity = impl::kMutableFlatSmallHashtableCapacities[idx];
    ASSERT_GE(capacity.mutableCapacity(), std::min<uint32_t>(hint, 64000))
        << hint;
    if (idx > 0) {
      EXPECT_LT(
          impl::kMutableFlatSmallHashtableCapacities[idx - 1].mutableCapacity(),
          hint);
    }
    previous = idx;
  }
}

// Verifies the live-entry limit is the greatest count within three-quarter
// occupancy for every candidate, with the existing maximum-size exception.
TEST(FlatSmallHashtableCapacity, ThreeQuarterEntryLimits) {
  EXPECT_FLOAT_EQ(kMaxFillRatio, 0.75f);
  for (const auto& capacity : impl::kMutableFlatSmallHashtableCapacities) {
    uint32_t slots = capacity.slot_count;
    uint32_t limit = capacity.mutableCapacity();
    if (slots == 65519) {
      EXPECT_EQ(limit, 64000u);
    } else {
      EXPECT_LE(4u * limit, 3u * slots) << slots;
      EXPECT_GT(4u * (limit + 1), 3u * slots) << slots;
    }
  }
}

// Verifies 32 entries fit in 43 buckets at the new load limit, and only a new
// distinct entry beyond that limit triggers growth.
TEST(FlatSmallHashtableCapacity, ThreeQuarterBoundary) {
  FlatSmallHashMap<int, int> map(32);
  ASSERT_EQ(map.ht_len(), 43);
  ASSERT_EQ(map.capacity(), 32);
  for (int key = 0; key < 32; ++key) map[key] = key * 10;
  EXPECT_EQ(map.ht_len(), 43);
  EXPECT_FALSE(map.insert({31, 999}).second);
  EXPECT_EQ(map.ht_len(), 43);
  map[32] = 320;
  EXPECT_EQ(map.ht_len(), 107);
  EXPECT_EQ(map.capacity(), 80);
  for (int key = 0; key <= 32; ++key) EXPECT_EQ(map.at(key), key * 10);
}

// Verifies explicit size hints save buckets for both maps and sets without
// changing default construction or allowing an early rehash before the hint.
TEST(FlatSmallHashtableCapacity, SizeHintsUseDenseCapacities) {
  FlatSmallHashMap<int, int> map(100);
  const ReadOnlyFlatSmallHashMap<int, int>& base = map;
  EXPECT_EQ(map.ht_len(), 151);
  EXPECT_EQ(map.capacity(), 113);
  for (int key = 0; key < 100; ++key) map[key] = key * 10;
  EXPECT_EQ(base.ht_len(), 151);
  EXPECT_EQ(base.at(99), 990);
  FlatSmallHashSet<int> set(1000);
  EXPECT_EQ(set.ht_len(), 1447);
  EXPECT_EQ(set.capacity(), 1085);
  for (int key = 0; key < 1000; ++key) set.insert(key);
  EXPECT_EQ(set.ht_len(), 1447);
  FlatSmallHashMap<int, int> default_map;
  EXPECT_EQ(default_map.ht_len(), 11);
  EXPECT_EQ(default_map.capacity(), 8);
}

// Verifies forward-range construction also uses its known count to select a
// tight starting point and preserves every supplied mapping.
TEST(FlatSmallHashtableCapacity, KnownRangesUseDenseCapacities) {
  std::vector<std::pair<int, int>> entries;
  for (int key = 0; key < 100; ++key) entries.emplace_back(key, key * 10);
  FlatSmallHashMap<int, int> map(entries.begin(), entries.end());
  EXPECT_EQ(map.ht_len(), 151);
  for (const auto& entry : entries)
    EXPECT_EQ(map.at(entry.first), entry.second);
}

// Verifies insertion grows from a tight starting point by doubling the usable
// capacity before rounding, and duplicates in a full table do not resize it.
TEST(FlatSmallHashtableCapacity, AutomaticGrowthDoublesUsableCapacity) {
  FlatSmallHashMap<int, int> map(100);
  for (int key = 0; key < 113; ++key) map[key] = key * 10;
  EXPECT_EQ(map.ht_len(), 151);
  EXPECT_FALSE(map.insert({1, 999}).second);
  EXPECT_EQ(map.at(1), 10);
  EXPECT_EQ(map.ht_len(), 151);
  map[113] = 1130;
  EXPECT_EQ(map.ht_len(), 359);
  EXPECT_EQ(map.capacity(), 269);
  for (int key = 0; key <= 113; ++key) EXPECT_EQ(map.at(key), key * 10);
  FlatSmallHashSet<int> set(16);
  for (int key = 0; key < 17; ++key) set.insert(key);
  EXPECT_EQ(set.ht_len(), 23);
  set.insert(17);
  EXPECT_EQ(set.ht_len(), 47);
  EXPECT_EQ(set.capacity(), 35);
}

// Verifies doubling does not overflow when crossing the 16-bit midpoint or
// final capacity, and compaction at high occupancy accepts tombstones.
TEST(FlatSmallHashtableCapacity, GrowthAndCompactionAtMaximumSize) {
  FlatSmallHashSet<int> set(0);
  EXPECT_EQ(set.ht_len(), 1);
  uint32_t previous_capacity = 0;
  for (int key = 0; key < 64000; ++key) {
    set.insert(key);
    uint32_t capacity = set.capacity();
    if (capacity != previous_capacity) {
      EXPECT_GE(capacity, std::min<uint32_t>(64000, 2u * previous_capacity));
      previous_capacity = capacity;
    }
  }
  EXPECT_EQ(set.capacity(), 64000);
  EXPECT_EQ(set.ht_len(), 65519);
  EXPECT_FALSE(set.insert(63999).second);
  EXPECT_TRUE(set.erase(12345));
  set.compact();
  EXPECT_EQ(set.size(), 63999);
  EXPECT_EQ(set.ht_len(), 65519);
  EXPECT_FALSE(set.contains(12345));
  EXPECT_TRUE(set.contains(63999));
  uint64_t sum = 0;
  for (int key : set) sum += key;
  EXPECT_EQ(sum, uint64_t{64000} * 63999 / 2 - 12345);
}

struct IdentityHash {
  constexpr uint32_t operator()(int key) const { return key; }
};

// Verifies automatic tombstone reclamation keeps both capacity and allocation
// when erased entries leave enough room, instead of growing or shrinking.
TEST(FlatSmallHashtableCapacity, TombstoneCleanupRetainsStorage) {
  FlatSmallHashMap<int, int, IdentityHash> map(100);
  for (int key = 0; key < 113; ++key) map[key] = key * 10;
  for (int key = 0; key < 50; ++key) ASSERT_TRUE(map.erase(key));
  const int* address = &map.at(100);
  map[200] = 2000;
  EXPECT_EQ(map.ht_len(), 151);
  EXPECT_EQ(map.capacity(), 113);
  EXPECT_EQ(&map.at(100), address);
  for (int key = 50; key < 113; ++key) EXPECT_EQ(map.at(key), key * 10);
  EXPECT_EQ(map.at(200), 2000);
  EXPECT_FALSE(map.contains(0));
}

// Verifies explicit compaction shrinks to a dense candidate, updates live base
// references and iteration, and avoids reallocating an already compact table.
TEST(FlatSmallHashtableCapacity, ExplicitCompactionSelectsTightestCapacity) {
  FlatSmallHashMap<int, int> map(1000);
  const ReadOnlyFlatSmallHashMap<int, int>& base = map;
  for (int key = 0; key < 200; ++key) map[key] = key * 10;
  for (int key = 0; key < 100; ++key) ASSERT_TRUE(map.erase(key));
  EXPECT_EQ(base.ht_len(), 1447);
  map.compact();
  EXPECT_EQ(base.ht_len(), 151);
  EXPECT_EQ(base.capacity(), 113);
  int sum = 0;
  for (const auto& entry : base) {
    EXPECT_EQ(entry.second, entry.first * 10);
    sum += entry.first;
  }
  EXPECT_EQ(sum, 14950);
  const int* address = &map.at(150);
  map.compact();
  EXPECT_EQ(&map.at(150), address);
  FlatSmallHashSet<int> empty(100);
  empty.compact();
  EXPECT_EQ(empty.ht_len(), 1);
  EXPECT_EQ(empty.capacity(), 0);
  EXPECT_EQ(empty.begin(), empty.end());
  empty.insert(1);
  EXPECT_TRUE(empty.contains(1));
}

// Verifies the cached bucket count follows ownership changes, growth and
// compaction, including the allocation-free moved-from sentinel.
TEST(FlatSmallHashtableCapacity, CachedCountFollowsStorage) {
  using Map = FlatSmallHashMap<uint32_t, uint32_t>;
  for (uint16_t hint : {0, 1, 8, 128, 64000}) {
    Map map(hint);
    map.insert({7, 12});
    const uint16_t slots = map.ht_len();
    Map copy(map);
    EXPECT_EQ(copy.ht_len(), slots);
    Map assigned;
    assigned = copy;
    EXPECT_EQ(assigned.ht_len(), slots);
    Map moved(std::move(copy));
    EXPECT_EQ(moved.ht_len(), slots);
    EXPECT_EQ(copy.ht_len(), 1);
    EXPECT_EQ(copy.begin(), copy.end());
    assigned = std::move(moved);
    EXPECT_EQ(assigned.ht_len(), slots);
    EXPECT_EQ(moved.ht_len(), 1);
    assigned.compact();
    EXPECT_EQ(assigned.ht_len(), 3);
    EXPECT_EQ(assigned.at(7), 12u);
    for (uint32_t i = 0; i < 200; ++i) assigned.insert({i, i});
    const Map::read_only_type& view = assigned;
    EXPECT_EQ(std::distance(view.begin(), view.end()), 200);
    EXPECT_EQ(view.ht_len(), assigned.ht_len());
    Map empty(0);
    assigned = empty;
    EXPECT_EQ(assigned.ht_len(), 1);
    EXPECT_EQ(assigned.begin(), assigned.end());
    assigned.insert({0, 1});
    EXPECT_EQ(assigned.at(0), 1u);
  }
}

}  // namespace
}  // namespace roo_collections
