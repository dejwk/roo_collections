#include "roo_collections/hash.h"

#include <stdint.h>

#include <string>

#include "gtest/gtest.h"
#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {

namespace {

uint32_t rotl32(uint32_t value, int8_t bits) {
  return (value << bits) | (value >> (32 - bits));
}

uint32_t referenceMurmur3_32(const void* key, size_t len, uint32_t seed) {
  const uint8_t* data = static_cast<const uint8_t*>(key);
  uint32_t h = seed;
  for (size_t offset = 0; offset + 4 <= len; offset += 4) {
    uint32_t k = ((uint32_t)data[offset]) | ((uint32_t)data[offset + 1] << 8) |
                 ((uint32_t)data[offset + 2] << 16) |
                 ((uint32_t)data[offset + 3] << 24);
    k *= 0xcc9e2d51;
    k = rotl32(k, 15);
    k *= 0x1b873593;
    h ^= k;
    h = rotl32(h, 13);
    h = h * 5 + 0xe6546b64;
  }

  const uint8_t* tail = data + (len & ~((size_t)3));
  uint32_t k = 0;
  switch (len & 3) {
    case 3:
      k ^= ((uint32_t)tail[2] << 16);
      [[fallthrough]];
    case 2:
      k ^= ((uint32_t)tail[1] << 8);
      [[fallthrough]];
    case 1:
      k ^= tail[0];
      k *= 0xcc9e2d51;
      k = rotl32(k, 15);
      k *= 0x1b873593;
      h ^= k;
      [[fallthrough]];
    default:
      break;
  }

  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

}  // namespace

// Verifies murmur3_32 matches the standard x86_32 MurmurHash3 reference across
// both tail-only and full-block inputs.
TEST(Hash, Murmur3MatchesReferenceImplementation) {
  static const uint32_t seeds[] = {0u, 0x92F4E42Bu};
  static const std::string samples[] = {
      "",     "a",     "ab",      "abc",
      "abcd", "abcde", "abcdefg", "The quick brown fox jumps over the lazy dog",
  };

  for (uint32_t seed : seeds) {
    for (const std::string& sample : samples) {
      EXPECT_EQ(murmur3_32(sample.data(), sample.size(), seed),
                referenceMurmur3_32(sample.data(), sample.size(), seed))
          << "sample='" << sample << "' seed=" << seed;
    }
  }
}

// Verifies compile-time hashing agrees with the reference, including signed
// characters, embedded NULs, full blocks, and every tail length.
TEST(Hash, ConstexprHashesMatchRuntime) {
  static constexpr char bytes[] = {'a', '\0', char(0x80), char(0xff),
                                   'z', 'x',  'y'};
  static constexpr uint32_t hashes[] = {
      Murmur3_32(bytes, 0, 0x92F4E42B), Murmur3_32(bytes, 1, 0x92F4E42B),
      Murmur3_32(bytes, 2, 0x92F4E42B), Murmur3_32(bytes, 3, 0x92F4E42B),
      Murmur3_32(bytes, 4, 0x92F4E42B), Murmur3_32(bytes, 5, 0x92F4E42B),
      Murmur3_32(bytes, 6, 0x92F4E42B), Murmur3_32(bytes, 7, 0x92F4E42B),
  };
  static_assert(Murmur3_32("", 0, 0) == 0, "empty hash");
  static_assert(Murmur3_32("foo", 3, 0) == 0xf6a5c420, "known hash");
  static_assert(TransparentStringHashFn()("foo") ==
                    DefaultHashFn<::roo::string_view>()("foo"),
                "transparent hash consistency");
  static_assert(TransparentEq()(::roo::string_view("foo"), "foo"),
                "constexpr string equality");
  static_assert(TransparentEq()("foo", ::roo::string_view("foo")),
                "constexpr reversed string equality");
  constexpr const char* literal = "foo";
  static_assert(TransparentEq()(::roo::string_view("foo"), literal),
                "constexpr C string equality");
  char buffer[] = "foo";
  EXPECT_TRUE(TransparentEq()(::roo::string_view("foo"), buffer));
  EXPECT_TRUE(TransparentEq()(buffer, ::roo::string_view("foo")));
  for (size_t len = 0; len <= sizeof(bytes); ++len) {
    EXPECT_EQ(hashes[len], referenceMurmur3_32(bytes, len, 0x92F4E42B));
    EXPECT_EQ(hashes[len], murmur3_32(bytes, len, 0x92F4E42B));
  }
  EXPECT_EQ(Murmur3_32(nullptr, 0, 0), 0u);
  EXPECT_EQ(murmur3_32(nullptr, 0, 0), 0u);
}

}  // namespace roo_collections