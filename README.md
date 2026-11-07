# roo_collections
Small, flat, fast, memory-conscious hash collections (hashtable, hashmap, hashset) for microcontrollers (e.g. in Arduino projects). Tested on ESP32 (Arduino and esp-idf) and Raspberry Pi Pico.

These collections keep the items in a single array, which tends to be ~2x the minimum size needed to store all the elements. It is ideal for small, frequently accessed hash containers, particularly when element deletion is infrequent.

These nice properties are achieved by sacrificing element address stability. That is, an address of an element added to the hashtable gets invalidated by subsequent unrelated inserts or deletions (as elements may be moved around during rehashing).

Hashtable authors obsess about performance. These hashtables have excellent performance at small sizes (up to ~5000 elements in a Linux benchmark), and decent performance outside of that range - but more importantly, their implementation is *simple* - they will be easy on your microcontroller image sizes.

These collections play nicely with strings. You can use strings as both keys and values. If your strings are constants (e.g. defined as compile-time `const char*` constants), you can use `roo::string_view` which ensures that no copies are made. If your lookup key is of a different string type
than the storage key, e.g. if you use `std::string` keys but use `const char*` or `roo::string_view` for the lookup, no dynamic allocation is done; the internal comparator functions work with heterogeneous types. Additionally, the library contains a templated SmallString implementation
which can be used to store small fixed-size strings inline (without using heap).

## Default hashing

Integral and enum keys use MurmurHash3's `fmix32` finalizer rather than identity
hashing. Keys up to 32 bits convert to `uint32_t` before mixing; 64-bit keys
XOR their mixed upper word into the lower word, then mix the result. All result
bits are the same on 32-bit and 64-bit targets. Hashes intentionally differ from
`std::hash`, and changing the hash can change iteration order.

`Murmur3Fmix32(uint32_t)` is available in `roo_collections/hash.h`. It and the
default integral/enum policy are `constexpr` in C++14 and later, and usable at
runtime in C++11. Mutable, immutable and read-only tables share this policy.
The finalizer is an unseeded integer mixer; it is not full MurmurHash3 of a
four-byte buffer. String keys continue to use full MurmurHash3, and other key
types use `std::hash` unless a custom policy is supplied.

Mixing makes lookup less sensitive to structured missing-key ranges, at the
cost of hash computation and potentially more probes for previously
collision-free keys. It does not guarantee faster lookups for every workload.

## Mutable capacity selection

Mutable tables, maps and sets use the largest primes congruent to 3 modulo 4
below `2^(k/4)`, for `k = 1..64`. Repeated small values are omitted, leaving 50
candidate sizes including the one-slot empty sentinel. The primes and their
reciprocal descriptors each have one shared constant copy across translation units.

A size hint selects the smallest candidate accommodating the requested entry
count at 75% occupancy. For example, a 100-entry hint uses 151 buckets instead
of the previous 251; a 1000-entry hint uses 1447 instead of 2039. Default
construction still reserves space for eight entries in eleven buckets.

Automatic growth doubles the usable entry capacity, then rounds up to a table
candidate. Tombstone cleanup retains the current capacity and backing storage;
`compact()` explicitly selects the smallest candidate for the remaining live
entries. `clear()` retains the allocated capacity. Iteration scans the bucket
array, so tighter hinted or compacted tables also scan fewer empty buckets.
The final 65519-bucket candidate retains the existing 64000-entry limit and
higher-occupancy exception.

Insertion reuses erased buckets after checking the probe chain for duplicates.
The live-entry limit controls growth; a separate occupancy limit allows
roughly half the remaining buckets to hold tombstones before cleanup. At
least one bucket stays empty so unsuccessful lookups terminate. Repeatedly
erasing and reinserting the same keys therefore requires no table rebuilds.

For very small known sizes, pass a size hint: the default eleven buckets still
need scanning even with one entry. Caching `end()` outside a traversal can
also reduce repeated metadata loads when the loop body might modify memory.

## Common read-only interfaces

`ReadOnlyFlatSmallHashtable`, `ReadOnlyFlatSmallHashMap` and
`ReadOnlyFlatSmallHashSet` accept both mutable and immutable owners with matching
entry/key types and policies. They are non-virtual bases, accessed by pointer
or reference. They expose const lookup and iteration; map specializations also
provide const `at()` and `operator[]`, which require an existing key.

```cpp
#include "roo_collections/read_only_flat_small_hash_map.h"

int ReadSetting(const roo_collections::ReadOnlyFlatSmallHashMap<int, int>& map,
                int key) {
  auto it = map.find(key);
  return it == map.end() ? -1 : it->second;
}
```

Pass either owner directly to `ReadSetting`; no view construction or copying is
needed. The owner must outlive every use of the reference. A reference remains
attached to the same owner through insertion, erasure, rehashing, clearing and
assignment, and observes its updated storage and size. Iterators and entry
references still follow the owner's usual invalidation rules. Moving from an
owner leaves its existing base references attached to the valid moved-from
object. Destroy concrete owners, never base pointers; base construction,
assignment and destruction are protected to prevent slicing and ownership loss.

Lookup reads the existing arrays and policies directly, without storage
callbacks or virtual calls. Each header points to constant metadata containing
the bucket count and modulo reciprocal. This replaces the old capacity index
without enlarging the header on 32-bit targets. Immutable owners bind storage
pointers to their own inline arrays on construction, copy, move and assignment.
Concrete immutable lookup retains its compile-time-capacity implementation.

The default `ImmutableHashFn` is an alias of `DefaultHashFn`, so default-policy
owners with matching key/value types share exactly the same base type. Stored
key types must match too: `FlatSmallStringHashMap` owns `std::string` keys,
whereas immutable string maps store `roo::string_view`. Use a regular
`FlatSmallHashMap<roo::string_view, Value, TransparentStringHashFn, TransparentEq>`
when it needs to share the string-view-keyed read-only base with an immutable
string map.

## Immutable tables in flash

Immutable headers expose their types and factories only with C++17 or later.
Including one in C++11/14 emits a compiler warning and skips its declarations;
mutable containers remain available. Guard uses of immutable APIs with
`#if __cplusplus >= 201703L` when supporting older language standards.

With C++17, `ImmutableFlatSmallHashtable<Entry, Key, N>` stores entries and
occupancy bytes inline and supports constant evaluation. It exposes const-only
lookup and iteration, with no table-owned heap allocation, resizing, or runtime
initialization for a `static constexpr` instance. Actual flash placement depends
on the target linker; memory-mapped flash platforms such as ESP32 can read it directly.

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

For maps, `ImmutableFlatSmallHashMap<Key, Value, N>` adds `at()` and a read-only
`operator[]`; both require an existing key and never insert. String-specialized
maps store `roo::string_view` keys and support heterogeneous lookup:

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

`WithMapping<Key, Value>(pairs)` prepares a map for general key types;
`WithStringMapping<Value>(pairs)` selects transparent string-view policies.
`ImmutableFlatSmallStringHashMap<Value, N>` is the explicit string-view
map type; it accepts string literals, `roo::string_view`, `std::string`,
`SmallString`, and Arduino `String` (when available) as lookup keys.

`ImmutableFlatSmallHashSet<Key, N>` exposes the same read-only membership and
iteration API as the hashtable. Its string-specialized alias is
`ImmutableFlatSmallStringHashSet<N>`:

```cpp
#include "roo_collections/immutable_flat_small_hash_set.h"

static constexpr roo_collections::ImmutableFlatSmallHashtable kAllowed(
    roo_collections::WithElements<int>({1, 2, 3}));
static constexpr roo_collections::ImmutableFlatSmallHashtable kNames(
    roo_collections::WithStringElements({"start", "stop"}));

static_assert(kAllowed.contains(2));
static_assert(kNames.contains("stop"));
```

`WithEntries`, `WithMapping`, and `WithElements` return pointer-free prepared
arrays for tables, maps, and sets. `WithStringMapping` and `WithStringElements`
select transparent string-view policies. Constructing the owner directly from
those arrays binds its read-only header to its final storage,
avoiding GCC's constexpr limitations on returning self-referential objects.
Use namespace-scope or `static constexpr` owners: a standalone automatic
`constexpr` owner cannot retain pointers to its own automatic storage. Ordinary
runtime locals and transient owners used inside constexpr evaluation are
supported. Explicit copy/move constructors and assignments rebind each owner's
header; immutable moves copy inline data and leave both owners valid.

Existing array/std::array constructors and `MakeImmutable...` owner-returning
factories remain available for runtime use. For portable constexpr construction,
use the `With...` construction form above. Set names remain aliases; deducing
`ImmutableFlatSmallHashtable` from set data produces the corresponding set
specialization without requiring an explicit entry count.

The helpers deduce the count from an array reference. `std::array` inputs are
also accepted, including `std::array<Entry, 0>{}` for an empty table. The entry
count is part of the type, and `capacity()` reports that fixed count; `ht_len()`
reports the bucket count. Immutable owners compute the smallest prime congruent
to 3 modulo 4 that provides at most 75% occupancy, using integer arithmetic and
compile-time prime search. For example, 100 entries use 139 buckets, compared
with 151 selected by a mutable table's size hint. The bucket count and
modulo reciprocal are constants; lookup performs no prime search or division,
and immutable-only code needs no stored list of candidate primes.

Empty tables retain their one-slot sentinel. Above 49139 entries, immutable
owners deliberately preserve the existing higher-occupancy exception: they use
65519 buckets and continue to support at most 64000 entries. Mutable tables use
the denser static candidates described above.

Duplicate keys are rejected: constexpr construction fails to compile, and
runtime construction aborts, including with `NDEBUG`. Entries must be default
constructible and copy assignable; entry types can still allocate internally.
For constant evaluation, their operations, the key extractor, hash and equality
functions must all be constexpr.
`ImmutableHashFn` provides constexpr hashing for integral/enum keys and
`roo::string_view`; other keys can supply their own hash. Hash and equality must
agree, and hashes must be identical during construction and runtime lookup.
String views and other borrowed data must outlive the table; string literals
are suitable. The pre-C++17 backported string view is not supported for
constexpr construction.

## Why use `roo_collections`? (vs. Alternatives)

When developing for memory-constrained embedded systems like the ESP32, developers typically choose between standard node-based maps (which cause heap fragmentation), static ordered arrays like `etl::flat_map`, or third-party flat hash maps.

`roo_collections` provides a highly optimized **flat hash map** (using quadratic probing). It gives you the contiguous memory benefits of a flat array, but with the **O(1)** lookup speeds of a hash map. 

Here is how `roo_collections` compares to the most common embedded alternatives:

### 🚀 Key Advantages

* **O(1) Constant Time Lookups:** Unlike `etl::flat_map` or `std::vector` which use O(log N) binary search, `roo_collections` uses hashing. It vastly outperforms binary search as your map grows to dozens or hundreds of elements.
* **Zero Heap Fragmentation Strings:** This is the killer feature for IoT devices. By utilizing `roo::string_view` (avoiding copies) and `SmallString` (storing small strings inline), `roo_collections` safely handles string keys (like MQTT topics, JSON keys, or Wi-Fi SSIDs) without thrashing the heap.
* **Cache-Friendly:** Elements are stored contiguously in memory, practically eliminating the CPU cache misses associated with standard `std::map` or `std::unordered_map`.
* **Fast Insertions:** Unlike Robin Hood hashing, quadratic probing requires zero memory-shifting or element-swapping during insertions, avoiding CPU spikes when populating the map.

### 🧠 Memory Efficiency & The "Load Factor"

All flat hash maps trade some RAM for speed by leaving empty slots in their arrays. `roo_collections` manages this exceptionally well for small data types.

* **Target Occupancy:** `roo_collections` normally allows **75%** live occupancy before growth. Ignoring bucket rounding, backing-array overhead is about **1.33x** just before growth and **2.67x** just after doubling. Discrete bucket sizes and small default reservations can increase this further.
* **Beats Standard Robin Hood:** Many popular Robin Hood maps (like `ska` or `tsl`) default to a 50% max load factor, meaning they fluctuate between 2x and 4x overhead. Out of the box, `roo_collections` operates with a tighter memory footprint.

### ⚖️ Trade-offs to Consider

* **Empty Slot Penalty for Large Objects:** Because `roo_collections` stores the actual Key/Value pairs in the main array, every empty slot wastes `sizeof(Key) + sizeof(Value)` bytes. If you are mapping large structs (e.g., 64-byte config objects), this dead space adds up. For heavy objects, a sparse/dense map implementation is more memory-efficient.
* **Heap Allocation:** It allocates its contiguous array on the heap. For strict safety-critical applications that forbid *any* heap allocation, a statically allocated `etl::flat_map` is required.
* **Unordered:** Because it relies on hashing, you cannot iterate through your keys in numerical or alphabetical order.

### Feature Comparison Matrix

| Feature | `roo_collections` | `ankerl::unordered_dense` | `ska` / `tsl` (Robin Hood) | `etl::flat_map` |
| :--- | :--- | :--- | :--- | :--- |
| **Data Structure** | **Flat Hash Map** | Sparse-Dense Hash Map | Flat Hash Map | Flat Ordered Array |
| **Lookup Speed** | **O(1)** | O(1) (requires 2 jumps) | O(1) | O(log N) |
| **Ordering** | **Unordered** | Unordered | Unordered | Sorted by Key |
| **Empty Slot Cost**| **`sizeof(pair)`** | `sizeof(uint32_t)` | `sizeof(pair)` | None (Zero overhead) |
| **Default Overhead** | **1.37x to 2.7x** | Medium (Sparse array) | 2.0x to 4.0x | Exact Capacity |
| **String Handling** | **Excellent (`SmallString`)**| Standard `std::string` | Standard `std::string` | Standard `std::string` |

### The Verdict: When should you use this?
Use **`roo_collections`** if your ESP32 firmware relies heavily on string keys (networking, MQTT, APIs), requires fast O(1) lookups, and stores small-to-medium-sized values (like integers, floats, or short strings). 

*If your dataset is incredibly small (< 20 items), strictly requires alphabetical iteration, or forbids heap usage, consider `etl::flat_map`. If you are mapping integers to massive memory-heavy structs, consider `ankerl::unordered_dense`.*