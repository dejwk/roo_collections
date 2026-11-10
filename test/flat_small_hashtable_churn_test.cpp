#include <stdint.h>

#include <iterator>
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

struct CopyCountedKey {
  static int copies;
  uint32_t id = 0;
  CopyCountedKey() = default;
  explicit CopyCountedKey(uint32_t value) : id(value) {}
  CopyCountedKey(const CopyCountedKey& other) : id(other.id) { ++copies; }
  CopyCountedKey(CopyCountedKey&&) = default;
  CopyCountedKey& operator=(const CopyCountedKey&) = default;
  CopyCountedKey& operator=(CopyCountedKey&&) = default;
  bool operator==(const CopyCountedKey& other) const { return id == other.id; }
};
int CopyCountedKey::copies = 0;

struct CountedKeyHash {
  size_t operator()(const CopyCountedKey& key) const { return key.id; }
};

// Verifies insertion borrows its owned argument's key instead of introducing
// a second owning-key copy, including across growth and duplicate detection.
TEST(FlatSmallHashtableChurn, InsertDoesNotCopyTheOwnedKey) {
  FlatSmallHashMap<CopyCountedKey, int, CountedKeyHash> map(1);
  for (unsigned key = 0; key < 20; ++key) {
    std::pair<CopyCountedKey, int> entry{CopyCountedKey(key), int(key)};
    CopyCountedKey::copies = 0;
    ASSERT_TRUE(map.insert(std::move(entry)).second);
    EXPECT_EQ(CopyCountedKey::copies, 0);
  }
  std::pair<CopyCountedKey, int> duplicate{CopyCountedKey(19), 999};
  CopyCountedKey::copies = 0;
  EXPECT_FALSE(map.insert(std::move(duplicate)).second);
  EXPECT_EQ(CopyCountedKey::copies, 0);
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
struct FingerprintHash {
  using is_transparent = void;
  uint32_t value;

  template <typename K>
  size_t operator()(const K&) const {
    return value;
  }
};

struct TransparentEqual {
  using is_transparent = void;

  template <typename A, typename B>
  bool operator()(const A& a, const B& b) const {
    return a == b;
  }
};

// Verifies every low hash byte, including 0 and 1, remains distinct from empty
// and deleted states, with collisions and typed/heterogeneous lookup and erase.
TEST(FlatSmallHashtableChurn, OccupiedFingerprintByte) {
  using Map = FlatSmallHashMap<int, int, FingerprintHash, TransparentEqual>;
  for (uint32_t hash = 0; hash < 256; ++hash) {
    SCOPED_TRACE(hash);
    Map map(8, FingerprintHash{hash});
    const Map::read_only_type& view = map;
    EXPECT_EQ(map.find(0), map.end());
    EXPECT_EQ(view.find(0L), view.end());
    for (int key = 0; key < 8; ++key)
      ASSERT_TRUE(map.insert({key, key}).second);
    ASSERT_TRUE(map.erase(0));
    ASSERT_TRUE(map.erase(1L));
    EXPECT_FALSE(map.erase(0L));
    EXPECT_FALSE(map.erase(1));
    EXPECT_EQ(map.find(0), map.end());
    EXPECT_EQ(view.find(1L), view.end());
    EXPECT_FALSE(map.insert({7, 99}).second);
    ASSERT_TRUE(map.insert({8, 8}).second);
    ASSERT_TRUE(map.insert({9, 9}).second);
    for (int key = 2; key < 10; ++key) {
      ASSERT_NE(map.find(key), map.end());
      ASSERT_NE(map.find(static_cast<long>(key)), map.end());
      ASSERT_NE(view.find(key), view.end());
      ASSERT_NE(view.find(static_cast<long>(key)), view.end());
      EXPECT_EQ(map.at(key), key);
      EXPECT_EQ(view.at(static_cast<long>(key)), key);
    }
    EXPECT_EQ(std::distance(view.begin(), view.end()), 8);
    // A fresh table exercises heterogeneous erase at the primary bucket too.
    Map singleton(1, FingerprintHash{hash});
    singleton.insert({1, 1});
    ASSERT_TRUE(singleton.erase(1L));
    EXPECT_EQ(singleton.begin(), singleton.end());
    EXPECT_EQ(singleton.find(1), singleton.end());
  }
}

}  // namespace
}  // namespace roo_collections
