#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "gtest/gtest.h"
#include "roo_collections/flat_small_hash_map.h"
#include "roo_collections/flat_small_hash_set.h"

namespace roo_collections {
namespace {
struct CountingHash {
  size_t* calls;
  size_t operator()(uint32_t key) const {
    ++*calls;
    return DefaultHashFn<uint32_t>()(key);
  }
};

struct ConstantHash {
  size_t operator()(int) const { return 0; }
};

struct IdentityHash {
  size_t operator()(int key) const { return key; }
};

// Verifies recycling erased positions avoids migrating unrelated entries or
// allocating temporary tables even when the live-entry limit was reached.
TEST(FlatSmallHashtableChurn, ReinsertSameKeysWithoutRehashing) {
  for (unsigned n : {2u, 4u, 8u, 16u, 32u, 64u}) {
    size_t calls = 0;
    FlatSmallHashMap<uint32_t, uint32_t, CountingHash> map(
        8, CountingHash{&calls});
    for (unsigned key = 0; key < n; ++key) map.insert({key, key + 1});
    unsigned slots = map.ht_len();
    calls = 0;
    for (unsigned i = 0; i < 1024; ++i) {
      unsigned key = i % n;
      ASSERT_TRUE(map.erase(key));
      ASSERT_TRUE(map.insert({key, key + 1}).second);
    }
    EXPECT_EQ(map.size(), n);
    EXPECT_EQ(map.ht_len(), slots);
    EXPECT_LE(calls, 3u * 1024) << n;
    for (unsigned key = 0; key < n; ++key) EXPECT_EQ(map.at(key), key + 1);
  }
}

// Verifies a full small map can churn through new keys without rebuilding on
// every mutation, while retaining its size, capacity and lookup correctness.
TEST(FlatSmallHashtableChurn, NewKeysHaveAmortizedTombstoneCleanup) {
  for (unsigned n : {8u, 16u}) {
    size_t calls = 0;
    FlatSmallHashMap<uint32_t, uint32_t, CountingHash> map(
        8, CountingHash{&calls});
    for (unsigned key = 0; key < n; ++key) map.insert({key, key});
    unsigned slots = map.ht_len();
    calls = 0;
    for (unsigned i = 0; i < 1024; ++i) {
      ASSERT_TRUE(map.erase(i));
      ASSERT_TRUE(map.insert({i + n, i + n}).second);
      ASSERT_EQ(map.size(), n);
      ASSERT_FALSE(map.contains(i));
    }
    EXPECT_EQ(map.ht_len(), slots);
    // Includes the extra missing-key check on every operation.
    EXPECT_LT(calls, 9u * 1024) << n;
    for (unsigned key = 1024; key < 1024 + n; ++key)
      EXPECT_EQ(map.at(key), key);
  }
}

// Verifies a tombstone preceding a colliding duplicate is not mistaken for
// absence, and reusing it preserves every other key and move-only value.
TEST(FlatSmallHashtableChurn, DuplicateBeyondTombstone) {
  FlatSmallHashMap<int, std::unique_ptr<int>, ConstantHash> map(8);
  for (int key = 0; key < 8; ++key) {
    map.insert({key, std::unique_ptr<int>(new int(key + 10))});
  }
  ASSERT_TRUE(map.erase(0));
  auto duplicate = map.insert({7, std::unique_ptr<int>(new int(999))});
  EXPECT_FALSE(duplicate.second);
  EXPECT_EQ(*duplicate.first->second, 17);
  EXPECT_EQ(map.size(), 7);
  EXPECT_TRUE(map.insert({8, std::unique_ptr<int>(new int(18))}).second);
  EXPECT_EQ(map.ht_len(), 11);
  const auto& base = static_cast<const decltype(map)::read_only_type&>(map);
  for (int key = 1; key <= 8; ++key) EXPECT_EQ(*base.at(key), key + 10);
  map.insert({9, std::unique_ptr<int>(new int(19))});
  EXPECT_GT(map.ht_len(), 11);
  for (int key = 1; key <= 9; ++key) EXPECT_EQ(*map.at(key), key + 10);
}

// Verifies hostile collision chains keep terminating through repeated erasure,
// tombstone reuse, new-key insertion, compaction and growth.
TEST(FlatSmallHashtableChurn, CollisionChurnMatchesReference) {
  FlatSmallHashMap<int, int, ConstantHash> map(16);
  std::unordered_map<int, int> reference;
  for (int i = 0; i < 1500; ++i) {
    int removed = (i * 7) % 47;
    EXPECT_EQ(map.erase(removed), reference.erase(removed) != 0);
    int key = (i * 11) % 47;
    EXPECT_EQ(map.insert({key, i}).second, reference.insert({key, i}).second);
    EXPECT_EQ(map.size(), reference.size());
    const auto& base = static_cast<const decltype(map)::read_only_type&>(map);
    for (int query = 0; query < 48; ++query) {
      auto expected = reference.find(query);
      auto found = base.find(query);
      ASSERT_EQ(found == base.end(), expected == reference.end());
      if (expected != reference.end()) {
        EXPECT_EQ(found->second, expected->second);
      }
    }
    if (i % 101 == 0) map.compact();
  }
}

// Verifies iterators still observe storage changes that do not
// rehash, skip tombstones, preserve owner identity, and handle empty sentinels.
TEST(FlatSmallHashtableChurn, IterationAcrossInPlaceMutations) {
  FlatSmallHashMap<int, int, IdentityHash> map(8);
  map.insert({1, 10});
  map.insert({7, 70});
  auto it = map.begin();
  const auto& base = static_cast<const decltype(map)::read_only_type&>(map);
  typename decltype(map)::const_iterator const_it = it;
  EXPECT_EQ(const_it, base.begin());
  map.insert({4, 40});
  ++it;
  ++const_it;
  EXPECT_EQ(it->first, 4);
  EXPECT_EQ(const_it->first, 4);
  ASSERT_TRUE(map.erase(7));
  ++it;
  ++const_it;
  EXPECT_EQ(it, map.end());
  EXPECT_EQ(const_it, base.end());
  auto first = map.begin();
  auto next = map.erase(typename decltype(map)::const_iterator(first));
  ASSERT_NE(next, map.end());
  EXPECT_EQ(next->first, 4);
  FlatSmallHashMap<int, int, IdentityHash> other(0);
  EXPECT_NE(other.end(), map.end());
  auto moved = std::move(map);
  EXPECT_EQ(map.begin(), map.end());
  EXPECT_EQ(base.begin(), base.end());
  EXPECT_EQ(moved.begin()->first, 4);
  EXPECT_EQ(typename decltype(map)::const_iterator(),
            typename decltype(map)::const_iterator());
}
}  // namespace
}  // namespace roo_collections
