#pragma once

/// @file
/// @brief Hashing utilities used by roo_collections containers.
/// @ingroup roo_collections

#include <inttypes.h>
#include <stddef.h>

#include "roo_collections/impl/config.h"

namespace roo_collections {

/// @brief Mixes a 32-bit integer with MurmurHash3's fmix32 finalizer.
/// Supports constant evaluation in C++14+ and runtime use in C++11. This is
/// an unseeded integer permutation, not full MurmurHash3 of four bytes.
ROO_COLLECTIONS_CONSTEXPR14 uint32_t Murmur3Fmix32(uint32_t value) noexcept {
  value ^= value >> 16;
  value *= 0x85ebca6b;
  value ^= value >> 13;
  value *= 0xc2b2ae35;
  return value ^ (value >> 16);
}

namespace internal {

/// Mixes one four-byte block for MurmurHash3.
ROO_COLLECTIONS_CONSTEXPR14 uint32_t Murmur32Scramble(uint32_t k) {
  k *= 0xcc9e2d51;
  k = (k << 15) | (k >> 17);
  k *= 0x1b873593;
  return k;
}

/// Hashes character or unsigned-byte data without pointer reinterpretation.
template <typename Byte>
ROO_COLLECTIONS_CONSTEXPR14 uint32_t Murmur3_32Bytes(const Byte* key,
                                                     size_t len,
                                                     uint32_t seed) {
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
  return Murmur3Fmix32(h ^ len);
}

}  // namespace internal

/// @brief Computes MurmurHash3 at runtime, or in constant evaluation with
/// C++14+.
/// @param key Character buffer, including arbitrary byte values and embedded
/// NULs.
/// @param len Number of bytes to hash; zero permits a null @p key.
/// @param seed Initial hash seed.
/// @return The same 32-bit hash as murmur3_32().
ROO_COLLECTIONS_CONSTEXPR14 uint32_t Murmur3_32(const char* key, size_t len,
                                                uint32_t seed) {
  return internal::Murmur3_32Bytes(key, len, seed);
}

/// @brief Computes 32-bit MurmurHash3 of a binary buffer.
/// @param key Pointer to the first byte of the buffer.
/// @param len Number of bytes to hash.
/// @param seed Hash seed.
/// @return 32-bit hash value.
uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);

}  // namespace roo_collections