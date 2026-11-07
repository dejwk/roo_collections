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
runtime in C++11. Mutable and immutable tables share this policy.
The finalizer is an unseeded integer mixer; it is not full MurmurHash3 of a
four-byte buffer. String keys continue to use full MurmurHash3, and other key
types use `std::hash` unless a custom policy is supplied.

Mixing makes lookup less sensitive to structured missing-key ranges, at the
cost of hash computation and potentially more probes for previously
collision-free keys. It does not guarantee faster lookups for every workload.

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

static constexpr auto kSettings =
    roo_collections::MakeImmutableFlatSmallHashtable<Setting, int>(
        {{1, 100}, {2, 200}}, roo_collections::ImmutableHashFn<int>{},
        SettingKey{});

static_assert(kSettings.find(2)->value == 200);
```

For maps, `ImmutableFlatSmallHashMap<Key, Value, N>` adds `at()` and a read-only
`operator[]`; both require an existing key and never insert. String-specialized
maps store `roo::string_view` keys and support heterogeneous lookup:

```cpp
#include "roo_collections/immutable_flat_small_hash_map.h"

static constexpr auto kCommands =
    roo_collections::MakeImmutableFlatSmallStringHashMap<int>(
        {{"start", 1}, {"stop", 2}, {"reset", 3}});

static_assert(kCommands.at("stop") == 2);
static_assert(kCommands["reset"] == 3);
```

`MakeImmutableFlatSmallHashMap<Key, Value>(pairs)` builds a map for general key
types. `ImmutableFlatSmallStringHashMap<Value, N>` is the explicit string-view
map type; it accepts string literals, `roo::string_view`, `std::string`,
`SmallString`, and Arduino `String` (when available) as lookup keys.

`ImmutableFlatSmallHashSet<Key, N>` exposes the same read-only membership and
iteration API as the hashtable. Its string-specialized alias is
`ImmutableFlatSmallStringHashSet<N>`:

```cpp
#include "roo_collections/immutable_flat_small_hash_set.h"

static constexpr auto kAllowed =
    roo_collections::MakeImmutableFlatSmallHashSet<int>({1, 2, 3});
static constexpr auto kNames =
    roo_collections::MakeImmutableFlatSmallStringHashSet({"start", "stop"});

static_assert(kAllowed.contains(2));
static_assert(kNames.contains("stop"));
```

The factory deduces the count from an array reference. `std::array` inputs are
also accepted, including `std::array<Entry, 0>{}` for an empty table. The entry
count is part of the type, and `capacity()` reports that fixed count; `ht_len()`
reports the bucket count. At most 64000 entries are supported.

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

* **High Max Capacity:** `roo_collections` uses a maximum load factor of **73%** before it doubles in size. This means its memory overhead fluctuates between **~1.37x** (right before a rehash) and **~2.74x** (worst-case, right after a rehash). 
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