#include <array>
#include <iterator>
#include <string>
#include <type_traits>

#include "gtest/gtest.h"
#include "roo_collections_immutable_flat_small_hash_set.h"

namespace roo_collections {
namespace {

static constexpr ImmutableFlatSmallHashtable kNumbers(WithElements<int>({1, -2,
                                                                         42}));
static constexpr ImmutableFlatSmallHashtable kCommands(
    WithStringElements({"start", "stop", "reset"}));
static constexpr ImmutableFlatSmallHashtable kEmpty(
    WithElements(std::array<int, 0>{}));
static constexpr ImmutableFlatSmallHashSet<int, 0> kDefaultEmpty;

static_assert(kNumbers.contains(-2) && !kNumbers.contains(3), "integral set");
static_assert(kCommands.contains("stop"), "string set");
static_assert(kCommands.find("missing") == kCommands.end(), "missing key");
static_assert(kEmpty.empty() && kDefaultEmpty.empty(), "empty set");
static_assert(
    std::is_same<decltype(*std::declval<decltype(kNumbers)&>().begin()),
                 const int&>::value,
    "keys cannot be mutated");

struct ConstantHash {
  constexpr size_t operator()(int) const { return 10; }
};

// Verifies constexpr construction and misses with collisions wrapping around
// the bucket array, and equality independent of key input order.
TEST(ImmutableFlatSmallHashSet, CollisionsAndIteration) {
  static constexpr ImmutableFlatSmallHashtable set(
      WithElements<int>({1, 2, 3, 4, 5, 6, 7, 8}, ConstantHash{}));
  static constexpr ImmutableFlatSmallHashtable reversed(
      WithElements<int>({8, 7, 6, 5, 4, 3, 2, 1}, ConstantHash{}));
  static_assert(set == reversed, "order independent equality");
  static_assert(!set.contains(9), "colliding miss");
  int sum = 0;
  for (int key : set) sum += key;
  EXPECT_EQ(sum, 36);
  EXPECT_EQ(std::distance(set.cbegin(), set.cend()), 8);
  EXPECT_EQ(set, reversed);
  for (int key = 1; key <= 8; ++key) EXPECT_TRUE(set.contains(key));
}

// Verifies string views support runtime heterogeneous queries and constexpr
// literal lookup while retaining only borrowed character data.
TEST(ImmutableFlatSmallHashSet, StringLookups) {
  std::string start = "start";
  EXPECT_TRUE(kCommands.contains(start));
  EXPECT_TRUE(kCommands.contains(::roo::string_view("stop")));
  SmallString<8> reset("reset");
  EXPECT_NE(kCommands.find(reset), kCommands.end());
  EXPECT_FALSE(kCommands.contains(std::string("missing")));
#ifdef ARDUINO
  EXPECT_TRUE(kCommands.contains(::String("stop")));
#endif
  static constexpr ImmutableFlatSmallHashtable binary(
      WithStringElements({::roo::string_view("a\0b", 3), "a", "\xff"}));
  static_assert(binary.contains(::roo::string_view("a\0b", 3)), "embedded NUL");
  static_assert(binary.contains("\xff"), "high-bit byte");
  EXPECT_FALSE(binary.contains(::roo::string_view("a\0", 2)));
}

// Verifies input entries and copied sets have independent inline storage.
TEST(ImmutableFlatSmallHashSet, CopiesOwnTheirKeys) {
  int keys[] = {1, 2, 3};
  auto set = MakeImmutableFlatSmallHashSet(keys);
  keys[0] = 9;
  EXPECT_TRUE(set.contains(1));
  EXPECT_FALSE(set.contains(9));
  auto copy = set;
  static_assert(std::is_same<decltype(*copy.begin()), const int&>::value,
                "non-const set still exposes const keys");
  EXPECT_EQ(copy, set);
  EXPECT_NE(&*copy.find(1), &*set.find(1));
}

// Verifies helper deduction for nonempty std::arrays, default empty sets
// and zero-key string inputs, alongside direct array construction.
TEST(ImmutableFlatSmallHashSet, ArrayAndEmptyConstruction) {
  static constexpr ImmutableFlatSmallHashSet<int, 2> set(
      std::array<int, 2>{{4, 5}});
  static constexpr ImmutableFlatSmallHashtable from_array(
      WithElements(std::array<int, 2>{{4, 5}}));
  static constexpr ImmutableFlatSmallHashtable names(
      WithStringElements(std::array<::roo::string_view, 2>{{"on", "off"}}));
  static constexpr ImmutableFlatSmallHashtable strings(
      WithStringElements(std::array<::roo::string_view, 0>{}));
  static constexpr ImmutableFlatSmallStringHashSet<0> default_strings;
  static_assert(set.contains(4) && set.contains(5),
                "direct array construction");
  static_assert(from_array == set, "deduced array elements");
  static_assert(names.contains("on"), "deduced string array elements");
  static_assert(strings.empty() && default_strings.empty(), "empty strings");
  EXPECT_TRUE(from_array.contains(5));
  EXPECT_TRUE(names.contains(std::string("off")));
  EXPECT_EQ(kEmpty, kDefaultEmpty);
  EXPECT_EQ(kEmpty.begin(), kEmpty.end());
  EXPECT_FALSE(strings.contains("missing"));
}

// Verifies duplicate strings compare by content even with distinct buffers.
TEST(ImmutableFlatSmallHashSet, DuplicateKeysAbort) {
  EXPECT_DEATH((MakeImmutableFlatSmallHashSet<int>({1, 1})), "");
  const char first[] = "duplicate";
  const char second[] = "duplicate";
  EXPECT_DEATH((MakeImmutableFlatSmallStringHashSet({first, second})), "");
}

}  // namespace
}  // namespace roo_collections
