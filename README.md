# roo_collections

Flat hash maps, sets, and record tables for memory-constrained C++ firmware.
Use them for small collections that see frequent key lookups, such as settings,
command dispatch tables, and MQTT topic indexes. Tested on ESP32 with Arduino
and ESP-IDF and on Raspberry Pi Pico.

## Why use roo_collections?

**Fast lookups on microcontrollers.** Roo keeps entries in a flat array and
provides expected constant-time lookup. The default hashes balance speed and
distribution with lightweight mixing for integer and enum keys and
content-based hashing for strings. In the general ESP32 benchmark, Roo
completed successful lookups in about 37% of Abseil flat's time across the
tested key types and sizes of 16 entries and up. In the SmallString suite,
where every map used the same inline key types, Roo took about 81% of Boost
flat's time and about the same time as emhash5 for successful lookups.

[General benchmark PDF](benchmarks/hashmap/reference_results/esp32/general/benchmark_charts.pdf) ·
[SmallString benchmark PDF](benchmarks/hashmap/reference_results/esp32/small_string/benchmark_charts.pdf) ·
[Results and measurement setup](benchmarks/hashmap/reference_results/esp32/README.md)

**Less RAM lost to capacity rounding.** Entries and bucket metadata share
one backing allocation. Roo chooses from closely spaced bucket counts, so a
size hint or a call to `compact()` can fit the table closely to its live
entries. The savings are especially noticeable just above a power-of-two
capacity boundary. A 100-entry Roo map can compact to 151 buckets. At the
same load limit, a power-of-two design needs 256. That gives Roo 41% fewer
bucket slots in this example, with room for a full key and value in each
slot.

**Flexible string keys and flash-resident tables.** Look up an owned string
using a literal, a string view, or an Arduino `String` without allocating a
temporary key. Use `SmallString` to keep bounded strings inline, or build an
immutable map at compile time and let the linker place it in flash. Code
that only reads a map can use the same interface with either a mutable or
an immutable owner.

### Trade-offs

As with most open-addressing maps that store entries directly in buckets,
a full scan visits empty buckets too. Excess capacity after growth or many
erases increases the work per live entry. A size hint or `compact()` can
reduce that cost. A dense-entry map such as `ankerl::unordered_dense` is
worth considering when full scans dominate or when large values make empty
slots expensive.

Mutable Roo tables use heap storage. Growth can briefly require both the old
and new allocations, and rebuilding can invalidate iterators and element
addresses. Consider `etl::unordered_map` if you need
fixed-capacity mutable storage, or `etl::flat_map` if you need sorted keys.

## Quick start

Use `FlatSmallHashMap<Key, Value>` for general keys, `FlatSmallHashSet<Key>`
for membership, and `FlatSmallStringHashMap<Value>` for owned string keys.
Pass a size hint when you know roughly how many entries the table will hold.
Mutable containers work with C++11 or later. The library depends on
`roo_backport`.

```cpp
#include "roo_collections/flat_small_hash_map.h"

int ReadTimeout() {
  roo_collections::FlatSmallStringHashMap<int> settings(16);
  settings.insert({"timeout", 30});

  roo::string_view key = "timeout";
  auto it = settings.find(key);
  return it == settings.end() ? 0 : it->second;
}
```

## String keys and values

`FlatSmallStringHashMap` and `FlatSmallStringHashSet` own their keys as
`std::string`. Their heterogeneous lookup accepts `std::string`,
`roo::string_view`, literals, `SmallString`, and `String` on Arduino. For
keys backed by literals or other long-lived storage, store `roo::string_view`
directly to avoid copying the characters. Borrowed characters must outlive
the table entries.

For bounded keys or values, `SmallString<N>` keeps the payload and its length
in an `N`-byte object. It avoids a separate heap allocation for each string
and makes the length available without scanning for a terminator. Choose
`N` to fit your longest key plus one byte. Longer inputs are truncated, so
keys with the same retained prefix compare equal. Use `view()` to access the
length-delimited payload.

```cpp
#include "roo_collections/flat_small_hash_map.h"
#include "roo_collections/small_string.h"

int ReadTemperature() {
  using SensorId = roo_collections::SmallString<16>;
  roo_collections::FlatSmallHashMap<SensorId, int> readings(8);
  readings.insert({SensorId("living-room"), 21});
  return readings.at(SensorId("living-room"));
}
```

## Immutable tables in flash

With C++17 or later, define a `static constexpr` map, set, or record table
for keys known at build time. The compiler builds the table, and the owner
has no table-owned heap allocation or runtime initialization. Linker
placement is target-dependent. On memory-mapped flash targets such as
ESP32, the firmware can look up entries directly in flash.

Immutable maps support `find()`, `at()`, and a read-only `operator[]`.
Use `find()` when a key might be absent. Both `at()` and `operator[]` require
an existing key. String-specialized maps store `roo::string_view` and
accept the same heterogeneous lookup types as mutable string maps.

```cpp
#include "roo_collections/immutable_flat_small_hash_map.h"

static constexpr roo_collections::ImmutableFlatSmallHashMap kRecords(
    roo_collections::WithMapping<int, int>({{1, 10}, {2, 20}}));

static constexpr roo_collections::ImmutableFlatSmallHashMap kCommands(
    roo_collections::WithStringMapping<int>(
        {{"start", 1}, {"stop", 2}, {"reset", 3}}));

static_assert(kRecords.at(2) == 20);
static_assert(kCommands.at("stop") == 2);
static_assert(kCommands["reset"] == 3);
```

Use `WithMapping<Key, Value>` for general keys and `WithStringMapping<Value>`
for string views. These helpers deduce the entry count from their input.
`ImmutableFlatSmallStringHashMap<Value, N>` is the explicit string-view
map type.

### Sets and custom entries

`ImmutableFlatSmallHashSet<Key, N>` provides membership and iteration.
Use `ImmutableFlatSmallStringHashSet<N>` for string views. Both are aliases
of `ImmutableFlatSmallHashtable`, which can deduce the key type and entry
count from the set helpers.

```cpp
#include "roo_collections/immutable_flat_small_hash_set.h"

static constexpr roo_collections::ImmutableFlatSmallHashtable kAllowed(
    roo_collections::WithElements<int>({1, 2, 3}));
static constexpr roo_collections::ImmutableFlatSmallHashtable kNames(
    roo_collections::WithStringElements({"start", "stop"}));

static_assert(kAllowed.contains(2));
static_assert(kNames.contains("stop"));
```

Use `ImmutableFlatSmallHashtable<Entry, Key, N>` to store application
records directly. Supply a key extractor to `WithEntries`.

```cpp
#include "roo_collections/immutable_flat_small_hashtable.h"

struct Setting {
  int id;
  int value;
};

struct SettingKey {
  constexpr const int& operator()(const Setting& entry) const {
    return entry.id;
  }
};

static constexpr roo_collections::ImmutableFlatSmallHashtable kSettings(
    roo_collections::WithEntries<Setting, int>(
        {{1, 100}, {2, 200}}, roo_collections::ImmutableHashFn<int>{},
        SettingKey{}));

static_assert(kSettings.find(2)->value == 200);
```

Use a namespace-scope or `static constexpr` owner for a persistent
compile-time table. String views and other borrowed data must outlive the
table. String literals are suitable. Immutable headers require C++17, so
guard their use with `#if __cplusplus >= 201703L` in code that also supports
older language standards. Including one in C++11/14 emits a warning and
skips its declarations.

## Common read-only interfaces

Use a `const ReadOnlyFlatSmallHashMap<Key, Value>&` in code that reads
settings from either a mutable map or a compile-time table. Pass the owner
directly. Matching owners share a non-virtual base, so the call needs no adapter or
copy. `ReadOnlyFlatSmallHashtable` and `ReadOnlyFlatSmallHashSet` provide
the same pattern for tables and sets.

```cpp
#include "roo_collections/read_only_flat_small_hash_map.h"

int ReadSetting(const roo_collections::ReadOnlyFlatSmallHashMap<int, int>& map,
                int key) {
  auto it = map.find(key);
  return it == map.end() ? -1 : it->second;
}
```

The base exposes const lookup and iteration. Map bases also expose
`at()` and `operator[]` for existing keys. The owner must outlive the
borrow. The base reference stays attached through insertion, erasure,
rehashing, clearing, and assignment and observes the owner's updated
storage. Iterators and entry references follow the owner's invalidation
rules. A reference to a moved-from owner still refers to that object.
Destroy concrete owners. Base construction, assignment, and destruction
are protected.

Mutable and immutable owners must have matching entry types, key types,
and policies. `ImmutableHashFn` aliases `DefaultHashFn`, so default-policy
owners with matching key and value types already qualify. To share an
interface with an immutable string map, use
`FlatSmallHashMap<roo::string_view, Value, TransparentStringHashFn, TransparentEq>`
as the mutable owner. The convenience alias `FlatSmallStringHashMap` owns
`std::string` keys.

## ESP32 comparison benchmark

The benchmark measures lookup, iteration, construction and copy, mutation,
and retained memory on a classic ESP32. It includes Abseil flat and node,
the standard maps, Tessil robin_map, unordered_dense, ETL, Boost flat, and
emhash5. The PDFs break down results by key type, table size, and hash
policy. For build and capture instructions, see the
[benchmark guide](benchmarks/hashmap/README.md).

## Implementation details

### Default hashing

Integral and enum keys up to 32 bits are converted to `uint32_t` and
multiplied by `0x9e3779b1` with 32-bit wraparound. Tables reduce the full
product to pick the initial bucket and use its high seven bits as a
fingerprint to filter key comparisons. For wider integral and enum keys,
the upper word is multiplied by `0x85ebca6b` and its product is added
to the lower word's product with 32-bit wraparound.

The integer hash results are the same on 32-bit and 64-bit targets. Changing
the hash can change iteration order. Mutable, immutable, and read-only tables
share the policy. Both the default integral and enum hash and the public
`Murmur3Fmix32(uint32_t)` in `roo_collections/hash.h` are `constexpr` in
C++14 and later and available at runtime in C++11. String keys use full
MurmurHash3. Other key types use `std::hash` by default. You can supply
your own hash and key equality policies.

Mixing makes lookup less sensitive to structured missing-key ranges. It
adds some hash computation and can increase probing for workloads whose
keys previously mapped to distinct buckets.

### Mutable capacity selection

Roo uses open addressing with quadratic probing. Each backing allocation
contains the entry array followed by one state byte per bucket. The state
byte distinguishes empty, erased, and occupied buckets and stores a
fingerprint for occupied ones.

Mutable tables, maps, and sets use the largest primes congruent to 3 modulo 4
below `2^(k/4)`, for `k = 1..64`. Repeated small values are omitted, leaving
50 candidate sizes including the one-slot empty sentinel. The primes and
their reciprocal descriptors each have one shared constant copy across
translation units. Each table stores a precomputed 32-bit reciprocal and
uses a multiply-high quotient estimate with signed correction to compute
exact modulo without division. Immutable tables use the same reducer with
their own constant bucket count.

A size hint selects the smallest candidate that holds the requested entry
count at 75% live occupancy, such as 151 buckets for 100 entries or 1447 for
1000 entries. Default construction reserves space for eight entries in
eleven buckets. Automatic growth doubles the usable entry capacity and
rounds up to a candidate. The final 65519-bucket candidate supports up to
64000 entries as a higher-occupancy exception.

Insertion checks the probe chain for duplicates before reusing an erased
bucket. The live-entry limit controls growth. A separate occupancy limit
lets tombstones use roughly half the remaining buckets before cleanup,
with at least one empty bucket so unsuccessful lookups terminate.
Repeatedly erasing and reinserting the same keys requires no rebuilds.
A same-capacity cleanup can rebuild through temporary storage. It retains
the original backing allocation. `clear()` also retains capacity, while
`compact()` selects the smallest candidate for the remaining live entries.

Iteration scans bucket metadata, so even a one-entry map can benefit from
a size hint. Caching `end()` outside a traversal can reduce repeated metadata
loads when the loop body might modify memory.

### Immutable construction and capacity

`WithEntries`, `WithMapping`, and `WithElements` return pointer-free
prepared arrays. The string helpers select transparent string-view
policies. Constructing the owner directly from a prepared array binds
the read-only header to its final inline storage and avoids GCC's
limitations on returning self-referential objects in constant evaluation.
An automatic `constexpr` owner cannot retain pointers to its own automatic
storage. Runtime locals and transient owners used within constant
evaluation are supported. Array and `std::array` constructors and the
owner-returning `MakeImmutable...` factories also support runtime use.

Helpers accept array references and `std::array`, including
`std::array<Entry, 0>{}` for an empty table. The number of entries is part
of the type. `capacity()` reports that count, and `ht_len()` reports
the bucket count. Immutable owners compute the smallest prime congruent
to 3 modulo 4 that provides at most 75% occupancy. For example, 100 entries
use 139 buckets. The constant bucket count and modulo reciprocal keep
prime search and division out of lookup. Immutable-only code needs no
list of candidate primes.

Empty tables use a one-slot sentinel. For more than 49139 entries, the
table uses 65519 buckets and supports up to 64000 entries as a
higher-occupancy exception.

Duplicate keys fail during constexpr construction and cause an abort in
runtime construction, even with `NDEBUG`. Entries must be default
constructible and copy assignable. Entry types can allocate internally.
For constant evaluation, entry operations, key extraction, hashing, and
equality must all be constexpr. `ImmutableHashFn` supports constexpr
hashing for integral and enum keys and `roo::string_view`. Supply a custom
constexpr hash for other keys as needed. Hash and equality must agree, and
hashes must match during construction and runtime lookup.

### Read-only lookup

Lookup through the base reads the owner's arrays and policies directly.
Each header holds the bucket count and 32-bit modulo reciprocal. Immutable
owners bind the header's pointers to their own inline arrays on construction,
copy, move, and assignment. Immutable moves copy the inline data and leave
both owners valid. Lookup on a concrete immutable owner uses the
compile-time bucket count directly.
