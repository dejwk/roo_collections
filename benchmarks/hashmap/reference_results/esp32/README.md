# ESP32 reference results — ten-container comparison

Roo was remeasured on 2026-11-20 with multiplicative hashing for wide
integer keys, combined entry and state storage, direct copy construction for
trivial-field pairs, and direct placement during rehash. The table uses a 75%
fill limit. The aligned Roo fixtures use length-delimited `SmallString<16>`
and `SmallString<52>`. The nine competitors retain their original 2026-11-10 captures.

- [General comparison PDF](general/benchmark_charts.pdf): all ten containers;
  **Roo (SmallString)** is an additional series in every string panel.
- [SmallString comparison PDF](small_string/benchmark_charts.pdf): all ten
  containers use SmallString keys.
- [Detailed general tables](general/REPORT.md) and [SmallString tables](small_string/REPORT.md), including Roo before/after ratios.
- [Provenance and checksums](manifest.json).
- Original full captures: [general](general/raw.tar.gz), [SmallString](small_string/raw.tar.gz).
- Fresh Roo-only captures: [general](general/roo-update.tar.gz), [SmallString](small_string/roo-update.tar.gz).

Each PDF contains 11 pages: overview; lookup, construction/copy,
iteration/mutation and cache charts for both hash policies; retained memory;
and presizing. The dashed/hatched Roo (SmallString) series has 12- and
48-character inline keys. Its 48-character fixture ends at 400 entries.

## Capture and interpretation

| Suite | Device | Comparison cases | Fresh Roo cases | Roo passes |
|---|---|---:|---:|---:|
| General | ESP32-D0WD-V3 revision 3.1, COM18 (`/dev/ttyUSB1`) | 11,553 | 1,234 | 3 |
| SmallString | ESP32-D0WDQ6 revision 1.1, COM20 (`/dev/ttyUSB0`) | 4,460 | 476 | 3 |

The same board pairing as the original comparison was used. There are 5,130 new
Roo observations, each with seven timed samples, and zero discarded rows. All
device correctness, per-map heap-release and whole-suite heap checks passed.
Both original firmware images were restored and independently verified over the
full 4 MiB flash contents afterward. Competitors have one original observation
per case; Roo points now use the median of three run medians.
The measured settings are 240 MHz, internal RAM without PSRAM, C++17, `-O2`,
`-DNDEBUG`, pioarduino 55.03.37, Arduino 3.3.7, GCC 14.2.0 and light heap poisoning.

Native default hashing and explicitly supplied `absl::Hash<Key>` are separate
series. Native hash postprocessing, type traits and load/capacity policies remain
intact. SmallString adapters preserve native string-content hashing and avalanche
traits; they are benchmark-only code. Dependencies are checksum-pinned and fetched
outside Git; see the [benchmark README](../../README.md) for adapter details.

ETL is fixed-capacity: exactly N nodes and N buckets. It has no growing-build
series. Its retained-memory numbers include the full embedded node/bucket
storage, owning adapter and allocator overhead. Lookup, iteration and mutation
call the native size-independent interface; construction/copy include adapter
ownership overhead. A stack/static ETL instance avoids the benchmark's owning
allocation overhead, but still requires its embedded storage.

The shared C++ local-static initialization machinery is warmed before the heap
baseline; its one-time allocation is recorded as `RUNTIME_INIT` in each raw log.
It is not attributed to individual maps. Per-map retained heap is not peak
memory, a count of allocations, or a fragmentation stress test.

The same new firmware served both Roo suites and still contains all ten
container implementations; a runtime selector skipped competitor measurements.
Toolchain settings, fixture payloads and dependency contents match the original
captures. The retained SmallString competitor results use the former packed,
NUL-terminated 13- and 49-byte keys. New Roo results use aligned 16- and 52-byte
keys with a cached length. That distinction matters for timing and memory.
The two changed string adapter headers are recorded in the manifest.

Code layout, allocation history and hash seeds can also differ between builds.
The SmallString overlay comes from a separate board. The charts show the
recorded timings and per-run ranges without confidence intervals.

## Change from the previous Roo capture

Both Roo captures use the same ESP32s, key fixtures and build settings.
Timings use the default hash with at least 16 keys. Each ratio is the
geometric mean of new time divided by previous time. Lower is faster.

| Fixture | Hit | Miss | Copy | Growing build | Presized build |
|---|---:|---:|---:|---:|---:|
| Random uint32 | 1.000× | 1.000× | 0.677× | 0.718× | 0.884× |
| Sequential uint32 | 1.000× | 1.000× | 0.677× | 0.718× | 0.875× |
| Random uint64 | 0.849× | 0.783× | 0.681× | 0.695× | 0.832× |
| String, 12 chars | 1.000× | 1.012× | 0.823× | 0.723× | 0.938× |
| String, 48 chars | 1.000× | 1.005× | 0.975× | 0.934× | 0.989× |
| SmallString, 12 chars | 1.000× | 1.000× | 0.809× | 0.777× | 0.958× |
| SmallString, 48 chars | 1.000× | 0.992× | 0.768× | 0.829× | 0.955× |

The published build separates bucket placement from iterator construction.
That follow-up recovered the string construction and mutation timings with
shared Abseil hashing. In the published firmware, the random uint64
Abseil erase/insert control took 2.01× the previous Roo time.
It took 1.00× in the initial combined build. Both builds
use the same table layout and hash policies.

## Default-hash performance relative to Roo

Values are geometric means of **competitor time / Roo time**: above 1× favors
Roo, below 1× favors the competitor. Each available key/size case with at least
16 entries gets equal weight, through that key's fixture limit. There are
50 cases per operation in the general table and 17 in the SmallString table.
`std::map` uses ordering. Tiny-map results remain visible in the PDFs.

### General suite

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 2.87× | 3.13× | 1.27× | 2.48× |
| std::unordered_map | 1.24× | 1.36× | 0.20× | 9.32× |
| Abseil node | 2.95× | 3.20× | 1.28× | 9.34× |
| std::map | 2.13× | 2.28× | 1.54× | 8.41× |
| Tessil robin_map | 0.97× | 0.84× | 1.12× | 0.77× |
| unordered_dense | 4.13× | 4.20× | 0.20× | 2.93× |
| ETL (fixed N) | 1.04× | 1.06× | 0.75× | 0.87× |
| Boost flat | 1.72× | 1.47× | 1.06× | 1.32× |
| emhash5 | 0.80× | 0.72× | 0.75× | 0.61× |

### SmallString suite

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 2.83× | 3.12× | 1.28× | 3.21× |
| std::unordered_map | 1.62× | 1.48× | 0.19× | 4.68× |
| Abseil node | 2.83× | 3.13× | 1.27× | 9.21× |
| std::map | 1.78× | 1.04× | 1.48× | 5.17× |
| Tessil robin_map | 1.58× | 1.37× | 1.15× | 1.82× |
| unordered_dense | 3.02× | 3.33× | 0.20× | 3.26× |
| ETL (fixed N) | 1.39× | 1.19× | 0.72× | 1.36× |
| Boost flat | 1.59× | 1.34× | 1.06× | 1.69× |
| emhash5 | 1.51× | 1.31× | 0.77× | 1.38× |

### General suite with shared Abseil hashing

The same 50 cases, now supplying `absl::Hash<Key>` to every hash map. Container
postprocessing and capacity policies still differ; std::map remains ordered.

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 2.11× | 2.17× | 1.20× | 1.89× |
| std::unordered_map | 1.50× | 1.48× | 0.18× | 7.57× |
| Abseil node | 2.15× | 2.19× | 1.20× | 6.47× |
| std::map | 1.58× | 1.60× | 1.42× | 6.50× |
| Tessil robin_map | 1.06× | 0.94× | 1.04× | 0.72× |
| unordered_dense | 3.11× | 2.97× | 0.18× | 3.20× |
| ETL (fixed N) | 1.01× | 0.97× | 0.68× | 0.86× |
| Boost flat | 1.60× | 1.42× | 1.00× | 1.17× |
| emhash5 | 0.85× | 0.75× | 0.70× | 0.60× |

### Retained memory at 128 entries

Bytes with default hashing, after growth for dynamic containers and with capacity
128 for ETL. Includes the map object, owned keys and allocator overhead; excludes
the shared query fixtures. All string rows except Roo (SmallString) use std::string.

| Container | Random uint32 | String, 12 characters | String, 48 characters |
|---|---:|---:|---:|
| Roo | 2,352 | 7,472 | 16,180 |
| Abseil flat | 2,476 | 7,468 | 16,172 |
| std::unordered_map | 4,752 | 7,856 | 16,516 |
| Abseil node | 4,476 | 7,100 | 15,720 |
| std::map | 5,160 | 7,720 | 16,424 |
| Tessil robin_map | 3,264 | 8,768 | 17,520 |
| unordered_dense | 2,684 | 5,436 | 13,952 |
| ETL (fixed N) | 2,724 | 5,668 | 14,116 |
| Boost flat | 2,348 | 7,212 | 15,916 |
| emhash5 | 3,252 | 8,756 | 17,476 |
| Roo (SmallString) | — | 5,680 | 14,896 |

## Regenerate without hardware

From `benchmarks/hashmap`, using Python 3.10+:

```sh
python scripts/refresh.py --previous reference_results/esp32/general/raw.tar.gz \
  --roo reference_results/esp32/general/roo-update.tar.gz \
  --allow-string-adapter-changes --output results/reference-general
python scripts/refresh.py --previous reference_results/esp32/small_string/raw.tar.gz \
  --roo reference_results/esp32/small_string/roo-update.tar.gz \
  --allow-string-adapter-changes --output results/reference-small-string
python -m pip install -r scripts/requirements-charts.txt
python scripts/plot.py --results-dir results/reference-general \
  --small-string-results results/reference-small-string
python scripts/plot.py --results-dir results/reference-small-string
```

Use fresh output paths: refresh refuses to overwrite existing results. Both raw
bundles are checksum-verified and strictly reanalyzed before composing the tables.
`--previous-sha256` and `--roo-sha256` optionally check the whole-archive hashes
listed in the manifest. The string-adapter flag records the two changed headers.
Source logs, metadata and coverage reports are retained
under `sources/previous` and `sources/roo`. Every measurement has a source label;
no synthetic full-suite raw capture is created. Regenerated CSVs, reports, HTML
galleries, PNG/SVG figures and PDFs stay in ignored `results/`.

The manifest records numeric-summary, archive, PDF and tooling checksums.
These PDFs use Python 3.13.5 and Matplotlib 3.11.2; rendering can vary with versions.
The original metadata is preserved unchanged. Fresh metadata identifies the
committed Roo implementation by commit ID and exact source hashes. Historical absolute paths identify inputs, not required checkout
paths. To repeat on hardware, follow the [build/capture instructions](../../README.md).
Full flash backups, dependency trees and firmware binaries are not published.
