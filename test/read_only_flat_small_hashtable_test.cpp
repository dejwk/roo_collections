#include <array>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "roo_collections/flat_small_hash_map.h"
#include "roo_collections/flat_small_hash_set.h"
#include "roo_collections/immutable_flat_small_hash_map.h"
#include "roo_collections/immutable_flat_small_hash_set.h"
#include "roo_collections_read_only_flat_small_hash_map.h"
#include "roo_collections_read_only_flat_small_hash_set.h"
#include "roo_collections_read_only_flat_small_hashtable.h"

namespace roo_collections {
namespace {

using NumberMap = ReadOnlyFlatSmallHashMap<int, int>;
using NumberSet = ReadOnlyFlatSmallHashSet<int>;
using StringMap =
    ReadOnlyFlatSmallHashMap<::roo::string_view, int, TransparentStringHashFn,
                             TransparentEq>;

static constexpr ImmutableFlatSmallHashMap kNumbers(
    WithMapping<int, int>({{1, 10}, {2, 20}, {3, 30}}));
static constexpr ImmutableFlatSmallHashMap kNames(
    WithStringMapping<int>({{"on", 1}, {"off", 0}}));
static constexpr ImmutableFlatSmallHashtable kKeys(WithElements<int>({1, 2,
                                                                      3}));
static constexpr ImmutableFlatSmallHashtable<int, int, 0> kEmpty;

constexpr int SumValues(const NumberMap& map) {
  int sum = 0;
  for (const auto& entry : map) sum += entry.second;
  return sum;
}

static_assert(SumValues(kNumbers) == 60, "constexpr base iteration");
static_assert(static_cast<const NumberMap&>(kNumbers).at(2) == 20,
              "constexpr mapped access through the common base");
static_assert(static_cast<const StringMap&>(kNames)["on"] == 1,
              "constexpr transparent lookup through the common base");
static_assert(static_cast<const NumberSet&>(kKeys).contains(2), "set base");
static_assert(static_cast<const NumberSet&>(kEmpty).empty(), "empty base");
static_assert(!static_cast<const NumberSet&>(kEmpty).contains(1), "empty miss");
static_assert(std::is_base_of<NumberMap, FlatSmallHashMap<int, int>>::value,
              "mutable map shares the read-only base");
static_assert(std::is_base_of<NumberMap, decltype(kNumbers)>::value,
              "immutable map shares the read-only base");
static_assert(std::is_same<ImmutableHashFn<int>, DefaultHashFn<int>>::value,
              "default hash policies have identical types");
static_assert(!std::is_polymorphic<NumberMap>::value, "no runtime dispatch");
static_assert(!std::is_copy_constructible<NumberMap>::value, "no base slicing");
static_assert(!std::is_copy_assignable<NumberMap>::value, "no base assignment");
static_assert(!std::is_destructible<NumberMap>::value,
              "owner-only destruction");
static_assert(std::is_same<decltype(kNumbers.at(1)), const int&>::value,
              "immutable access stays const");

// Verifies the same non-template client function accepts different owner types
// and sizes, with const mapped access and iteration.
TEST(ReadOnlyFlatSmallHashtable, AcceptsBothOwners) {
  FlatSmallHashMap<int, int> dynamic({{1, 10}, {2, 20}, {3, 30}});
  const NumberMap* first = &dynamic;
  const NumberMap* second = &kNumbers;
  EXPECT_EQ(SumValues(*first), 60);
  EXPECT_EQ(SumValues(*second), 60);
  EXPECT_EQ(*first, *second);
  static_assert(std::is_same<decltype(*first->begin()),
                             const std::pair<int, int>&>::value,
                "base iteration exposes const entries");
  EXPECT_EQ(std::distance(first->cbegin(), first->cend()), 3);
  const NumberMap& small = dynamic;
  EXPECT_EQ(small[1], 10);
  EXPECT_EQ(small.find(99), small.end());
}

// Verifies an existing reference observes live storage and count updates after
// growth, deletion, compaction and clearing without refreshing a descriptor.
TEST(ReadOnlyFlatSmallHashtable, ReferenceSurvivesStructuralUpdates) {
  FlatSmallHashMap<int, int> map(0);
  const NumberMap& reference = map;
  const NumberMap* address = &reference;
  EXPECT_TRUE(reference.empty());
  EXPECT_EQ(reference.begin(), reference.end());
  for (int i = 0; i < 400; ++i) map[i] = i * 10;
  EXPECT_EQ(&reference, address);
  EXPECT_EQ(reference.size(), 400);
  for (int i = 0; i < 400; ++i) EXPECT_EQ(reference.at(i), i * 10);
  map.at(5) = 99;
  EXPECT_EQ(reference.at(5), 99);
  for (int i = 0; i < 390; ++i) EXPECT_TRUE(map.erase(i));
  EXPECT_EQ(reference.size(), 10);
  EXPECT_FALSE(reference.contains(5));
  map.compact();
  EXPECT_EQ(reference.size(), 10);
  EXPECT_EQ(reference.at(399), 3990);
  map.clear();
  EXPECT_TRUE(reference.empty());
  EXPECT_EQ(reference.begin(), reference.end());
}

// Verifies copy/move operations retain destination references and reset source
// base state, including the no-allocation empty sentinel and source reuse.
TEST(ReadOnlyFlatSmallHashtable, MutableCopyMoveAndAssignment) {
  FlatSmallHashMap<int, int> source({{1, 10}, {2, 20}});
  const NumberMap& source_base = source;
  FlatSmallHashMap<int, int> copied(source);
  const NumberMap& copied_base = copied;
  EXPECT_NE(&copied_base.at(1), &source_base.at(1));
  FlatSmallHashMap<int, int> moved(std::move(source));
  const NumberMap& moved_base = moved;
  EXPECT_TRUE(source_base.empty());
  EXPECT_EQ(source_base.begin(), source_base.end());
  EXPECT_EQ(moved_base.at(2), 20);
  source[9] = 90;
  EXPECT_EQ(source_base.at(9), 90);
  copied = source;
  EXPECT_EQ(copied_base.size(), 1);
  EXPECT_EQ(copied_base.at(9), 90);
  copied = std::move(moved);
  EXPECT_EQ(copied_base.size(), 2);
  EXPECT_EQ(copied_base.at(1), 10);
  EXPECT_TRUE(moved_base.empty());
  FlatSmallHashMap<int, int> empty(0);
  copied = std::move(empty);
  EXPECT_TRUE(copied_base.empty());
  copied[7] = 70;
  EXPECT_EQ(copied_base.at(7), 70);
}

// Verifies immutable copy/move/assignment rebind the common base in constant
// evaluation, including addresses observed by an already-bound reference.
constexpr bool CheckImmutableSpecialMembers() {
  ImmutableFlatSmallHashMap a(
      WithMapping<int, int>({{1, 10}, {2, 20}, {3, 30}}));
  const NumberMap& a_base = a;
  auto copy = a;
  const NumberMap& copy_base = copy;
  if (&copy_base.at(1) == &a_base.at(1)) return false;
  auto moved = std::move(copy);
  const NumberMap& moved_base = moved;
  if (&moved_base.at(1) == &copy_base.at(1)) return false;
  ImmutableFlatSmallHashMap b(
      WithMapping<int, int>({{4, 40}, {5, 50}, {6, 60}}));
  a = b;
  if (a_base.contains(1) || a_base.at(4) != 40) return false;
  a = std::move(moved);
  return a_base.at(2) == 20 && &a_base.at(2) == &a.at(2) &&
         &copy_base.at(2) == &copy.at(2);
}

static_assert(CheckImmutableSpecialMembers(), "constexpr storage rebinding");

// Verifies the runtime common base and concrete fast path see the same arrays
// after copying, moving and assigning immutable maps.
TEST(ReadOnlyFlatSmallHashtable, ImmutableCopyMoveAndAssignment) {
  EXPECT_TRUE(CheckImmutableSpecialMembers());
  auto copy = kNumbers;
  const NumberMap& reference = copy;
  EXPECT_EQ(&reference.at(1), &copy.at(1));
  EXPECT_NE(&reference.at(1), &static_cast<const NumberMap&>(kNumbers).at(1));
  auto moved = std::move(copy);
  const NumberMap& moved_base = moved;
  copy = moved;
  copy = std::move(moved);
  EXPECT_EQ(reference.at(3), 30);
  EXPECT_EQ(moved_base.at(3), 30);
  EXPECT_EQ(reference, moved_base);
}

// Verifies transparent string access through one common type, including
// standard strings, C strings, small strings and Arduino strings when enabled.
TEST(ReadOnlyFlatSmallHashtable, StringMapInterface) {
  FlatSmallHashMap<::roo::string_view, int, TransparentStringHashFn,
                   TransparentEq>
      dynamic({{"on", 1}, {"off", 0}});
  const StringMap& live = dynamic;
  const StringMap& constant = kNames;
  EXPECT_EQ(live.at(std::string("on")), 1);
  EXPECT_EQ(constant[SmallString<8>("off")], 0);
  EXPECT_EQ(live, constant);
#ifdef ARDUINO
  EXPECT_EQ(live.at(::String("on")), 1);
  EXPECT_EQ(constant.at(::String("off")), 0);
#endif
}

// Verifies set aliases expose the same read-only type and remain live through
// mutations and assignment, including empty immutable sets.
TEST(ReadOnlyFlatSmallHashtable, SetInterface) {
  FlatSmallHashSet<int> dynamic;
  const NumberSet& reference = dynamic;
  dynamic.insert(1);
  dynamic.insert(2);
  dynamic.insert(3);
  EXPECT_EQ(reference, static_cast<const NumberSet&>(kKeys));
  dynamic.erase(2);
  EXPECT_FALSE(reference.contains(2));
  FlatSmallHashSet<int> replacement;
  replacement.insert(99);
  dynamic = std::move(replacement);
  EXPECT_EQ(reference.size(), 1);
  EXPECT_TRUE(reference.contains(99));
  EXPECT_TRUE(replacement.empty());
  EXPECT_EQ(static_cast<const NumberSet&>(kEmpty).begin(),
            static_cast<const NumberSet&>(kEmpty).end());
}

struct Record {
  int id;
  int value;
};
struct Query {
  int id;
};
struct StatefulKey {
  int offset;
  constexpr int operator()(const Record& entry) const {
    return entry.id + offset;
  }
};
struct StatefulHash {
  using is_transparent = void;
  unsigned seed;
  constexpr size_t operator()(int key) const { return unsigned(key) ^ seed; }
  constexpr size_t operator()(Query key) const {
    return unsigned(key.id) ^ seed;
  }
};
struct StatefulEq {
  using is_transparent = void;
  constexpr bool operator()(int key, int other) const { return key == other; }
  constexpr bool operator()(int key, Query other) const {
    return key == other.id;
  }
};
using RecordBase = ReadOnlyFlatSmallHashtable<Record, int, StatefulHash,
                                              StatefulKey, StatefulEq>;

// Verifies policy state and custom extraction are identical through both base
// references, including queries with no implicit conversion to the key type.
TEST(ReadOnlyFlatSmallHashtable, StatefulPoliciesAndHeterogeneousQueries) {
  static constexpr ImmutableFlatSmallHashtable fixed(WithEntries<Record, int>(
      {{1, 10}, {2, 20}}, StatefulHash{99}, StatefulKey{10}, StatefulEq{}));
  const RecordBase& readonly = fixed;
  EXPECT_EQ(readonly.find(Query{11})->value, 10);
  EXPECT_FALSE(readonly.contains(Query{1}));
  FlatSmallHashtable<Record, int, StatefulHash, StatefulKey, StatefulEq>
      dynamic({{1, 10}, {2, 20}}, StatefulHash{99}, StatefulKey{10},
              StatefulEq{});
  const RecordBase& live = dynamic;
  EXPECT_EQ(live.find(Query{12})->value, 20);
  auto copy = dynamic;
  auto moved = std::move(copy);
  EXPECT_EQ(static_cast<const RecordBase&>(moved).find(Query{11})->value, 10);
}

struct CopyOnlyHash {
  unsigned seed;
  constexpr explicit CopyOnlyHash(unsigned value) : seed(value) {}
  constexpr CopyOnlyHash(const CopyOnlyHash&) = default;
  CopyOnlyHash(CopyOnlyHash&&) = delete;
  CopyOnlyHash& operator=(const CopyOnlyHash&) = default;
  constexpr size_t operator()(int key) const { return unsigned(key) ^ seed; }
};

// Verifies construction still accepts copy-only stateful policies; the common
// base must not impose a new move-construction requirement on existing callers.
TEST(ReadOnlyFlatSmallHashtable, CopyOnlyHashConstruction) {
  const CopyOnlyHash hash(99);
  FlatSmallHashMap<int, int, CopyOnlyHash> map(2, hash);
  map[1] = 10;
  using Base = ReadOnlyFlatSmallHashMap<int, int, CopyOnlyHash>;
  EXPECT_EQ(static_cast<const Base&>(map).at(1), 10);
  const auto data = WithMapping<int, int>({{1, 10}}, hash);
  ImmutableFlatSmallHashMap fixed(data);
  EXPECT_EQ(static_cast<const Base&>(fixed).at(1), 10);
}

// Verifies runtime owners and data factories support non-literal owning values
// as well as constexpr-friendly keys and values.
TEST(ReadOnlyFlatSmallHashtable, OwningStringValues) {
  auto data = WithMapping<int, std::string>(
      {{1, std::string(100, 'a')}, {2, std::string(100, 'b')}});
  ImmutableFlatSmallHashMap fixed(std::move(data));
  const ReadOnlyFlatSmallHashMap<int, std::string>& base = fixed;
  EXPECT_EQ(base.at(1), std::string(100, 'a'));
  auto copy = fixed;
  EXPECT_NE(&copy.at(1), &fixed.at(1));
  EXPECT_EQ(copy.at(2), std::string(100, 'b'));
}

struct AssignOnlyEntry {
  int id = 0;
  int value = 0;
  constexpr AssignOnlyEntry() = default;
  constexpr AssignOnlyEntry(int key, int mapped) : id(key), value(mapped) {}
  AssignOnlyEntry(const AssignOnlyEntry&) = delete;
  AssignOnlyEntry(AssignOnlyEntry&&) = delete;
  AssignOnlyEntry& operator=(const AssignOnlyEntry&) = default;
};
struct AssignOnlyKey {
  constexpr int operator()(const AssignOnlyEntry& entry) const {
    return entry.id;
  }
};

// Verifies immutable construction retains the original default-constructible,
// copy-assignable entry contract even when entries cannot be copy constructed.
TEST(ReadOnlyFlatSmallHashtable, AssignOnlyEntries) {
  static constexpr ImmutableFlatSmallHashtable fixed(
      WithEntries<AssignOnlyEntry, int>({{1, 10}, {2, 20}},
                                        DefaultHashFn<int>{}, AssignOnlyKey{}));
  static_assert(fixed.find(2)->value == 20, "copy-assignable entries");
  auto copy = fixed;
  using Base = decltype(copy)::read_only_type;
  const Base& base = copy;
  EXPECT_EQ(base.find(1)->value, 10);
  EXPECT_NE(&*base.find(1), &*static_cast<const Base&>(fixed).find(1));
}

// Verifies replacing integral/enum hashing leaves the std::hash fallback for
// floating-point and pointer keys intact, including equal signed-zero keys.
TEST(ReadOnlyFlatSmallHashtable, NonIntegralHashCompatibility) {
  const double values[] = {0.0, -0.0, 1.0, -1.0, 123.5};
  for (double key : values) {
    EXPECT_EQ(DefaultHashFn<double>()(key), std::hash<double>()(key));
  }
  EXPECT_EQ(DefaultHashFn<double>()(0.0), DefaultHashFn<double>()(-0.0));
  int key = 0;
  EXPECT_EQ(DefaultHashFn<int*>()(&key), std::hash<int*>()(&key));
}

}  // namespace
}  // namespace roo_collections
