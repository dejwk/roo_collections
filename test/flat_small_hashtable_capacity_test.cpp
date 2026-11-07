#include <stdint.h>

#include <algorithm>
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

// Verifies explicit size hints save buckets for both maps and sets without
// changing default construction or allowing an early rehash before the hint.
TEST(FlatSmallHashtableCapacity, SizeHintsUseDenseCapacities) {
  FlatSmallHashMap<int, int> map(100);
  const ReadOnlyFlatSmallHashMap<int, int>& base = map;
  EXPECT_EQ(map.ht_len(), 151);
  EXPECT_EQ(map.capacity(), 110);
  for (int key = 0; key < 100; ++key) map[key] = key * 10;
  EXPECT_EQ(base.ht_len(), 151);
  EXPECT_EQ(base.at(99), 990);
  FlatSmallHashSet<int> set(1000);
  EXPECT_EQ(set.ht_len(), 1447);
  EXPECT_EQ(set.capacity(), 1056);
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
  for (int key = 0; key < 110; ++key) map[key] = key * 10;
  EXPECT_EQ(map.ht_len(), 151);
  EXPECT_FALSE(map.insert({1, 999}).second);
  EXPECT_EQ(map.at(1), 10);
  EXPECT_EQ(map.ht_len(), 151);
  map[110] = 1100;
  EXPECT_EQ(map.ht_len(), 359);
  EXPECT_EQ(map.capacity(), 262);
  for (int key = 0; key <= 110; ++key) EXPECT_EQ(map.at(key), key * 10);
  FlatSmallHashSet<int> set(16);
  for (int key = 0; key < 16; ++key) set.insert(key);
  EXPECT_EQ(set.ht_len(), 23);
  set.insert(16);
  EXPECT_EQ(set.ht_len(), 47);
  EXPECT_EQ(set.capacity(), 34);
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
  for (int key = 0; key < 110; ++key) map[key] = key * 10;
  for (int key = 0; key < 50; ++key) ASSERT_TRUE(map.erase(key));
  const int* address = &map.at(100);
  map[200] = 2000;
  EXPECT_EQ(map.ht_len(), 151);
  EXPECT_EQ(map.capacity(), 110);
  EXPECT_EQ(&map.at(100), address);
  for (int key = 50; key < 110; ++key) EXPECT_EQ(map.at(key), key * 10);
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
  EXPECT_EQ(base.capacity(), 110);
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

}  // namespace
}  // namespace roo_collections
