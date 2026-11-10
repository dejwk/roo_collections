#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

#include "benchmark_containers.h"

namespace {

static_assert(std::is_same<hashmap_benchmark::BoostHash<std::string>,
                           boost::hash<std::string>>::value);
static_assert(std::is_same<hashmap_benchmark::DenseHash<std::string>,
                           ankerl::unordered_dense::hash<std::string>>::value);
static_assert(
    boost::hash_is_avalanching<
        hashmap_benchmark::BoostHash<roo_collections::SmallString<13>>>::value);

template <typename Key>
struct CountingHash {
  inline static unsigned calls = 0;

  size_t operator()(const Key& key) const {
    ++calls;
    return std::hash<Key>{}(key);
  }
};

template <typename Key>
Key MakeKey(unsigned value) {
  if constexpr (hashmap_benchmark::IsStringKey<Key>::value) {
    return Key(std::to_string(value));
  } else {
    return Key(value);
  }
}

// Verifies duplicate insertion at full capacity, independent copies, iteration,
// and repeated erase/reinsert with all benchmark key representations.
template <typename Map>
void CheckMap(unsigned n) {
  using Key = typename Map::key_type;
  std::unique_ptr<Map> owner;
  if constexpr (hashmap_benchmark::FixedCapacity<Map>::value) {
    owner.reset(new Map(n));
  } else {
    owner.reset(new Map);
  }
  auto& map = hashmap_benchmark::MapView(*owner);
  for (unsigned i = 0; i < n; ++i) {
    assert(map.insert(std::make_pair(MakeKey<Key>(i), i + 1)).second);
  }
  assert(!map.insert(std::make_pair(MakeKey<Key>(0), 999U)).second);
  assert(map.find(MakeKey<Key>(n)) == map.end());
  {
    Map copy(*owner);
    assert(copy.size() == n);
    copy.erase(MakeKey<Key>(0));
    assert(map.find(MakeKey<Key>(0))->second == 1);
    unsigned visited = 0;
    for (auto it = copy.begin(); it != copy.end(); ++it) {
      ++visited;
    }
    assert(visited == n - 1);
  }
  for (unsigned cycle = 0; cycle < 4; ++cycle) {
    for (unsigned i = 0; i < n; ++i) {
      assert(map.erase(MakeKey<Key>(i)) == 1);
      assert(map.insert(std::make_pair(MakeKey<Key>(i), i + 1)).second);
      assert(map.find(MakeKey<Key>(i))->second == i + 1);
    }
  }
}

template <typename Key>
void CheckKeys() {
  for (unsigned n : {1, 4, 16, 100, 128}) {
    CheckMap<tsl::robin_map<Key, uint32_t>>(n);
    CheckMap<hashmap_benchmark::DenseMap<Key, uint32_t>>(n);
    CheckMap<hashmap_benchmark::BoostMap<Key, uint32_t>>(n);
    CheckMap<emhash5::HashMap<Key, uint32_t>>(n);
    CheckMap<hashmap_benchmark::EtlMap<Key, uint32_t>>(n);
  }
}

// Verifies every competitor honors an explicitly supplied hash function.
template <typename Map>
void CheckCustomHash() {
  using Hash = CountingHash<typename Map::key_type>;
  Hash::calls = 0;
  CheckMap<Map>(16);
  assert(Hash::calls > 0);
}

template <typename Key>
void CheckCustomHashes() {
  using Hash = CountingHash<Key>;
  CheckCustomHash<tsl::robin_map<Key, uint32_t, Hash>>();
  CheckCustomHash<hashmap_benchmark::DenseMap<Key, uint32_t, Hash>>();
  CheckCustomHash<hashmap_benchmark::BoostMap<Key, uint32_t, Hash>>();
  CheckCustomHash<emhash5::HashMap<Key, uint32_t, Hash>>();
  CheckCustomHash<hashmap_benchmark::EtlMap<Key, uint32_t, Hash>>();
}

}  // namespace

int main() {
  CheckKeys<uint32_t>();
  CheckKeys<uint64_t>();
  CheckKeys<std::string>();
  CheckKeys<roo_collections::SmallString<13>>();
  CheckKeys<roo_collections::SmallString<49>>();
  CheckCustomHashes<uint32_t>();
  CheckCustomHashes<std::string>();
  CheckCustomHashes<roo_collections::SmallString<13>>();
}
