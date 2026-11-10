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

}  // namespace
