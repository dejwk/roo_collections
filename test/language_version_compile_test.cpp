#include "roo_collections/flat_small_hash_map.h"
#include "roo_collections/flat_small_hash_set.h"
#include "roo_collections_immutable_flat_small_hash_map.h"
#include "roo_collections_immutable_flat_small_hash_set.h"
#include "roo_collections_immutable_flat_small_hashtable.h"

// Verifies each target actually compiles in its requested language mode.
static_assert(__cplusplus == ROO_COLLECTIONS_TEST_CXX,
              "Language compatibility test compiled with the wrong standard");

#if __cplusplus >= 201402L

// Verifies the mixer and default integer hash remain constexpr in C++14+.
static_assert(roo_collections::Murmur3Fmix32(1) == 0x514e28b7u,
              "constexpr integer mixer");
static_assert(roo_collections::DefaultHashFn<int>()(1) == 0x514e28b7u,
              "constexpr default integer hash");

#endif

#if __cplusplus >= 201703L

// Verifies immutable types, factories and constexpr lookup remain available
// when the required language version is enabled.
constexpr auto kTable =
    roo_collections::MakeImmutableFlatSmallHashtable<int>({1, 2});
constexpr auto kMap = roo_collections::MakeImmutableFlatSmallHashMap<int, int>(
    {{1, 10}, {2, 20}});
constexpr auto kSet =
    roo_collections::MakeImmutableFlatSmallHashSet<int>({1, 2});
constexpr auto kStringMap =
    roo_collections::MakeImmutableFlatSmallStringHashMap<int>(
        {{"on", 1}, {"off", 0}});
constexpr auto kStringSet =
    roo_collections::MakeImmutableFlatSmallStringHashSet({"on", "off"});
static_assert(kTable.contains(2), "immutable table available");
static_assert(kMap.at(2) == 20, "immutable map available");
static_assert(kSet.contains(2), "immutable set available");
static_assert(kStringMap.at("on") == 1, "immutable string map available");
static_assert(kStringSet.contains("off"), "immutable string set available");

#endif

// Verifies mutable containers and both hash entry points remain usable in
// C++11/14 after including all immutable headers (which only warn there).
int main() {
  roo_collections::FlatSmallHashMap<int, int> map(1);
  map[1] = 10;
  const roo_collections::FlatSmallHashMap<int, int>& const_map = map;
  if (const_map.at(1) != 10 || const_map.contains(2)) return 1;

  roo_collections::FlatSmallHashSet<int> set;
  set.insert(1);
  if (!set.contains(1) || set.contains(2)) return 2;

  roo_collections::FlatSmallStringHashMap<int> strings;
  strings["on"] = 1;
  if (strings.at(roo::string_view("on")) != 1) return 3;

  const char* key = "foo";
  if (roo_collections::Murmur3_32(key, 3, 0) != 0xf6a5c420u) return 4;
  if (roo_collections::murmur3_32(key, 3, 0) != 0xf6a5c420u) return 5;
  if (roo_collections::Murmur3Fmix32(1) != 0x514e28b7u) return 6;
  if (roo_collections::DefaultHashFn<int>()(1) != 0x514e28b7u) return 7;
  return 0;
}
