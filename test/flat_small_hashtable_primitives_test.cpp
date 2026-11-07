#include <vector>

#include "gtest/gtest.h"
#include "roo_collections/flat_small_hashtable.h"
#include "roo_collections/impl/immutable_flat_small_hashtable_capacity.h"

namespace roo_collections {
namespace {

static_assert(fastmod(1, 1) == 1, "modulo three");
static_assert(fastmod(1, 2) == 1, "modulo seven");
static_assert(fastmod(UINT32_MAX, 1) == 0, "large modulo three");
static_assert(initialCapacityIdx(0) == 0, "empty capacity");
static_assert(initialCapacityIdx(1) == 1, "singleton capacity");
static_assert(initialCapacityIdx(3) == 2, "small capacity");

static_assert(!impl::IsPrime16(0) && !impl::IsPrime16(1), "non-primes");
static_assert(impl::IsPrime16(2) && impl::IsPrime16(65521), "16-bit primes");
static_assert(!impl::IsPrime16(65535), "16-bit composite");
static_assert(impl::NextRadkePrime16(0) == 3, "first Radke prime");
static_assert(impl::NextRadkePrime16(12) == 19, "skip composite candidate");
static_assert(impl::NextRadkePrime16(65519) == 65519, "last Radke prime");
static_assert(impl::NextRadkePrime16(65520) == 0, "no larger Radke prime");
static_assert(impl::NextRadkePrime16(65536) == 0, "no narrowing wraparound");
static_assert(impl::NextRadkePrime16(UINT32_MAX) == 0, "wide input");
static_assert(impl::ImmutableFlatSmallHashtableCapacity<0>::kSlotCount == 1,
              "empty sentinel");
static_assert(impl::ImmutableFlatSmallHashtableCapacity<8>::kSlotCount == 11,
              "exact integer occupancy target");
static_assert(impl::ImmutableFlatSmallHashtableCapacity<9>::kSlotCount == 19,
              "dense small capacity");
static_assert(impl::ImmutableFlatSmallHashtableCapacity<100>::kSlotCount == 139,
              "100 entries need 139 buckets");
static_assert(impl::ImmutableFlatSmallHashtableCapacity<23>::kSlotCount == 31,
              "three-quarter occupancy permits the smaller prime");
static_assert(impl::ImmutableFlatSmallHashtableCapacity<32>::kSlotCount == 43,
              "32 entries fit in 43 buckets");
static_assert(
    impl::ImmutableFlatSmallHashtableCapacity<49139>::kRequiredSlots == 65519,
    "last count meeting the ordinary occupancy target");
static_assert(
    impl::ImmutableFlatSmallHashtableCapacity<49140>::kRequiredSlots == 65520,
    "first count requiring the maximum-size exception");
static_assert(impl::ImmutableFlatSmallHashtableCapacity<49139>::kSlotCount ==
                  65519,
              "largest count within 75 percent occupancy");
static_assert(impl::ImmutableFlatSmallHashtableCapacity<49140>::kSlotCount ==
                  65519,
              "high occupancy exception begins");
static_assert(impl::ImmutableFlatSmallHashtableCapacity<64000>::kSlotCount ==
                  65519,
              "existing maximum remains supported");

// Verifies primality and the smallest qualifying prime against an independent
// sieve over every 16-bit input, including the no-prime boundary above 65519.
TEST(FlatSmallHashtablePrimitives, ImmutablePrimeSearchMatchesSieve) {
  std::vector<bool> prime(65536, true);
  prime[0] = false;
  prime[1] = false;
  for (uint32_t divisor = 2; divisor * divisor < prime.size(); ++divisor) {
    if (!prime[divisor]) continue;
    for (uint32_t multiple = divisor * divisor; multiple < prime.size();
         multiple += divisor) {
      prime[multiple] = false;
    }
  }
  uint16_t expected = 0;
  for (int minimum = 65536; minimum >= 0; --minimum) {
    if (minimum < 65536) {
      EXPECT_EQ(impl::IsPrime16(minimum), prime[minimum]) << minimum;
      if (prime[minimum] && minimum % 4 == 3) expected = minimum;
    }
    EXPECT_EQ(impl::NextRadkePrime16(minimum), expected) << minimum;
  }
}

// Verifies computed reciprocals match modulo for every eligible 16-bit prime,
// including random full-width hashes, boundaries and the empty sentinel.
TEST(FlatSmallHashtablePrimitives, ExactPrimeReducersMatchModulo) {
  for (uint32_t slots = 1; slots <= 65535; slots += 2) {
    if (slots != 1 && (slots % 4 != 3 || !impl::IsPrime16(slots))) continue;
    const impl::FlatSmallHashtableCapacity capacity(slots);
    uint32_t boundaries[] = {0, 1, slots - 1, slots, slots + 1, UINT32_MAX};
    for (uint32_t hash : boundaries) {
      ASSERT_EQ(capacity.reduceHash(hash), hash % slots) << slots;
    }
    uint32_t hash = 42;
    for (int sample = 0; sample < 32; ++sample) {
      hash = hash * 1664525u + 1013904223u;
      ASSERT_EQ(capacity.reduceHash(hash), hash % slots) << slots;
    }
  }
}

// Verifies reciprocal reduction matches modulo for every supported capacity,
// including the small capacities used by compile-time tables.
TEST(FlatSmallHashtablePrimitives, FastmodMatchesModulo) {
  for (int idx = 0; idx < kRadkePrimeCount; ++idx) {
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

// Verifies the shared quadratic probe visits every bucket exactly once at
// every mutable growth size and representative dense immutable sizes, including
// wraparound from the last bucket.
TEST(FlatSmallHashtablePrimitives, ProbeVisitsEveryBucket) {
  std::vector<uint16_t> capacities(std::begin(kRadkePrimes),
                                   std::end(kRadkePrimes));
  capacities.insert(capacities.end(), {19, 23, 43, 139, 311, 1019});
  for (uint16_t capacity : capacities) {
    std::vector<bool> visited(capacity, false);
    internal::FlatSmallHashtableProbe probe(capacity - 1, capacity);
    for (uint32_t i = 0; i < capacity; ++i) {
      uint16_t pos = probe.position();
      ASSERT_LT(pos, capacity);
      ASSERT_FALSE(visited[pos]) << "capacity=" << capacity;
      visited[pos] = true;
      if (i + 1 < capacity) probe.advance();
    }
  }
}

}  // namespace
}  // namespace roo_collections
