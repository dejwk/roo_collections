#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "roo_collections/flat_small_hash_map.h"
#include "roo_collections/immutable_flat_small_hash_map.h"

namespace {

struct IdentityHash {
  constexpr uint32_t operator()(uint32_t value) const { return value; }
};

using Map = roo_collections::FlatSmallHashMap<uint32_t, uint32_t, IdentityHash>;
using View = Map::read_only_type;

// Verifies empty, cleared and moved-from tables have matching begin/end values
// and that mutable-to-const conversion preserves the owner's end identity.
TEST(FlatSmallHashtableIterator, EmptyAndOwnerIdentity) {
  for (uint16_t hint : {0, 1, 8, 128, 64000}) {
    Map map(hint);
    Map other(hint);
    const View& view = map;
    EXPECT_EQ(map.begin(), map.end());
    EXPECT_EQ(view.begin(), view.end());
    EXPECT_EQ(view.find(123), view.end());
    EXPECT_EQ(Map::const_iterator(map.end()), view.end());
    EXPECT_NE(view.end(), static_cast<const View&>(other).end());
    map.insert({0, 10});
    map.clear();
    EXPECT_EQ(map.begin(), map.end());
    EXPECT_EQ(view.begin(), view.end());
    Map moved(std::move(map));
    EXPECT_EQ(map.begin(), map.end());
    EXPECT_EQ(view.find(123), view.end());
  }
}

// Verifies increment reaches a canonical end after the last physical bucket or
// trailing empty buckets, including the largest supported 16-bit bucket count.
TEST(FlatSmallHashtableIterator, SparseBoundariesAndPostincrement) {
  for (uint16_t hint : {1, 8, 128, 64000}) {
    Map map(hint);
    uint32_t last = map.ht_len() - 1;
    map.insert({last, 17});
    const View& view = map;
    Map::iterator it = map.begin();
    ASSERT_NE(it, map.end());
    EXPECT_EQ(it->first, last);
    EXPECT_EQ((it++)->second, 17u);
    EXPECT_EQ(it, map.end());
    auto cit = view.find(last);
    EXPECT_EQ((cit++)->second, 17u);
    EXPECT_EQ(cit, view.end());
    EXPECT_TRUE(map.erase(last));
    map.insert({0, 23});
    cit = view.begin();
    EXPECT_EQ((cit++)->second, 23u);
    EXPECT_EQ(cit, view.end());
    EXPECT_EQ(std::distance(map.begin(), map.end()), 1);
    EXPECT_EQ(std::distance(view.begin(), view.end()), 1);
  }
}

// Verifies erase-by-const-iterator returns the next live bucket while skipping
// tombstones, and retained iterators observe in-place insertion and erasure.
TEST(FlatSmallHashtableIterator, MutationWithoutRebuild) {
  Map map(8);
  map.insert({0, 10});
  map.insert({4, 40});
  map.insert({8, 80});
  const View& view = map;
  auto retained = map.begin();
  auto retained_const = view.begin();
  map.insert({6, 60});
  ASSERT_TRUE(map.erase(4));
  EXPECT_EQ((++retained)->first, 6u);
  EXPECT_EQ((++retained_const)->first, 6u);
  EXPECT_EQ(Map::const_iterator(retained), retained_const);
  auto next = map.erase(retained_const);
  ASSERT_NE(next, map.end());
  EXPECT_EQ(next->first, 8u);
  next = map.erase(next);
  EXPECT_EQ(next, map.end());
  EXPECT_EQ(map.erase(map.end()), map.end());
  EXPECT_EQ(view.find(6), view.end());
  EXPECT_EQ(view.find(8), view.end());
  EXPECT_EQ(view.find(0)->second, 10u);
}

constexpr roo_collections::ImmutableFlatSmallHashMap kFixed(
    roo_collections::WithMapping<uint32_t, uint32_t>({{0, 10}, {4, 40}}));

constexpr bool CheckConstantIteration() {
  const roo_collections::ReadOnlyFlatSmallHashMap<uint32_t, uint32_t>& view =
      kFixed;
  auto it = view.begin();
  uint32_t sum = 0;
  while (it != view.end()) sum += (it++)->second;
  return sum == 50 && view.find(123) == view.end() &&
         view.find(4)->second == 40;
}

static_assert(CheckConstantIteration(), "shared read-only constexpr iteration");

// Checks observable traversal through the shared read-only interface.
void ExpectKeys(const View& view, const std::vector<uint32_t>& expected) {
  std::vector<uint32_t> actual;
  for (const auto& entry : view) actual.push_back(entry.first);
  EXPECT_EQ(actual, expected);
  EXPECT_EQ(view.size(), expected.size());
  for (uint32_t key : expected) EXPECT_TRUE(view.contains(key));
}

// Verifies every first-eight occupancy pattern survives tombstones, copying,
// moving and clearing, and scanning continues correctly beyond the bitmap.
TEST(FlatSmallHashtableIterator, BitmapPatternsAndOwnership) {
  for (unsigned pattern = 0; pattern < 256; ++pattern) {
    SCOPED_TRACE(pattern);
    Map map(16);
    for (uint32_t key = 0; key < 8; ++key) map.insert({key, key});
    map.insert({8, 8});
    const uint32_t last = map.ht_len() - 1;
    map.insert({last, last});
    std::vector<uint32_t> expected;
    for (uint32_t key = 0; key < 8; ++key) {
      if ((pattern & (1u << key)) != 0) {
        expected.push_back(key);
      } else {
        ASSERT_TRUE(map.erase(key));
      }
    }
    expected.push_back(8);
    expected.push_back(last);
    ExpectKeys(map, expected);
    Map copy(map);
    ExpectKeys(copy, expected);
    Map assigned(0);
    assigned = copy;
    ExpectKeys(assigned, expected);
    Map moved(std::move(copy));
    ExpectKeys(moved, expected);
    ExpectKeys(copy, {});
    assigned = std::move(moved);
    ExpectKeys(assigned, expected);
    ExpectKeys(moved, {});
    assigned.clear();
    ExpectKeys(assigned, {});
    assigned.insert({7, 7});
    ExpectKeys(assigned, {7});
    ASSERT_TRUE(assigned.erase(7));
    ExpectKeys(assigned, {});
  }
}

// Verifies bucket reuse and same-capacity tombstone cleanup keep iteration in
// sync while the live keys move across the bitmap boundary.
TEST(FlatSmallHashtableIterator, BitmapSurvivesTombstoneCleanup) {
  Map map(8);
  for (uint32_t key = 0; key < 6; ++key) map.insert({key, key});
  const uint16_t slots = map.ht_len();
  for (uint32_t first = 0; first < 500; ++first) {
    ASSERT_TRUE(map.erase(first));
    ASSERT_TRUE(map.insert({first + 6, first + 6}).second);
    EXPECT_EQ(map.ht_len(), slots);
    uint32_t sum = 0;
    size_t count = 0;
    for (const auto& entry : map) {
      EXPECT_GE(entry.first, first + 1);
      EXPECT_LE(entry.first, first + 6);
      sum += entry.first;
      ++count;
    }
    EXPECT_EQ(count, 6u);
    EXPECT_EQ(sum, 6 * first + 21);
  }
}

struct FixedIdentityHash {
  constexpr uint32_t operator()(uint32_t value) const { return value; }
};

constexpr roo_collections::ImmutableFlatSmallHashMap kBoundary(
    roo_collections::WithMapping<uint32_t, uint32_t>({{0, 10},
                                                      {1, 11},
                                                      {2, 12},
                                                      {3, 13},
                                                      {6, 16},
                                                      {7, 17},
                                                      {8, 18},
                                                      {10, 20}},
                                                     FixedIdentityHash{}));

// Verifies immutable rebinding rebuilds the bitmap in constant evaluation,
// including empty buckets below bit seven and occupied buckets beyond it.
constexpr bool CheckBoundaryIteration() {
  auto copy = kBoundary;
  auto assigned = kBoundary;
  assigned = copy;
  uint32_t keys = 0;
  size_t count = 0;
  for (const auto& entry : assigned) {
    keys |= 1u << entry.first;
    ++count;
  }
  return keys == 1487 && count == 8;
}

static_assert(CheckBoundaryIteration(),
              "constexpr bitmap and scalar traversal");

}  // namespace
