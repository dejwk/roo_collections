# ESP32 hash map benchmark

Compare `roo_collections::FlatSmallHashMap`, `absl::flat_hash_map`,
`std::unordered_map`, `absl::node_hash_map`, `std::map`, `tsl::robin_map`,
`ankerl::unordered_dense::map`, `etl::unordered_map`,
`boost::unordered_flat_map`, and `emhash5::HashMap` on a classic ESP32.
The general workloads preserve the prototype fixtures; a separate SmallString
suite adds inline string keys. The general tools live beside the library. A compact
[ESP32 reference snapshot](reference_results/esp32/README.md) includes validated
captures, provenance and comparative PDFs, with Roo (SmallString) included in the
general string charts. Ordinary build directories, dependencies, captures, CSV
tables, chart galleries and firmware backups are Git-ignored.

The benchmark dependencies are separate from the library's normal dependencies.
No device is flashed by dependency setup, building, analysis, plotting or
packaging. Upload and full-flash restore are explicit commands using a selected
serial port. Original full-flash backups stay local under `.pio/`.

## Workloads

The suite adapts Abseil 20250512.1's
[raw_hash_set_benchmark.cc](https://github.com/abseil/abseil-cpp/blob/20250512.1/absl/container/internal/raw_hash_set_benchmark.cc)
(`BM_EraseEmplace`, `BM_Iteration`, copy/range construction and cache churn) and
[unordered_map_lookup_test.h](https://github.com/abseil/abseil-cpp/blob/20250512.1/absl/container/internal/unordered_map_lookup_test.h)
(miss-before-insert and present/value-after-insert checks). See [NOTICE](NOTICE)
and [LICENSE.abseil](LICENSE.abseil) for attribution. This is an adaptation to a
common map API, not the complete upstream benchmark or unit-test suite.

The extended profile has 11,553 cases per complete general suite. Keys are deterministic random uint32,
sequential uint32, random uint64, and printable strings of 12 and 48 bytes;
mapped values are uint32. Hit and miss sets are disjoint and visits/insertion
use a reproducible shuffle. Each hash map runs with its default hash and with
`absl::Hash<Key>` explicitly supplied; `std::map` uses ordering. Shared hashing
does not normalize hasher type traits or container capacity/load policies.

The separate extended `small_string` suite has 4,460 cases. It uses
`roo_collections::SmallString<13>` and `SmallString<49>` for the same 12- and
48-character payloads (capacities include the trailing null). Fixture bytes,
visit order, sizes, correctness checks, timing and workloads match the string
portion of the general suite, including cache churn. The 48-character fixture stops at 400 entries: at 512,
Roo growth needed a 57,064-byte block while only 55,284 contiguous bytes remained
with both query vectors and the old table alive (147,608 bytes free in total).
The 12-character fixture still reaches 512 entries. This is a fixture/peak
contiguous-memory limit, not a final-size limit for a presized table.
`SmallString` stores keys
inline; its current length operation scans for the terminator.

Benchmark-only adapters in `src/benchmark_keys.h` provide lexicographic ordering
for `std::map`, `std::hash<std::string_view>` for `std::unordered_map`, and an
`AbslHashValue` hook delegating to `absl::string_view`. Roo's default continues
to use its native SmallString hash. All nine shared-hash containers use
`absl::Hash<SmallString<N>>`. These adapters are not library API additions.

Both suites are in one firmware. Send `r` for general or `s` for SmallString;
with the capture tool, select `--suite small_string`. The `BEGIN` record names
the suite. Analysis automatically selects its complete expected case set and
rejects mixed suites. Legacy captures without a suite tag remain general.
Compare candidates within the same suite and harness; do not treat old general
and newly compiled SmallString timings as a controlled string-type comparison.

Sizes are 1, 2, 4, 7, 8, 10, 15, 16, 32, 54, 64, 100, 128, 256, 400, 512,
1024 and 2048. Uint64 cases stop at 1024 and string cases at 512 because fixtures,
source tables and copies must fit in contiguous internal memory. This is a
fixture limit, not the maximum capacity of a standalone container.

Timing uses the CPU cycle counter at 240 MHz, seven samples and a median after
calibrating batches to at least 3 ms (up to 65,536 batches). Serial output,
fixture generation and correctness/heap checks are outside timed intervals.
Interrupts remain enabled; repeated sweeps are warm accesses. Wi-Fi/Bluetooth
are not started and PSRAM is not used. SDK heap instrumentation affects
allocation costs and is recorded in metadata when discoverable.

Before the suite's heap baseline, the harness exercises guarded C++ local-static
initialization. ESP-IDF lazily allocates shared synchronization objects for this
runtime facility; otherwise the first container using it would appear to leak.
The transcript records that one-time cost as `RUNTIME_INIT`. Per-map retained
heap excludes this shared runtime cost; per-map and whole-suite heap-release
checks remain enabled.

- `find_hit`, `find_miss`, `iteration`: ns per queried/visited element.
- `copy_ctor`: copy construction and destruction, ns per source element.
- `erase_insert`: ns per erase+insert pair, including table maintenance.
- `build_grow`, `build_presized`: allocation/construction, insertion and
  destruction, ns per entry; presizing is included in its measured cost.
- `cache_cycle`: ns per whole cycle of 90 hits, 10 misses, eviction and insertion
  for string maps of at least 10 entries, using a circular pool of 2N keys.
- `heap_bytes`: retained internal 8-bit heap, including the map object,
  allocations and owned strings, excluding shared query fixtures. This does not
  measure peak memory or fragmentation under arbitrary object lifetimes.

The suite checks duplicates, hit values, absent keys, post-churn contents, heap
integrity and release of allocations. It exercises mutable `find`, not immutable
owners or shared read-only interfaces. One seed, one board and one suite do not
establish confidence intervals or a universal ranking.

## Additional competitors

All new sources are fetched into ignored storage using archive SHA-256 pins in
`dependencies.json`; none are vendored into Git. Boost 1.90.0 uses only the
header modules needed by Unordered, rather than the full distribution.

| ID | Upstream implementation | Pinned version | Capacity policy |
|---|---|---|---|
| `tsl_robin` | [Tessil robin_map](https://github.com/Tessil/robin-map) | 1.4.1 | Native default growth/load factor |
| `unordered_dense` | [ankerl unordered_dense](https://github.com/martinus/unordered_dense) | 5.3.3 | Dense values plus a grouped fingerprint/index array |
| `etl` | [ETL unordered_map](https://www.etlcpp.com/docs/containers/maps/unordered-map/) | 20.49.0 | Exactly N nodes and N buckets, fixed capacity |
| `boost_flat` | [Boost unordered_flat_map](https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered.html) | 1.90.0 | Native default growth/load factor |
| `emhash5` | [emhash hash_table5](https://github.com/ktprime/emhash) | 1.3.0 | Native default growth/load factor |

The benchmark selects native hash types for standard key types. Benchmark-only
SmallString adapters use each library's string-content path: std::string_view
for the standard hasher, unordered_dense and Boost, and etl::string_view for ETL
(which also needs that adapter for std::string). Native avalanche traits are
preserved. Explicit Abseil hashing is supplied to all nine hash maps; their own
hash postprocessing, capacity and load policies remain intact.

ETL has no dynamic-growth workload. Its rows omit `build_grow`; charts and tables
leave that series absent rather than treating it as a zero or a growth result.
Its presized build measures allocation, construction, filling and destruction of
an actual `etl::unordered_map<Key, Value, N, N, Hash>` for each fixture N. An owning
adapter selects the capacity without duplicating the timing loops. Lookup,
iteration and mutation use the native size-independent ETL interface directly,
selected before timing; copy/construction include adapter ownership overhead.
Both retained heap and object bytes include the adapter and the entire native
object, including its embedded pool and bucket array. Allocating these objects
on the heap also keeps large ETL copies off the ESP32 task stack.

The expanded firmware uses `huge_app.csv` (a 3 MiB application slot on a 4 MiB
ESP32); the default 1.25 MiB slot is too small for all containers. Full-flash
backup/restore preserves the original partition layout along with its firmware.
The fixture sizes and timing loop settings are unchanged.

`BEGIN` now records `profile=extended`. Untagged historical captures remain the
classic five-container profile (5,553 general / 2,142 SmallString cases).
Analysis rejects mixed or incomplete profiles. Charts infer the exact roster
from validated CSVs, so existing reference bundles remain readable.

## Setup and build

Run commands from this directory, using Python 3.10 or later:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
python scripts/fetch_dependencies.py
pio run -j 2
pio run -t compiledb
python scripts/metadata.py --output-dir results/my-run --board-label "my ESP32"
```

On Windows activate `.venv\Scripts\Activate.ps1` instead; source linking requires
symlink support (for example, Windows Developer Mode). PlatformIO pins
pioarduino 55.03.37 (Arduino 3.3.7); `dependencies.json` pins Abseil 20250512.1
and roo_backport 1.2.4 with SHA-256 checksums. The Python requirements also pin
standalone esptool 5.1.0 for the backup/restore commands; PlatformIO manages its
own uploader separately. Downloads go under `.pio/` and the user-level
PlatformIO package cache, outside Git history.

The library under test defaults to this repository. Set
`ROO_COLLECTIONS_SOURCE_DIR` to test a separate checkout or frozen snapshot.
`ABSL_SOURCE_DIR` and `ROO_BACKPORT_SOURCE_DIR` select local dependency sources;
their declared versions must match the pins, and source hashes identify local
edits. For an already-installed platform, an ignored `platformio.local.ini`
can override only the platform location:

```ini
[env:esp32dev]
platform = /absolute/path/to/installed/espressif32
```

The dependency hook forces the benchmark translation unit to rebuild, avoiding
stale objects when switching source symlinks or snapshots with old timestamps.
Metadata derives versions from installed packages and records source and firmware
hashes. A post-build receipt binds the image to source/configuration fingerprints;
metadata rejects later edits even when a snapshot preserves old timestamps. Run metadata immediately
after build/compiledb and before editing measured sources. A dirty tree's hashes
identify measured bytes; publish clean commits when checkout-based reproduction
is needed, or retain the corresponding source patch separately.

## Capture on an explicitly selected board

Replace `PORT` with your board's serial port. These commands temporarily replace
its firmware. First save the complete original flash; the example assumes a
4 MiB device, configurable with the backup helper's `--size` option.

```sh
python scripts/backup_flash.py --port PORT --baud 460800 \
  --output .pio/original-flash.bin
pio run -t upload --upload-port PORT
python scripts/capture.py --port PORT --runs 1 \
  --output results/my-run/esp32.log
# Separate SmallString run, with the same flashed firmware:
python scripts/metadata.py --output-dir results/small-string --board-label "my ESP32"
python scripts/capture.py --port PORT --runs 1 --suite small_string \
  --output results/small-string/esp32.log
```

The firmware waits for `r` or `s`; capture requests the selected suite and preserves `BEGIN`, `ROW`,
`END` records. Capture refuses to overwrite an existing transcript. The default
is one suite. A comparison can reuse a previous validated baseline; there is no
automatic baseline run or source modification.

After the run, restore and verify the original flash using the installed
esptool, independently of benchmark timing:

```sh
python -m esptool --port PORT --baud 460800 --after no-reset \
  write-flash 0 .pio/original-flash.bin
python -m esptool --port PORT --after hard-reset \
  verify-flash 0 .pio/original-flash.bin
```

The backup helper uses one in-flight block and independently verified segments
to tolerate USB/IP transfers. Its `.partial` file belongs to that exact device
and unchanged firmware; use distinct output paths for other boards.

## Analyze, compare and plot

```sh
python scripts/analyze.py results/my-run/esp32.log --output-dir results/my-run
python scripts/compare.py --baseline results/baseline --updated results/my-run
python -m pip install -r scripts/requirements-charts.txt
python scripts/plot.py --results-dir results/my-run

# Analyze and plot the separate SmallString run:
python scripts/analyze.py results/small-string/esp32.log --output-dir results/small-string
python scripts/plot.py --results-dir results/small-string
# Also show Roo (SmallString) beside std::string containers in the general charts:
python scripts/plot.py --results-dir results/my-run \
  --small-string-results results/small-string
```

`--small-string-results` adds a separate Roo (SmallString) series to every
string panel, including the overview, lookup, iteration/mutation, construction,
cache and memory charts. It uses the matching payload length and hash policy,
retains the captured size range (48-character SmallString stops at 400), and
records both input sets in the chart manifest. Integer panels are unchanged.
The tool requires matching library/dependency source hashes and recorded build
settings. Captures can come from different boards or harnesses; the charts label
them as separate runs, so timing differences also include board and binary-layout
effects. The standalone SmallString PDF continues to compare all ten containers
using SmallString keys.

Analysis requires complete coverage, valid numeric fields, ordered samples and
matching start/end heap. It can recover missing record separators, but never
repairs payloads. Partial-suite recovery exists only through explicit advanced
options and is unsuitable for the default publishing workflow. Archives require
strict complete-suite validation. Plots include an HTML gallery, PNG/SVG, PDF
and input hashes; they need no hardware access.

The comparator checks actual flags, SDK/compiler versions, fixtures and recorded
non-Roo sources before producing per-case differences and unchanged-container
controls. Legacy manifests using `prototypes/hashmap_benchmark/src/main.cpp`
are supported by normalizing its path, while still requiring the same source
hash. Renaming the file does not bypass a changed-harness check.

## Compact result archives

The checked-in [reference snapshot](reference_results/esp32/README.md) keeps two
compact raw bundles and their comparative PDFs in Git; its manifest records file
checksums and source revisions. Follow its instructions to regenerate full tables
and galleries locally. Other runs can be published as release assets, with download
URLs in `results-index.json` (currently empty). Package a selected validated run:

```sh
python scripts/bundle.py pack --results results/my-run --id my-run \
  --output artifacts/my-run.tar.gz
```

Only `build_metadata.json`, the selected `.log` transcript(s), and a checksum
manifest are included. Use repeated `--log PATH` arguments for multiple suites.
Derived CSVs/reports/charts, firmware binaries and original device backups are
not included. A typical raw transcript and metadata compress to about 90 KB.
The command writes a small sibling `.index.json` entry; `--url HTTPS_URL` can
supply the eventual release-asset URL. Uploading is separate and explicit.

After publishing an asset, add its generated entry with the real URL to the
`runs` array in `results-index.json`. Keep that small index in Git. Fetching
checks the archive SHA-256 and each member, validates the complete capture and
regenerates CSV/report files:

```sh
python scripts/bundle.py fetch my-run --output results/my-run
# Or restore a local archive:
python scripts/bundle.py unpack artifacts/my-run.tar.gz --output results/restored
python scripts/plot.py --results-dir results/restored
```

Outputs refuse to overwrite existing captures/results. The archive workflow
works without Matplotlib until plots are requested. It can package historical
complete prototype runs without rewriting their metadata or raw bytes.

## Tool tests

```sh
python -m unittest discover -s tests -v
```

Tests generate small synthetic fixtures in temporary storage; no board,
downloaded dependencies, stored result history or plotting packages are needed.

After dependency setup, validate the SmallString adapters with a host C++17
compiler (adjust the include paths if using dependency source overrides):

```sh
c++ -std=c++17 -fno-exceptions -fno-rtti -Isrc -I../../src \
  -I.pio/dependencies/abseil-20250512.1 \
  -I.pio/dependencies/roo_backport-1.2.4/src \
  tests/keys_test.cpp -o .pio/keys_test
.pio/keys_test
```

Validate all five additional containers and adapters on the host after dependency
setup (GCC/Clang-compatible C++17 compiler):

```sh
python scripts/test_containers.py
# Optional sanitizer run:
python scripts/test_containers.py --sanitize
```

These tests cover native and explicit hashers, string/integer keys, duplicates at
full fixed capacity, independent copies, iteration and repeated erase/insert.
The explicit-hasher checks catch implementations/configurations that bypass the
supplied hash function. Device captures also check values, misses, copy contents,
churn contents, heap integrity and full release after every fixture.
