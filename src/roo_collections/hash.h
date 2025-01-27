#pragma once

#include <inttypes.h>
#include <stddef.h>

namespace roo_collections {

uint32_t murmur3_32(const void* key, size_t len, uint32_t seed);

}