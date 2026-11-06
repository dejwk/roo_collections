#pragma once

/// @file
/// @brief Shared quadratic probing for mutable and immutable flat tables.

#include <assert.h>
#include <stdint.h>

namespace roo_collections {
namespace internal {

/// Visits every bucket of a Radke-prime-sized table exactly once.
class FlatSmallHashtableProbe {
 public:
  /// Creates a probe starting at @p first in a table of @p capacity buckets.
  constexpr FlatSmallHashtableProbe(uint16_t first, uint16_t capacity)
      : position_(first), capacity_(capacity), step_(2 - int32_t(capacity)) {}

  /// Returns the current bucket.
  constexpr uint16_t position() const { return position_; }

  /// Advances to the next bucket; requires an unvisited bucket to remain.
  constexpr void advance() {
    assert(step_ < capacity_);
    position_ += step_ >= 0 ? step_ : -step_;
    if (position_ >= capacity_) position_ -= capacity_;
    step_ += 2;
  }

 private:
  uint32_t position_;
  uint16_t capacity_;
  int32_t step_;
};

/// Finds a matching bucket, or returns capacity. Accepts both signed and
/// unsigned state bytes, with zero empty and bit 7 marking occupied buckets.
template <typename Entry, typename State, typename Key, typename KeyFn,
          typename KeyCmpFn>
constexpr uint16_t FindFlatSmallBucket(const Entry* entries,
                                       const State* states, uint16_t capacity,
                                       uint16_t first, uint32_t hash,
                                       const Key& key, const KeyFn& key_fn,
                                       const KeyCmpFn& key_cmp_fn) {
  FlatSmallHashtableProbe probe(first, capacity);
  for (uint32_t visited = 0; visited < capacity; ++visited) {
    uint16_t pos = probe.position();
    if (states[pos] == 0) return capacity;
    if ((states[pos] & 0x80) != 0 && (states[pos] & 0x7F) == (hash & 0x7F) &&
        key_cmp_fn(key_fn(entries[pos]), key)) {
      return pos;
    }
    if (visited + 1 < capacity) probe.advance();
  }
  return capacity;
}

}  // namespace internal
}  // namespace roo_collections
