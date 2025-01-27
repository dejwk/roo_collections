#include "roo_collections/hash.h"

namespace roo_collections {

static inline uint32_t murmur_32_scramble(uint32_t k) {
  k *= 0xcc9e2d51;
  k = (k << 15) | (k >> 17);
  k *= 0x1b873593;
  return k;
}

uint32_t murmur3_32(const void* key, size_t len, uint32_t seed) {
  const unsigned char* buf = (const unsigned char*)key;
  uint32_t h = seed;
  uint32_t k;
  for (size_t i = len >> 2; i; i--) {
    k = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3]);
    buf += 4;
    h ^= murmur_32_scramble(k);
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xe6546b64;
  }
  k = 0;
  for (size_t i = len & 3; i; i--) {
    k <<= 8;
    k |= buf[i - 1];
  }
  h ^= murmur_32_scramble(k);
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

}  // namespace roo_collections