#include "gtest/gtest.h"
#include "roo_collections/flat_small_hashtable.h"

namespace roo_collections {
namespace {

static_assert(fastmod(1, 1) == 1, "modulo three");
static_assert(fastmod(1, 2) == 1, "modulo seven");
static_assert(fastmod(UINT32_MAX, 1) == 0, "large modulo three");
static_assert(initialCapacityIdx(0) == 0, "empty capacity");
static_assert(initialCapacityIdx(1) == 1, "singleton capacity");
static_assert(initialCapacityIdx(3) == 2, "small capacity");

// Verifies reciprocal reduction matches modulo for every supported capacity,
// including the small capacities used by compile-time tables.
TEST(FlatSmallHashtablePrimitives, FastmodMatchesModulo) {
  for (int idx = 0; idx < 16; ++idx) {
    uint32_t prime = kRadkePrimes[idx];
    uint32_t boundaries[] = {0, 1, 2, prime - 1, prime, prime + 1, UINT32_MAX};
    for (uint32_t n : boundaries) {
      EXPECT_EQ(fastmod(n, idx), n % prime) << "capacity=" << prime;
    }
    uint32_t n = 42;
    for (int sample = 0; sample < 1000; ++sample) {
      n = n * 1664525u + 1013904223u;
      EXPECT_EQ(fastmod(n, idx), n % prime) << "capacity=" << prime;
    }
  }
}

}  // namespace
}  // namespace roo_collections
