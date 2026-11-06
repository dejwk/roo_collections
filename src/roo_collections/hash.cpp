#include "roo_collections/hash.h"

namespace roo_collections {

uint32_t murmur3_32(const void* key, size_t len, uint32_t seed) {
  return internal::Murmur3_32Bytes(static_cast<const unsigned char*>(key), len,
                                   seed);
}

}  // namespace roo_collections
