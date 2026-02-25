#pragma once

/// @file
/// @brief Hashing utilities used by roo_collections containers.
/// @ingroup roo_collections

#include <inttypes.h>
#include <stddef.h>

namespace roo_collections {

/// @brief Computes 32-bit MurmurHash3 of a binary buffer.
/// @param key Pointer to the first byte of the buffer.
/// @param len Number of bytes to hash.
/// @param seed Hash seed.
/// @return 32-bit hash value.
uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);

}  // namespace roo_collections