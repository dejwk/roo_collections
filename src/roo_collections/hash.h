#pragma once

/// @file
/// @brief Hashing utilities used by roo_collections containers.
/// @ingroup roo_collections

#include <inttypes.h>
#include <stddef.h>

namespace roo_collections {

namespace internal {

/// Mixes one four-byte block for MurmurHash3.
constexpr uint32_t Murmur32Scramble(uint32_t k) {
  k *= 0xcc9e2d51;
  k = (k << 15) | (k >> 17);
  k *= 0x1b873593;
  return k;
}

/// Hashes character or unsigned-byte data without pointer reinterpretation.
template <typename Byte>
constexpr uint32_t Murmur3_32Bytes(const Byte* key, size_t len, uint32_t seed) {
  uint32_t h = seed;
  size_t offset = 0;
  for (size_t blocks = len / 4; blocks > 0; --blocks) {
    uint32_t k = static_cast<unsigned char>(key[offset]);
    k |= static_cast<uint32_t>(static_cast<unsigned char>(key[offset + 1]))
         << 8;
    k |= static_cast<uint32_t>(static_cast<unsigned char>(key[offset + 2]))
         << 16;
    k |= static_cast<uint32_t>(static_cast<unsigned char>(key[offset + 3]))
         << 24;
    offset += 4;
    h ^= Murmur32Scramble(k);
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xe6546b64;
  }
  uint32_t k = 0;
  for (size_t i = len % 4; i > 0; --i) {
    k = (k << 8) | static_cast<unsigned char>(key[offset + i - 1]);
  }
  h ^= Murmur32Scramble(k);
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

}  // namespace internal

/// @brief Computes MurmurHash3 during constant evaluation or at runtime.
/// @param key Character buffer, including arbitrary byte values and embedded
/// NULs.
/// @param len Number of bytes to hash; zero permits a null @p key.
/// @param seed Initial hash seed.
/// @return The same 32-bit hash as murmur3_32().
constexpr uint32_t Murmur3_32(const char* key, size_t len, uint32_t seed) {
  return internal::Murmur3_32Bytes(key, len, seed);
}

/// @brief Computes 32-bit MurmurHash3 of a binary buffer.
/// @param key Pointer to the first byte of the buffer.
/// @param len Number of bytes to hash.
/// @param seed Hash seed.
/// @return 32-bit hash value.
uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);

}  // namespace roo_collections