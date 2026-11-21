#pragma once

#include <cstdint>

namespace hashmap_benchmark {

// Keeps calibration and sampling on the same machine-code loop. Separate
// inlined copies can occupy different ESP32 flash-cache sets.
template <typename Work, typename ReadCycles>
[[gnu::noipa]] uint32_t TimeBatch(uint32_t count, Work& work,
                                  ReadCycles read_cycles) {
  asm volatile("" ::: "memory");
  const uint32_t begin = read_cycles();
  asm volatile("" ::: "memory");
  work(count);
  asm volatile("" ::: "memory");
  return read_cycles() - begin;
}

}  // namespace hashmap_benchmark
