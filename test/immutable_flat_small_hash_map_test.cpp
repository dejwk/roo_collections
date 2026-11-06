#include <array>
#include <string>
#include <type_traits>

#include "gtest/gtest.h"
#include "roo_collections_immutable_flat_small_hash_map.h"

namespace roo_collections {
namespace {

constexpr auto kCommands =
    MakeImmutableFlatSmallHashMap<::roo::string_view, int>(
        {{"start", 1}, {"stop", 2}, {"reset", 3}});
constexpr auto kStrings = MakeImmutableFlatSmallStringHashMap<int>(
    {{"start", 1}, {"stop", 2}, {"reset", 3}});
constexpr auto kEmpty =
    MakeImmutableFlatSmallHashMap(std::array<std::pair<int, int>, 0>{});
constexpr ImmutableFlatSmallHashMap<int, int, 0> kDefaultEmpty;

static_assert(kCommands.at("stop") == 2, "constexpr mapped lookup");
static_assert(kCommands["reset"] == 3, "constexpr subscript");
static_assert(kStrings.at("start") == 1, "transparent string literal lookup");
static_assert(kStrings.find("missing") == kStrings.end(), "missing key");
static_assert(kEmpty.empty() && kDefaultEmpty.empty(), "empty maps");
static_assert(std::is_same<decltype(kCommands.at("stop")), const int&>::value,
              "const mapped reference");

struct ConstantHash {
  constexpr size_t operator()(int) const { return 6; }
};

// Verifies pair insertion remains constexpr under C++17 despite std::pair's
// non-constexpr assignment, including collision handling and mapped values.
TEST(ImmutableFlatSmallHashMap, ConstexprPairsAndCollisions) {
  constexpr auto map = MakeImmutableFlatSmallHashMap<int, int>(
      {{1, 10}, {2, 20}, {3, 30}, {4, 40}}, ConstantHash{});
  static_assert(map.at(4) == 40, "colliding pair lookup");
  static_assert(!map.contains(5), "colliding pair miss");
  for (int key = 1; key <= 4; ++key) EXPECT_EQ(map.at(key), key * 10);
  EXPECT_EQ(map.find(5), map.end());
}

// Verifies the intended literal-key API, constexpr lookup, and runtime lookup
// of string keys without making owning copies of stored strings.
TEST(ImmutableFlatSmallHashMap, LiteralKeysAndRuntimeQueries) {
  EXPECT_EQ(kCommands.size(), 3);
  EXPECT_EQ(kCommands["start"], 1);
  std::string stop = "stop";
  EXPECT_EQ(kStrings.at(stop), 2);
  EXPECT_EQ(kStrings[::roo::string_view("reset")], 3);
  EXPECT_FALSE(kStrings.contains(std::string("missing")));
  SmallString<8> start("start");
  EXPECT_EQ(kStrings.at(start), 1);
#ifdef ARDUINO
  EXPECT_EQ(kStrings.at(::String("stop")), 2);
#endif
}

// Verifies string keys retain their explicit lengths and unsigned byte values.
TEST(ImmutableFlatSmallHashMap, BinaryStringKeys) {
  constexpr auto map = MakeImmutableFlatSmallStringHashMap<int>(
      {{::roo::string_view("a\0b", 3), 10}, {"a", 20}, {"\xff", 30}});
  static_assert(map.at(::roo::string_view("a\0b", 3)) == 10, "embedded NUL");
  static_assert(map.at("\xff") == 30, "high-bit byte");
  EXPECT_EQ(map.at("a"), 20);
  EXPECT_FALSE(map.contains(::roo::string_view("a\0", 2)));
}

// Verifies constexpr copies preserve values, own their arrays, and expose
// neither mutable keys nor mutable mapped values from non-const map objects.
TEST(ImmutableFlatSmallHashMap, CopiesAndConstOnlyAccess) {
  constexpr auto copy = kCommands;
  static_assert(copy == kCommands, "constexpr copy");
  auto runtime_copy = kCommands;
  static_assert(std::is_same<decltype(runtime_copy["stop"]), const int&>::value,
                "subscript is read only");
  static_assert(std::is_same<decltype(*runtime_copy.begin()),
                             const std::pair<::roo::string_view, int>&>::value,
                "entries are read only");
  EXPECT_NE(&runtime_copy.at("stop"), &kCommands.at("stop"));
  EXPECT_EQ(runtime_copy, kCommands);
}

// Verifies direct std::array construction and zero-entry string maps.
TEST(ImmutableFlatSmallHashMap, ArrayAndEmptyConstruction) {
  constexpr ImmutableFlatSmallHashMap<int, int, 2> map(
      std::array<std::pair<int, int>, 2>{{{1, 10}, {2, 20}}});
  constexpr auto empty = MakeImmutableFlatSmallStringHashMap<int>(
      std::array<std::pair<::roo::string_view, int>, 0>{});
  constexpr ImmutableFlatSmallStringHashMap<int, 0> default_empty;
  static_assert(map.at(2) == 20, "explicit construction");
  static_assert(empty.empty() && default_empty.empty(), "empty string maps");
  EXPECT_EQ(kEmpty.begin(), kEmpty.end());
  EXPECT_EQ(kDefaultEmpty, kEmpty);
  EXPECT_FALSE(empty.contains("missing"));
}

// Verifies map duplicates are rejected by key even when values differ.
TEST(ImmutableFlatSmallHashMap, DuplicateKeysAbort) {
  EXPECT_DEATH((MakeImmutableFlatSmallHashMap<int, int>({{1, 10}, {1, 20}})),
               "");
  EXPECT_DEATH((MakeImmutableFlatSmallStringHashMap<int>({{"x", 1}, {"x", 2}})),
               "");
}

#ifndef NDEBUG
// Verifies at() retains the existing map's debug assertion for missing keys.
TEST(ImmutableFlatSmallHashMap, MissingAtAsserts) {
  EXPECT_DEATH(kCommands.at("missing"), "");
}
#endif

}  // namespace
}  // namespace roo_collections
