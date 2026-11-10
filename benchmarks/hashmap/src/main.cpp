// Workload adaptations of Abseil 20250512.1 raw_hash_set_benchmark.cc
// and unordered_map_lookup_test.h. See README.md for provenance and changes.
// Copyright 2018 The Abseil Authors. Licensed under Apache-2.0.
#include <esp_cpu.h>
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/internal/hashtable_control_bytes.h"
#include "absl/container/node_hash_map.h"
#include "absl/hash/hash.h"
#include "benchmark_containers.h"
#include "benchmark_keys.h"

// Arduino defines F() and EXTERNAL macros that collide with Abseil headers.
#include <Arduino.h>

#include "roo_collections/flat_small_hash_map.h"

namespace {
constexpr int kSamples = 7;
constexpr uint32_t kTargetCycles = 720000;  // 3ms at fixed 240MHz.
volatile uint32_t sink;
uint32_t run_number = 0;

inline void barrier() { asm volatile("" ::: "memory"); }
template <class T>
inline void consume(const T& value) {
  asm volatile("" : : "m"(value) : "memory");
}
[[noreturn]] void fail(const char* reason) {
  Serial.printf("FAIL,%s\n", reason);
  Serial.flush();
  while (true) delay(1000);
}
void check(bool condition, const char* reason) {
  if (!condition) fail(reason);
}

struct Stats {
  double minimum, median, maximum;
  uint32_t batches;
};
template <class Fn>
Stats measure(uint32_t units_per_batch, Fn fn) {
  uint32_t batches = 1;
  auto time = [&](uint32_t count) {
    barrier();
    uint32_t begin = esp_cpu_get_cycle_count();
    fn(count);
    barrier();
    return uint32_t(esp_cpu_get_cycle_count() - begin);
  };
  // Warm up and select a batch lasting at least 3ms. No logging in timed code.
  while (time(batches) < kTargetCycles && batches < 65536) {
    batches *= 2;
    delay(1);
  }
  std::array<double, kSamples> samples;
  for (double& sample : samples) {
    delay(1);
    uint32_t cycles = time(batches);
    sample =
        double(cycles) * (1000.0 / 240.0) / (double(batches) * units_per_batch);
  }
  std::sort(samples.begin(), samples.end());
  return {samples.front(), samples[kSamples / 2], samples.back(), batches};
}

void emit(const char* map, const char* hashing, const char* keys, unsigned n,
          const char* workload, Stats s, int heap_bytes,
          unsigned object_bytes) {
  // libc's dtoa caches allocate lazily at some magnitudes, contaminating later
  // heap baselines. Print fixed-point nanoseconds using integer formatting.
  uint64_t lo = uint64_t(s.minimum * 1000 + 0.5);
  uint64_t mid = uint64_t(s.median * 1000 + 0.5);
  uint64_t hi = uint64_t(s.maximum * 1000 + 0.5);
  Serial.printf(
      "ROW,%u,%s,%s,%s,%u,%s,%llu.%03u,%llu.%03u,%llu.%03u,%u,%d,%u\n",
      run_number, map, hashing, keys, n, workload, lo / 1000,
      unsigned(lo % 1000), mid / 1000, unsigned(mid % 1000), hi / 1000,
      unsigned(hi % 1000), s.batches, heap_bytes, object_bytes);
  Serial.flush();
}

// SplitMix's invertible 32-bit finalizer generates deterministic unique keys.
uint32_t mix(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  return x ^ (x >> 16);
}
template <class Key>
Key make_key(uint32_t index, int length, bool sequential) {
  if constexpr (hashmap_benchmark::IsStringKey<Key>::value) {
    std::string value(length, ' ');
    uint32_t state = mix(index + 0x1234567U);
    for (int i = 0; i < length; ++i) {
      state = mix(state + 0x9e3779b9U);
      value[i] = char(0x20 + state % 95);  // Abseil printable ASCII generator.
    }
    // Fixed-length printable prefix makes disjoint key sets provable.
    for (int i = 0; i < 5; ++i) {
      value[i] = char(0x20 + index % 95);
      index /= 95;
    }
    return Key(value);
  } else if constexpr (sizeof(Key) == 8) {
    return sequential ? Key(index)
                      : (Key(mix(index)) << 32) | Key(mix(index + 0x9e3779b9U));
  } else {
    return sequential ? Key(index) : Key(mix(index));
  }
}

template <class Map>
struct Factory {
  static Map* create(size_t n, bool presized) {
    Map* m = new Map;
    if (presized) m->reserve(n);
    return m;
  }
};
template <class K, class V, class H, class E>
struct Factory<roo_collections::FlatSmallHashMap<K, V, H, E>> {
  using Map = roo_collections::FlatSmallHashMap<K, V, H, E>;
  static Map* create(size_t n, bool presized) {
    return presized ? new Map(uint16_t(n)) : new Map;
  }
};
template <class K, class V, class C, class A>
struct Factory<std::map<K, V, C, A>> {
  using Map = std::map<K, V, C, A>;
  static Map* create(size_t, bool) { return new Map; }
};

template <class K, class V, class H>
struct Factory<hashmap_benchmark::EtlMap<K, V, H>> {
  using Map = hashmap_benchmark::EtlMap<K, V, H>;
  static Map* create(size_t n, bool) { return new Map(n); }
};

template <class Map>
void run_case(const char* name, const char* hashing, const char* key_name,
              const std::vector<typename Map::key_type>& hits,
              const std::vector<typename Map::key_type>& misses,
              const std::vector<uint16_t>& order) {
  const unsigned n = hits.size();
  using K = typename Map::key_type;
  using Pair = std::pair<K, uint32_t>;
  auto fill = [&](auto& m) {
    for (unsigned i = 0; i < n; ++i)
      m.insert(Pair(hits[order[i]], order[i] + 1));
  };
  const int before =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  {
    std::unique_ptr<Map> owner(Factory<Map>::create(n, false));
    auto& m = hashmap_benchmark::MapView(*owner);
    // LookupTest.Find checks absent entries before insertion, values after it.
    for (unsigned i = 0; i < n; ++i)
      check(m.find(hits[i]) == m.end(), "empty find");
    fill(m);
    const int heap = before - int(heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                                          MALLOC_CAP_8BIT));
    check(m.size() == n, "insert size");
    for (unsigned i = 0; i < n; ++i) {
      auto it = m.find(hits[i]);
      check(it != m.end() && it->second == i + 1, "find hit/value");
      check(m.find(misses[i]) == m.end(), "find miss/disjoint data");
      check(!m.insert(Pair(hits[i], 999)).second, "duplicate insertion");
    }
    {
      Map copy(*owner);
      check(copy.size() == n, "copy size");
      for (unsigned i = 0; i < n; ++i) {
        auto it = copy.find(hits[i]);
        check(it != copy.end() && it->second == i + 1, "copy value");
      }
    }
    auto record = [&](const char* workload, auto fn, unsigned units) {
      emit(name, hashing, key_name, n, workload, measure(units, fn), heap,
           hashmap_benchmark::ObjectBytes(*owner));
    };
    record(
        "find_hit",
        [&](unsigned batches) {
          uint32_t sum = 0;
          for (unsigned b = 0; b < batches; ++b) {
            barrier();
            for (unsigned j = 0; j < n; ++j) {
              auto it = m.find(hits[order[j]]);
              sum += it == m.end() ? 0 : it->second;
            }
          }
          sink = sum;
        },
        n);
    record(
        "find_miss",
        [&](unsigned batches) {
          uint32_t found = 0;
          for (unsigned b = 0; b < batches; ++b) {
            barrier();
            for (unsigned j = 0; j < n; ++j)
              found += m.find(misses[order[j]]) != m.end();
          }
          sink = found;
        },
        n);
    record(
        "iteration",
        [&](unsigned batches) {
          for (unsigned b = 0; b < batches; ++b) {
            barrier();
            for (auto it = m.begin(); it != m.end(); ++it) consume(*it);
          }
        },
        n);
    record(
        "copy_ctor",
        [&](unsigned batches) {
          for (unsigned b = 0; b < batches; ++b) {
            Map copy(*owner);
            consume(copy);
          }
        },
        n);
    // BM_EraseEmplace: delete/reinsert the same key, retaining a constant size.
    record(
        "erase_insert",
        [&](unsigned batches) {
          for (unsigned b = 0; b < batches; ++b) {
            for (unsigned i = 0; i < n; ++i) {
              const unsigned j = order[i];
              m.erase(hits[j]);
              m.insert(Pair(hits[j], j + 1));
            }
            consume(m);
          }
        },
        n);  // one unit = an erase + an insert
    check(m.size() == n, "erase/insert size");
    for (unsigned i = 0; i < n; ++i) {
      auto it = m.find(hits[i]);
      check(it != m.end() && it->second == i + 1, "erase/insert value");
    }
    if constexpr (hashmap_benchmark::IsStringKey<K>::value) {
      if (n >= 10) {
        // BM_CacheInSteadyState: 90 hits in the newest 10 entries, 10 misses,
        // evict oldest, insert new. Pre-generated keys exclude RNG/string
        // setup.
        std::vector<const K*> pool;
        pool.reserve(2 * n);
        for (const auto& key : hits) pool.push_back(&key);
        for (const auto& key : misses) pool.push_back(&key);
        unsigned oldest = 0;
        record(
            "cache_cycle",
            [&](unsigned batches) {
              uint32_t found = 0;
              for (unsigned b = 0; b < batches; ++b) {
                barrier();
                for (unsigned i = 0; i < 90; ++i)
                  found += m.find(*pool[(oldest + n - 1 - i % 10) % (2 * n)]) !=
                           m.end();
                for (unsigned i = 0; i < 10; ++i)
                  found += m.find(*pool[(oldest + n + i) % (2 * n)]) != m.end();
                m.erase(*pool[oldest]);
                unsigned next = (oldest + n) % (2 * n);
                m.insert(Pair(*pool[next], 1));
                oldest = (oldest + 1) % (2 * n);
              }
              sink = found;
            },
            1);  // one unit = 100 finds + erase + insert
        check(m.size() == n, "cache size");
        for (unsigned i = 0; i < 2 * n; ++i) {
          bool expected = (i + 2 * n - oldest) % (2 * n) < n;
          check((m.find(*pool[i]) != m.end()) == expected, "cache contents");
        }
      }
    }
  }
  // Time construction, insertion, and destruction, without the live source map.
  for (bool presized : {false, true}) {
    if constexpr (hashmap_benchmark::FixedCapacity<Map>::value) {
      if (!presized) continue;
    }
    int heap;
    unsigned object_bytes;
    {
      std::unique_ptr<Map> m(Factory<Map>::create(n, presized));
      fill(hashmap_benchmark::MapView(*m));
      object_bytes = hashmap_benchmark::ObjectBytes(*m);
      heap = before - int(heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                                  MALLOC_CAP_8BIT));
    }
    auto s = measure(n, [&](unsigned batches) {
      for (unsigned b = 0; b < batches; ++b) {
        std::unique_ptr<Map> m(Factory<Map>::create(n, presized));
        fill(hashmap_benchmark::MapView(*m));
        consume(*m);
      }
    });
    emit(name, hashing, key_name, n, presized ? "build_presized" : "build_grow",
         s, heap, object_bytes);
  }
  check(heap_caps_check_integrity_all(true), "heap integrity");
  int after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (after != before)
    Serial.printf("HEAP_DELTA,%s,%s,%s,%u,before=%d,after=%d\n", name, hashing,
                  key_name, n, before, after);
  check(after == before, "heap leak");
}

template <class Key>
void run_type(const char* key_name, int length = 0, bool sequential = false) {
  // Includes exact Abseil EraseEmplace / Iteration sizes and growth boundaries.
  const unsigned sizes[] = {1,  2,  4,   7,   8,   10,  15,  16,   32,
                            54, 64, 100, 128, 256, 400, 512, 1024, 2048};
  for (unsigned n : sizes) {
    // Inline 48-character fixtures leave too little contiguous memory for
    // old/new growing tables together at 512 entries on the classic ESP32.
    if constexpr (std::is_same<Key, roo_collections::SmallString<49>>::value) {
      if (n > 400) continue;
    }
    if (length && n > 512) continue;
    if (sizeof(Key) == 8 && n > 1024) continue;
    std::vector<Key> hits, misses;
    hits.reserve(n);
    misses.reserve(n);
    for (unsigned i = 0; i < n; ++i) {
      hits.push_back(make_key<Key>(i + 1, length, sequential));
      misses.push_back(make_key<Key>(i + 100000, length, sequential));
    }
    std::vector<uint16_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    uint32_t state = 0x12345678;
    for (unsigned i = n; i > 1; --i) {
      state = mix(state + i);
      std::swap(order[i - 1], order[state % i]);
    }
    // Rotate execution order by size and run to reduce systematic order bias.
    for (unsigned entry = 0; entry < 19; ++entry) {
      switch ((entry + n + run_number) % 19) {
        case 0:
          run_case<roo_collections::FlatSmallHashMap<Key, uint32_t>>(
              "roo", "default", key_name, hits, misses, order);
          break;
        case 1:
          run_case<absl::flat_hash_map<Key, uint32_t>>(
              "absl_flat", "default", key_name, hits, misses, order);
          break;
        case 2:
          run_case<std::unordered_map<Key, uint32_t>>(
              "std_unordered", "default", key_name, hits, misses, order);
          break;
        case 3:
          run_case<absl::node_hash_map<Key, uint32_t>>(
              "absl_node", "default", key_name, hits, misses, order);
          break;
        case 4:
          run_case<std::map<Key, uint32_t>>("std_map", "ordered", key_name,
                                            hits, misses, order);
          break;
        case 5:
          run_case<roo_collections::FlatSmallHashMap<Key, uint32_t,
                                                     absl::Hash<Key>>>(
              "roo", "absl_hash", key_name, hits, misses, order);
          break;
        case 6:
          run_case<absl::flat_hash_map<Key, uint32_t, absl::Hash<Key>>>(
              "absl_flat", "absl_hash", key_name, hits, misses, order);
          break;
        case 7:
          run_case<std::unordered_map<Key, uint32_t, absl::Hash<Key>>>(
              "std_unordered", "absl_hash", key_name, hits, misses, order);
          break;
        case 8:
          run_case<absl::node_hash_map<Key, uint32_t, absl::Hash<Key>>>(
              "absl_node", "absl_hash", key_name, hits, misses, order);
          break;
        case 9:
          run_case<tsl::robin_map<Key, uint32_t>>(
              "tsl_robin", "default", key_name, hits, misses, order);
          break;
        case 10:
          run_case<tsl::robin_map<Key, uint32_t, absl::Hash<Key>>>(
              "tsl_robin", "absl_hash", key_name, hits, misses, order);
          break;
        case 11:
          run_case<hashmap_benchmark::DenseMap<Key, uint32_t>>(
              "unordered_dense", "default", key_name, hits, misses, order);
          break;
        case 12:
          run_case<hashmap_benchmark::DenseMap<Key, uint32_t, absl::Hash<Key>>>(
              "unordered_dense", "absl_hash", key_name, hits, misses, order);
          break;
        case 13:
          run_case<hashmap_benchmark::EtlMap<Key, uint32_t>>(
              "etl", "default", key_name, hits, misses, order);
          break;
        case 14:
          run_case<hashmap_benchmark::EtlMap<Key, uint32_t, absl::Hash<Key>>>(
              "etl", "absl_hash", key_name, hits, misses, order);
          break;
        case 15:
          run_case<hashmap_benchmark::BoostMap<Key, uint32_t>>(
              "boost_flat", "default", key_name, hits, misses, order);
          break;
        case 16:
          run_case<hashmap_benchmark::BoostMap<Key, uint32_t, absl::Hash<Key>>>(
              "boost_flat", "absl_hash", key_name, hits, misses, order);
          break;
        case 17:
          run_case<emhash5::HashMap<Key, uint32_t>>(
              "emhash5", "default", key_name, hits, misses, order);
          break;
        case 18:
          run_case<emhash5::HashMap<Key, uint32_t, absl::Hash<Key>>>(
              "emhash5", "absl_hash", key_name, hits, misses, order);
          break;
      }
      delay(1);
    }
  }
}

void diagnose_large_copy() {
  using Map = absl::flat_hash_map<uint64_t, uint32_t, absl::Hash<uint64_t>>;
  std::vector<uint64_t> hits, misses;
  hits.reserve(2048);
  misses.reserve(2048);
  for (unsigned i = 0; i < 2048; ++i) {
    hits.push_back(make_key<uint64_t>(i + 1, 0, false));
    misses.push_back(make_key<uint64_t>(i + 100000, 0, false));
  }
  std::vector<uint16_t> order(2048);
  std::iota(order.begin(), order.end(), 0);
  uint32_t state = 0x12345678;
  for (unsigned i = 2048; i > 1; --i) {
    state = mix(state + i);
    std::swap(order[i - 1], order[state % i]);
  }
  std::unique_ptr<Map> owner(new Map);
  for (unsigned i : order) owner->insert(std::make_pair(hits[i], i + 1));
  Serial.printf(
      "LARGE_COPY,entries=%u,slots=%u,free=%u,largest=%u\n",
      unsigned(owner->size()), unsigned(owner->capacity()),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL |
                                                MALLOC_CAP_8BIT)));
  try {
    Map copy(*owner);
    consume(copy);
    Serial.println("LARGE_COPY,success");
  } catch (const std::bad_alloc&) {
    Serial.println("LARGE_COPY,bad_alloc");
  }
}

void warm_up_runtime() {
  // ESP-IDF lazily allocates a mutex and semaphore for C++ local-static
  // initialization. Exercise that shared runtime before any heap baseline;
  // otherwise the first container using a guarded static appears to leak.
  const int before = ESP.getFreeHeap();
  static const uint32_t initialized = esp_cpu_get_cycle_count();
  consume(initialized);
  Serial.printf("RUNTIME_INIT,static_guards,heap_bytes=%d\n",
                before - int(ESP.getFreeHeap()));
}

void benchmark(bool small_string = false) {
  warm_up_runtime();
  ++run_number;
  Serial.printf(
      "BEGIN,%u,chip=%s,revision=%u,cpu_mhz=%u,core=%u,heap=%u,psram=%u,"
      "compiler=%s,absl=%d.%d,group_width=%u,suite=%s,profile=extended\n",
      run_number, ESP.getChipModel(), ESP.getChipRevision(),
      getCpuFrequencyMhz(), xPortGetCoreID(), ESP.getFreeHeap(),
      ESP.getPsramSize(), __VERSION__, ABSL_LTS_RELEASE_VERSION,
      ABSL_LTS_RELEASE_PATCH_LEVEL,
      unsigned(absl::container_internal::Group::kWidth),
      small_string ? "small_string" : "general");
  Serial.println(
      "HEADER,run,map,hash,key,n,workload,min_ns,median_ns,max_ns,batches,heap_"
      "bytes,object_bytes");
  const int64_t start = esp_timer_get_time();
  if (small_string) {
    run_type<roo_collections::SmallString<13>>("small_string12", 12);
    run_type<roo_collections::SmallString<49>>("small_string48", 48);
  } else {
    run_type<uint32_t>("u32_random");
    run_type<uint32_t>("u32_sequential", 0, true);
    run_type<uint64_t>("u64_random");
    run_type<std::string>("string12", 12);
    run_type<std::string>("string48", 48);
  }
  Serial.printf("END,%u,elapsed_ms=%lld,sink=%u,heap=%u\n", run_number,
                (esp_timer_get_time() - start) / 1000, unsigned(sink),
                ESP.getFreeHeap());
  Serial.flush();
}
}  // namespace

void setup() {
  setCpuFrequencyMhz(240);
  Serial.begin(115200);
  delay(1000);
  heap_caps_register_failed_alloc_callback([](size_t requested, uint32_t caps,
                                              const char* fn) {
    esp_rom_printf("ALLOC_FAILED,bytes=%u,caps=%u,free=%u,largest=%u,fn=%s\n",
                   unsigned(requested), unsigned(caps),
                   unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                                    MALLOC_CAP_8BIT)),
                   unsigned(heap_caps_get_largest_free_block(
                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                   fn);
  });
  Serial.println(
      "READY,hashmap benchmark; send r for general, s for SmallString");
}
void loop() {
  if (Serial.available()) {
    char command = Serial.read();
    if (command == 'r') benchmark();
    if (command == 's') benchmark(true);
    if (command == 'd') diagnose_large_copy();
  }
  delay(10);
}
