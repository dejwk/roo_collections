#pragma once

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

#include "ankerl/unordered_dense.h"
#include "benchmark_keys.h"
#include "boost/unordered/unordered_flat_map.hpp"
#include "emhash/hash_table5.hpp"
#include "etl/string_view.h"
#include "etl/unordered_map.h"
#include "tsl/robin_map.h"

namespace hashmap_benchmark {

/// Uses the dense map's native hashing, including its string-view path.
template <typename Key>
struct DenseHashFor {
  using type = ankerl::unordered_dense::hash<Key>;
};

template <size_t N>
struct SmallStringDenseHash : ankerl::unordered_dense::hash<std::string_view> {
  uint64_t operator()(const roo_collections::SmallString<N>& key) const {
    return ankerl::unordered_dense::hash<std::string_view>{}(
        std::string_view(key.c_str(), key.length()));
  }
};

template <size_t N>
struct DenseHashFor<roo_collections::SmallString<N>> {
  using type = SmallStringDenseHash<N>;
};

template <typename Key>
using DenseHash = typename DenseHashFor<Key>::type;

/// Uses Boost's native hashing with a content adapter for SmallString.
template <typename Key>
struct BoostHashFor {
  using type = boost::hash<Key>;
};

template <size_t N>
struct SmallStringBoostHash {
  using is_avalanching =
      boost::hash_is_avalanching<boost::hash<std::string_view>>;

  size_t operator()(const roo_collections::SmallString<N>& key) const {
    return boost::hash<std::string_view>{}(
        std::string_view(key.c_str(), key.length()));
  }
};

template <size_t N>
struct BoostHashFor<roo_collections::SmallString<N>> {
  using type = SmallStringBoostHash<N>;
};

template <typename Key>
using BoostHash = typename BoostHashFor<Key>::type;

/// Uses ETL's native integer or string-view hash for the common fixtures.
template <typename Key>
struct EtlHash {
  size_t operator()(const Key& key) const {
    if constexpr (IsStringKey<Key>::value) {
      return etl::hash<etl::string_view>{}(
          etl::string_view(key.c_str(), key.length()));
    } else {
      return etl::hash<Key>{}(key);
    }
  }
};

template <typename Key, typename Value, typename Hash = DenseHash<Key>>
using DenseMap = ankerl::unordered_dense::map<Key, Value, Hash>;

template <typename Key, typename Value, typename Hash = BoostHash<Key>>
using BoostMap = boost::unordered_flat_map<Key, Value, Hash>;

/// Selects an actual fixed-capacity ETL map without duplicating the timing
/// loops. Native node storage and buckets are embedded in the selected map
/// object; its single allocation and this adapter are both included in measured
/// heap.
template <typename Key, typename Value, typename Hash = EtlHash<Key>>
class EtlMap {
 public:
  using key_type = Key;
  using Base = etl::iunordered_map<Key, Value, Hash>;
  using iterator = typename Base::iterator;
  static constexpr bool kFixedCapacity = true;

  /// Creates a native ETL map with exactly @p capacity nodes and buckets.
  explicit EtlMap(size_t capacity) {
    switch (capacity) {
#define ROO_BENCHMARK_ETL_SIZE(N) \
  case N:                         \
    initialize<N>();              \
    break
      ROO_BENCHMARK_ETL_SIZE(1);
      ROO_BENCHMARK_ETL_SIZE(2);
      ROO_BENCHMARK_ETL_SIZE(4);
      ROO_BENCHMARK_ETL_SIZE(7);
      ROO_BENCHMARK_ETL_SIZE(8);
      ROO_BENCHMARK_ETL_SIZE(10);
      ROO_BENCHMARK_ETL_SIZE(15);
      ROO_BENCHMARK_ETL_SIZE(16);
      ROO_BENCHMARK_ETL_SIZE(32);
      ROO_BENCHMARK_ETL_SIZE(54);
      ROO_BENCHMARK_ETL_SIZE(64);
      ROO_BENCHMARK_ETL_SIZE(100);
      ROO_BENCHMARK_ETL_SIZE(128);
      ROO_BENCHMARK_ETL_SIZE(256);
      ROO_BENCHMARK_ETL_SIZE(400);
      ROO_BENCHMARK_ETL_SIZE(512);
      ROO_BENCHMARK_ETL_SIZE(1024);
      ROO_BENCHMARK_ETL_SIZE(2048);
#undef ROO_BENCHMARK_ETL_SIZE
      default:
        std::abort();
    }
  }

  /// Copies the selected native map, preserving its fixed capacity.
  EtlMap(const EtlMap& other)
      : map_(other.copy_(other.map_)),
        destroy_(other.destroy_),
        copy_(other.copy_),
        object_bytes_(other.object_bytes_) {}

  EtlMap& operator=(const EtlMap&) = delete;

  /// Releases the native map and its embedded pool.
  ~EtlMap() { destroy_(map_); }

  /// Returns the native lookup cursor without an allocation or virtual call.
  iterator find(const Key& key) { return map_->find(key); }

  /// Returns the first occupied node.
  iterator begin() { return map_->begin(); }

  /// Returns the native end cursor.
  iterator end() { return map_->end(); }

  /// Returns the number of live entries.
  size_t size() const { return map_->size(); }

  /// Inserts a common fixture pair into the native ETL map.
  auto insert(std::pair<Key, Value> value) {
    return map_->insert(typename Base::value_type(std::move(value.first),
                                                  std::move(value.second)));
  }

  /// Removes the matching key, releasing its pool node for reuse.
  size_t erase(const Key& key) { return map_->erase(key); }

  /// Reports adapter plus native object bytes, including embedded node storage.
  size_t objectBytes() const { return object_bytes_; }

  /// Exposes the native size-independent interface outside the timing loop.
  Base& native() { return *map_; }

 private:
  template <size_t N>
  void initialize() {
    using Map = etl::unordered_map<Key, Value, N, N, Hash>;
    map_ = new Map;
    destroy_ = [](Base* map) { delete static_cast<Map*>(map); };
    copy_ = [](const Base* map) -> Base* {
      return new Map(*static_cast<const Map*>(map));
    };
    object_bytes_ = sizeof(EtlMap) + sizeof(Map);
  }

  Base* map_;
  void (*destroy_)(Base*);
  Base* (*copy_)(const Base*);
  size_t object_bytes_;
};

template <typename Map>
struct FixedCapacity : std::false_type {};

template <typename Key, typename Value, typename Hash>
struct FixedCapacity<EtlMap<Key, Value, Hash>> : std::true_type {};

/// Selects the native map before timing, avoiding adapter dispatch per
/// operation.
template <typename Map>
auto& MapView(Map& map) {
  if constexpr (FixedCapacity<Map>::value) {
    return map.native();
  } else {
    return map;
  }
}

/// Counts embedded storage for ETL and the native header for dynamic maps.
template <typename Map>
size_t ObjectBytes(const Map& map) {
  if constexpr (FixedCapacity<Map>::value) {
    return map.objectBytes();
  } else {
    return sizeof(Map);
  }
}

}  // namespace hashmap_benchmark
