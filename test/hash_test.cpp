#include "roo_collections/hash.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "roo_collections/flat_small_hash_map.h"
#include "roo_collections/immutable_flat_small_hash_map.h"

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

// Verifies the public finalizer's known outputs at compile time and runtime,
// including zero, the sign bit, the maximum word, and an arbitrary bit pattern.
TEST(Hash, Fmix32KnownVectors) {
  static constexpr uint32_t inputs[] = {0u,          1u,          2u,
                                        0x80000000u, 0xffffffffu, 0x12345678u};
  static constexpr uint32_t expected[] = {
      0u, 0x514e28b7u, 0x30f4c306u, 0x6d3c65a0u, 0x81f16f39u, 0xe37cd1bcu};
  static_assert(Murmur3Fmix32(1) == 0x514e28b7u, "constexpr finalizer");
  for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
    EXPECT_EQ(Murmur3Fmix32(inputs[i]), expected[i]);
  }
}

// Verifies the default policy mixes all supported integer widths and enums,
// preserves upper-word influence, and works during constant evaluation.
TEST(Hash, DefaultIntegralAndEnumHashUsesFmix32) {
  enum class Small : int8_t { kNegative = -1, kOne = 1 };
  enum class Wide : uint64_t { kUpper = uint64_t{1} << 32 };
  static_assert(DefaultHashFn<int>()(1) == 0x514e28b7u, "default is mixed");
  static_assert(DefaultHashFn<Small>()(Small::kNegative) == 0x81f16f39u,
                "signed enum conversion");
  static_assert(DefaultHashFn<Wide>()(Wide::kUpper) == 0x2fef5c8fu,
                "wide enum includes its upper word");
  EXPECT_EQ(DefaultHashFn<bool>()(false), 0u);
  EXPECT_EQ(DefaultHashFn<bool>()(true), 0x514e28b7u);
  EXPECT_EQ(DefaultHashFn<int8_t>()(-1), 0x81f16f39u);
  EXPECT_EQ(DefaultHashFn<uint8_t>()(1), 0x514e28b7u);
  EXPECT_EQ(DefaultHashFn<int16_t>()(-1), 0x81f16f39u);
  EXPECT_EQ(DefaultHashFn<uint16_t>()(1), 0x514e28b7u);
  EXPECT_EQ(DefaultHashFn<int32_t>()(-1), 0x81f16f39u);
  EXPECT_EQ(DefaultHashFn<uint32_t>()(0x80000000u), 0x6d3c65a0u);
  EXPECT_EQ(DefaultHashFn<uint64_t>()(uint64_t{1} << 32), 0x2fef5c8fu);
  EXPECT_EQ(DefaultHashFn<uint64_t>()(0x100000001ULL), 0x3d8ed02fu);
  EXPECT_EQ(DefaultHashFn<uint64_t>()(0x123456789abcdef0ULL), 0xf7b45ce0u);
  EXPECT_EQ(DefaultHashFn<int64_t>()(-1), 0xce2d4699u);
  EXPECT_EQ(DefaultHashFn<Small>()(Small::kOne), 0x514e28b7u);
  EXPECT_EQ(DefaultHashFn<Wide>()(Wide::kUpper), 0x2fef5c8fu);
}

// Verifies constexpr construction and runtime lookup agree with mutable maps
// when default hashes mix both words of wide keys.
TEST(Hash, DefaultMixedHashWorksAcrossMapOwners) {
  static constexpr std::pair<uint64_t, int> entries[] = {
      {0, 10}, {1, 20}, {uint64_t{1} << 32, 30}, {UINT64_MAX, 40}};
  static constexpr ImmutableFlatSmallHashMap<uint64_t, int, 4> immutable(
      entries);
  static_assert(immutable.at(uint64_t{1} << 32) == 30,
                "constexpr mixed lookup");
  static_assert(immutable.at(UINT64_MAX) == 40, "constexpr wide lookup");
  static_assert(!immutable.contains(0x100000001ULL), "constexpr mixed miss");
  FlatSmallHashMap<uint64_t, int> mutable_map{
      {0, 10}, {1, 20}, {uint64_t{1} << 32, 30}, {UINT64_MAX, 40}};
  for (const auto& entry : immutable) {
    EXPECT_EQ(immutable.at(entry.first), entry.second);
    EXPECT_EQ(mutable_map.at(entry.first), entry.second);
  }
  EXPECT_FALSE(mutable_map.contains(0x100000001ULL));
}

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