#include <array>
#include <iterator>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "roo_collections_immutable_flat_small_hashtable.h"

namespace roo_collections {
namespace {

struct Record {
  int key;
  int value;

  constexpr bool operator==(const Record& other) const {
    return key == other.key && value == other.value;
  }
};

struct RecordKey {
  constexpr const int& operator()(const Record& record) const {
    return record.key;
  }
};

struct ConstantHash {
  uint32_t hash;

  constexpr uint32_t operator()(int) const { return hash; }
};

static constexpr ImmutableFlatSmallHashtable kRecords(WithEntries<Record, int>(
    {{1, 10}, {2, 20}, {3, 30}, {4, 40}, {5, 50}, {6, 60}, {7, 70}, {8, 80}},
    ConstantHash{10}, RecordKey{}));
static constexpr ImmutableFlatSmallHashtable kEmpty(
    WithEntries(std::array<int, 0>{}));
static constexpr ImmutableFlatSmallHashtable<int, int, 0> kDefaultEmpty;
static constexpr ImmutableFlatSmallHashtable kSingleton(WithEntries<int>({-1}));

/// Exercises iteration, postfix increment, copying and lookup in constant
/// evaluation.
constexpr bool CheckConstexprTable() {
  auto copy = kRecords;
  int sum = 0;
  for (auto it = copy.begin(); it != copy.end();) {
    sum += (it++)->value;
  }
  return sum == 360 && copy.find(8)->value == 80 &&
         copy.find(9) == copy.end() && copy == kRecords;
}

static_assert(kRecords.size() == 8, "fixed size");
static_assert(kRecords.ht_len() == 11, "capacity based on fill ratio");
static_assert(kRecords.contains(8), "colliding key found");
static_assert(!kRecords.contains(9), "colliding missing key");
static_assert(kEmpty.empty() && kEmpty.begin() == kEmpty.end(),
              "empty iteration");
static_assert(kDefaultEmpty.find(0) == kDefaultEmpty.end(), "empty lookup");
static_assert(kSingleton.contains(-1), "negative integral key");
static_assert(CheckConstexprTable(),
              "constexpr collision handling and iteration");
static_assert(
    std::is_same<decltype(*std::declval<decltype(kRecords)&>().begin()),
                 const Record&>::value,
    "entries remain immutable");

// Verifies custom extraction, stateful hashing, wraparound probing and misses
// work on runtime lookup keys as well as in constant evaluation.
TEST(ImmutableFlatSmallHashtable, CollisionsAndCustomKeyExtraction) {
  for (int key = 1; key <= 8; ++key) {
    auto it = kRecords.find(key);
    ASSERT_NE(it, kRecords.end());
    EXPECT_EQ(it->value, key * 10);
  }
  int missing = 99;
  EXPECT_FALSE(kRecords.contains(missing));
  EXPECT_EQ(kRecords.capacity(), 8);
  EXPECT_EQ(std::distance(kRecords.cbegin(), kRecords.cend()), 8);
}

// Verifies returning a copy retains its own storage and only const access is
// exposed even when the table object itself is non-const.
TEST(ImmutableFlatSmallHashtable, CopiesOwnTheirArrays) {
  auto copy = kRecords;
  auto assigned = kRecords;
  assigned = copy;
  EXPECT_EQ(copy, kRecords);
  EXPECT_EQ(assigned, kRecords);
  EXPECT_NE(&*copy.find(1), &*kRecords.find(1));
  EXPECT_NE(&*assigned.find(1), &*copy.find(1));
  static_assert(std::is_same<decltype(*copy.begin()), const Record&>::value,
                "mutable object exposes const references");
}

// Verifies equality uses content rather than the order of colliding entries.
TEST(ImmutableFlatSmallHashtable, EqualityIgnoresInsertionOrder) {
  static constexpr ImmutableFlatSmallHashtable reversed(WithEntries<Record,
                                                                    int>(
      {{8, 80}, {7, 70}, {6, 60}, {5, 50}, {4, 40}, {3, 30}, {2, 20}, {1, 10}},
      ConstantHash{10}, RecordKey{}));
  static constexpr ImmutableFlatSmallHashtable changed(WithEntries<Record, int>(
      {{1, 11}, {2, 20}, {3, 30}, {4, 40}, {5, 50}, {6, 60}, {7, 70}, {8, 80}},
      ConstantHash{10}, RecordKey{}));
  static_assert(reversed == kRecords, "order independent equality");
  static_assert(changed != kRecords, "value comparison");
  EXPECT_EQ(reversed, kRecords);
  EXPECT_NE(changed, kRecords);
}

// Verifies the largest supported table does not truncate its count or bucket
// indexes and misses terminate at the high end of the key range.
TEST(ImmutableFlatSmallHashtable, MaximumSize) {
  std::array<int, 64000> entries{};
  for (size_t i = 0; i < entries.size(); ++i) entries[i] = i;
  auto table = MakeImmutableFlatSmallHashtable(entries);
  EXPECT_EQ(table.size(), 64000);
  EXPECT_EQ(table.ht_len(), 65519);
  EXPECT_TRUE(table.contains(0));
  EXPECT_TRUE(table.contains(63999));
  EXPECT_FALSE(table.contains(64000));
  EXPECT_FALSE(table.contains(-1));
}

// Verifies an empty default table and an empty array have identical semantics.
TEST(ImmutableFlatSmallHashtable, Empty) {
  EXPECT_EQ(kEmpty.size(), 0);
  EXPECT_EQ(kEmpty.capacity(), 0);
  EXPECT_EQ(kEmpty.ht_len(), 1);
  EXPECT_EQ(kEmpty, kDefaultEmpty);
  EXPECT_EQ(kEmpty.cbegin(), kEmpty.cend());
  EXPECT_FALSE(kEmpty.contains(0));
}

struct KeyQuery {
  int value;
};

struct TransparentHash {
  using is_transparent = void;

  constexpr size_t operator()(int key) const { return key; }

  constexpr size_t operator()(KeyQuery key) const { return key.value; }
};

struct TransparentEquality {
  using is_transparent = void;

  constexpr bool operator()(int stored, int query) const {
    return stored == query;
  }

  constexpr bool operator()(int stored, KeyQuery query) const {
    return stored == query.value;
  }
};

// Verifies transparent lookups accept a query type that cannot convert to Key.
TEST(ImmutableFlatSmallHashtable, HeterogeneousLookup) {
  static constexpr ImmutableFlatSmallHashtable table(
      WithEntries<Record, int>({{1, 10}, {2, 20}}, TransparentHash{},
                               RecordKey{}, TransparentEquality{}));
  static_assert(table.find(KeyQuery{2})->value == 20, "heterogeneous lookup");
  static_assert(!table.contains(KeyQuery{3}), "heterogeneous miss");
  EXPECT_EQ(table.find(KeyQuery{1})->value, 10);
  EXPECT_FALSE(table.contains(KeyQuery{3}));
}

// Verifies enum default hashing and both explicit fixed-size constructors.
TEST(ImmutableFlatSmallHashtable, EnumKeysAndDirectConstruction) {
  enum class Mode { kOff, kOn };
  constexpr Mode entries[] = {Mode::kOff, Mode::kOn};
  static constexpr ImmutableFlatSmallHashtable<Mode, Mode, 2> table(entries);
  static constexpr ImmutableFlatSmallHashtable<int, int, 2> integers(
      std::array<int, 2>{{-1, 42}});
  static_assert(table.contains(Mode::kOn), "enum hash");
  static_assert(integers.contains(-1) && integers.contains(42), "array input");
  EXPECT_EQ(table.size(), 2);
  EXPECT_FALSE(integers.contains(0));
}

// Verifies std::array helpers retain custom key extraction and stateful hash
// policies when deducing the table type and entry count.
TEST(ImmutableFlatSmallHashtable, PreparedArrayDeduction) {
  static constexpr ImmutableFlatSmallHashtable table(
      WithEntries<Record, int>(std::array<Record, 2>{{{1, 10}, {2, 20}}},
                               ConstantHash{10}, RecordKey{}));
  static_assert(table.size() == 2, "deduced entry count");
  static_assert(table.find(2)->value == 20, "deduced custom-key array");
  EXPECT_EQ(table.find(1)->value, 10);
  EXPECT_FALSE(table.contains(3));
}

// Verifies duplicate rejection also runs during runtime construction.
TEST(ImmutableFlatSmallHashtable, DuplicateKeysAbort) {
  EXPECT_DEATH((MakeImmutableFlatSmallHashtable<int>({1, 1})), "");
  EXPECT_DEATH((MakeImmutableFlatSmallHashtable<Record, int>(
                   {{1, 10}, {1, 20}}, ConstantHash{10}, RecordKey{})),
               "");
}

}  // namespace
}  // namespace roo_collections
