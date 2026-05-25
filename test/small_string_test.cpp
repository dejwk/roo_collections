#include "roo_collections/small_string.h"

#include <cstdlib>
#include <string>

#include "gtest/gtest.h"

namespace roo_collections {

namespace {

[[noreturn]] void ExitWithStatus(int status) { ::_Exit(status); }

[[noreturn]] void VerifyStringViewConstructorCase() {
  char raw[3] = {'a', 'b', 'c'};
  SmallString<4> value(roo::string_view(raw, 3));
  if (value.length() != 3 || std::string(value.c_str()) != "abc") {
    ExitWithStatus(1);
  }
  ExitWithStatus(0);
}

[[noreturn]] void VerifyStringViewAssignmentCase() {
  char raw[3] = {'d', 'e', 'f'};
  SmallString<4> value;
  value = roo::string_view(raw, 3);
  if (value.length() != 3 || std::string(value.c_str()) != "def") {
    ExitWithStatus(1);
  }
  ExitWithStatus(0);
}

[[noreturn]] void VerifyCStringConstructorCase() {
  SmallString<4> value("abcd");
  if (value.length() != 3 || std::string(value.c_str()) != "abc") {
    ExitWithStatus(1);
  }
  ExitWithStatus(0);
}

[[noreturn]] void VerifyStdStringConstructorCase() {
  SmallString<4> value(std::string("abcd"));
  if (value.length() != 3 || std::string(value.c_str()) != "abc") {
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

// Verifies C-string construction truncates to fit and keeps the inline buffer
// null-terminated.
TEST(SmallString, CStringConstructorTruncatesAndNullTerminates) {
  EXPECT_EXIT(VerifyCStringConstructorCase(), ::testing::ExitedWithCode(0), "");
}

// Verifies std::string construction truncates to fit and keeps the inline
// buffer null-terminated.
TEST(SmallString, StdStringConstructorTruncatesAndNullTerminates) {
  EXPECT_EXIT(VerifyStdStringConstructorCase(), ::testing::ExitedWithCode(0),
              "");
}

}  // namespace roo_collections