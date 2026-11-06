#include <array>
#include <iterator>
#include <string>
#include <type_traits>

#include "gtest/gtest.h"
#include "roo_collections_immutable_flat_small_hash_set.h"

namespace roo_collections {
namespace {

constexpr auto kNumbers = MakeImmutableFlatSmallHashSet<int>({1, -2, 42});
constexpr auto kCommands =
    MakeImmutableFlatSmallStringHashSet({"start", "stop", "reset"});
constexpr auto kEmpty = MakeImmutableFlatSmallHashSet(std::array<int, 0>{});
constexpr ImmutableFlatSmallHashSet<int, 0> kDefaultEmpty;

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
  constexpr auto set = MakeImmutableFlatSmallHashSet<int>(
      {1, 2, 3, 4, 5, 6, 7, 8}, ConstantHash{});
  constexpr auto reversed = MakeImmutableFlatSmallHashSet<int>(
      {8, 7, 6, 5, 4, 3, 2, 1}, ConstantHash{});
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
  constexpr auto binary = MakeImmutableFlatSmallStringHashSet(
      {::roo::string_view("a\0b", 3), "a", "\xff"});
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

// Verifies std::array input, default empty sets and zero-key string factories.
TEST(ImmutableFlatSmallHashSet, ArrayAndEmptyConstruction) {
  constexpr ImmutableFlatSmallHashSet<int, 2> set(std::array<int, 2>{{4, 5}});
  constexpr auto strings =
      MakeImmutableFlatSmallStringHashSet(std::array<::roo::string_view, 0>{});
  constexpr ImmutableFlatSmallStringHashSet<0> default_strings;
  static_assert(set.contains(4) && set.contains(5),
                "direct array construction");
  static_assert(strings.empty() && default_strings.empty(), "empty strings");
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
