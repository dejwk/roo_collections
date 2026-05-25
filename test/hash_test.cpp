#include "roo_collections/hash.h"

#include <stdint.h>

#include <string>

#include "gtest/gtest.h"

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

}  // namespace roo_collections