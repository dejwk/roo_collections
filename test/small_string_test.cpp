#include "roo_collections/small_string.h"

#include <cstdlib>
#include <string>

#include "gtest/gtest.h"
#include "roo_collections/flat_small_hash_map.h"

namespace roo_collections {

namespace {

[[noreturn]] void ExitWithStatus(int status) { ::_Exit(status); }

[[noreturn]] void VerifyStringViewConstructorCase() {
  char raw[3] = {'a', 'b', 'c'};
  SmallString<4> value(roo::string_view(raw, 3));
  if (value.length() != 3 || value.view() != "abc") {
    ExitWithStatus(1);
  }
  ExitWithStatus(0);
}

[[noreturn]] void VerifyStringViewAssignmentCase() {
  char raw[3] = {'d', 'e', 'f'};
  SmallString<4> value;
  value = roo::string_view(raw, 3);
  if (value.length() != 3 || value.view() != "def") {
    ExitWithStatus(1);
  }
  ExitWithStatus(0);
}

[[noreturn]] void VerifyCStringConstructorCase() {
  SmallString<4> value("abcd");
  if (value.length() != 3 || value.view() != "abc") {
    ExitWithStatus(1);
  }
  ExitWithStatus(0);
}

[[noreturn]] void VerifyStdStringConstructorCase() {
  SmallString<4> value(std::string("abcd"));
  if (value.length() != 3 || value.view() != "abc") {
    ExitWithStatus(1);
  }
  ExitWithStatus(0);
}

}  // namespace

// Verifies constructing from a bounded string_view does not read beyond the
// view and preserves the expected contents.
TEST(SmallString, StringViewConstructorDoesNotReadPastBounds) {
  EXPECT_EXIT(VerifyStringViewConstructorCase(), ::testing::ExitedWithCode(0),
              "");
}

// Verifies assigning from a bounded string_view does not read beyond the view
// and preserves the expected contents.
TEST(SmallString, StringViewAssignmentDoesNotReadPastBounds) {
  EXPECT_EXIT(VerifyStringViewAssignmentCase(), ::testing::ExitedWithCode(0),
              "");
}

// Verifies C-string construction truncates to the payload capacity.
TEST(SmallString, CStringConstructorTruncatesToPayloadCapacity) {
  EXPECT_EXIT(VerifyCStringConstructorCase(), ::testing::ExitedWithCode(0), "");
}

// Verifies std::string construction truncates to the payload capacity.
TEST(SmallString, StdStringConstructorTruncatesToPayloadCapacity) {
  EXPECT_EXIT(VerifyStdStringConstructorCase(), ::testing::ExitedWithCode(0),
              "");
}

// Verifies that the payload starts at the beginning of the object and the
// maximum length fits in the last byte.
TEST(SmallString, StoresFullLengthAfterCharacterBuffer) {
  const std::string text(260, 'x');
  const SmallString<256> value(text);
  static_assert(sizeof(value) == 256, "The length is included in N");
  static_assert(sizeof(SmallString<1>) == 1,
                "An empty-only string holds one byte");
  EXPECT_EQ(value.length(), 255u);
  EXPECT_EQ(value.view().data(), reinterpret_cast<const char*>(&value));
  EXPECT_EQ(reinterpret_cast<const unsigned char*>(&value)[255], 255u);
  EXPECT_EQ(value.view(), roo::string_view(text.data(), 255));
  EXPECT_TRUE(SmallString<1>("x").view().empty());
}

// Verifies bounded sources preserve zeros and distinct binary keys stay
// distinct during hashing and lookup.
TEST(SmallString, BoundedSourcesPreserveZeroBytes) {
  const char input[] = {'a', 'b', '\0', 'c'};
  const std::string text(input, sizeof(input));
  SmallString<8> from_string(text);
  SmallString<8> from_view(roo::string_view(input, sizeof(input)));
  EXPECT_EQ(from_string.length(), sizeof(input));
  EXPECT_EQ(from_string, from_view);
  EXPECT_EQ(from_view.view(), roo::string_view(input, sizeof(input)));
  EXPECT_NE(from_view, SmallString<8>("ab"));

  FlatSmallHashMap<SmallString<8>, int> map;
  map.insert({from_view, 17});
  map.insert({SmallString<8>("ab"), 42});
  ASSERT_EQ(map.size(), 2);
  ASSERT_NE(map.find(from_string), map.end());
  EXPECT_EQ(map.find(from_string)->second, 17);
  EXPECT_EQ(map.find(SmallString<8>("ab"))->second, 42);

  from_view = roo::string_view(input + 2, 2);
  EXPECT_EQ(from_view.view(), roo::string_view(input + 2, 2));
  EXPECT_FALSE(from_view.empty());
}

// Verifies cross-capacity assignment preserves zeros, truncates to the
// destination's capacity, and clears the destination for an empty source.
TEST(SmallString, AssignsAcrossCapacities) {
  const char input[] = {'a', '\0', 'b', 'c'};
  const SmallString<5> source(roo::string_view(input, sizeof(input)));

  SmallString<8> larger("initial");
  larger = source;
  EXPECT_EQ(larger.view(), roo::string_view(input, sizeof(input)));

  SmallString<4> smaller;
  smaller = larger;
  EXPECT_EQ(smaller.view(), roo::string_view(input, 3));

  larger = SmallString<1>();
  EXPECT_TRUE(larger.empty());
}

// Verifies copying and assignment maintain the payload and length, also
// when the input view aliases the destination's buffer.
TEST(SmallString, CopyAndAliasedAssignment) {
  SmallString<8> value("abcdef");
  value = roo::string_view(value.view().data() + 1, 3);
  EXPECT_EQ(value.length(), 3u);
  EXPECT_EQ(value.view(), roo::string_view("bcd"));

  const SmallString<8> copy(value);
  SmallString<8> assigned("another");
  assigned = copy;
  EXPECT_EQ(copy, value);
  EXPECT_EQ(assigned, value);
  EXPECT_EQ(assigned.view(), roo::string_view("bcd"));

  assigned = "a";
  EXPECT_EQ(assigned.length(), 1u);
  EXPECT_NE(assigned, value);
  assigned = roo::string_view();
  EXPECT_TRUE(assigned.empty());
  EXPECT_EQ(assigned.view(), roo::string_view(""));
}

}  // namespace roo_collections
