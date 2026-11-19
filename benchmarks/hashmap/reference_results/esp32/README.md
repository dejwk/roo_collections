# ESP32 reference results — ten-container comparison

Roo was remeasured on 2026-11-19 with length-delimited SmallString keys.
The aligned 12- and 48-character Roo fixtures use `SmallString<16>` and
`SmallString<52>`, including a trailing length byte. The table code uses the
`04e569b` reciprocal and lookup baseline with a 75% fill limit.
The nine competitors retain their original 2026-11-10 captures.

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

## Default-hash performance relative to Roo

Values are geometric means of **competitor time / Roo time**: above 1× favors
Roo, below 1× favors the competitor. Each available key/size case with at least
16 entries gets equal weight, through that key's fixture limit. There are
50 cases per operation in the general table and 17 in the SmallString table.
`std::map` uses ordering. Tiny-map results remain visible in the PDFs.

### General suite

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 2.46× | 2.67× | 1.27× | 2.31× |
| std::unordered_map | 1.06× | 1.16× | 0.20× | 8.68× |
| Abseil node | 2.52× | 2.72× | 1.28× | 8.71× |
| std::map | 1.82× | 1.94× | 1.54× | 7.84× |
| Tessil robin_map | 0.83× | 0.71× | 1.13× | 0.72× |
| unordered_dense | 3.54× | 3.58× | 0.20× | 2.73× |
| ETL (fixed N) | 0.89× | 0.90× | 0.75× | 0.81× |
| Boost flat | 1.47× | 1.25× | 1.06× | 1.23× |
| emhash5 | 0.68× | 0.61× | 0.75× | 0.57× |

### SmallString suite

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 2.83× | 3.10× | 1.28× | 3.26× |
| std::unordered_map | 1.62× | 1.47× | 0.19× | 4.75× |
| Abseil node | 2.83× | 3.11× | 1.27× | 9.34× |
| std::map | 1.78× | 1.03× | 1.48× | 5.25× |
| Tessil robin_map | 1.58× | 1.36× | 1.15× | 1.85× |
| unordered_dense | 3.02× | 3.32× | 0.20× | 3.31× |
| ETL (fixed N) | 1.39× | 1.18× | 0.72× | 1.38× |
| Boost flat | 1.59× | 1.33× | 1.06× | 1.72× |
| emhash5 | 1.51× | 1.31× | 0.77× | 1.40× |

### General suite with shared Abseil hashing

The same 50 cases, now supplying `absl::Hash<Key>` to every hash map. Container
postprocessing and capacity policies still differ; std::map remains ordered.

| Container | Hit lookup | Miss lookup | Iteration | Erase + insert |
|---|---:|---:|---:|---:|
| Abseil flat | 2.11× | 2.18× | 1.20× | 2.16× |
| std::unordered_map | 1.51× | 1.49× | 0.18× | 8.64× |
| Abseil node | 2.15× | 2.20× | 1.20× | 7.39× |
| std::map | 1.59× | 1.61× | 1.42× | 7.41× |
| Tessil robin_map | 1.06× | 0.95× | 1.04× | 0.82× |
| unordered_dense | 3.12× | 2.99× | 0.18× | 3.65× |
| ETL (fixed N) | 1.01× | 0.97× | 0.68× | 0.99× |
| Boost flat | 1.61× | 1.43× | 1.00× | 1.33× |
| emhash5 | 0.85× | 0.76× | 0.70× | 0.69× |

### Retained memory at 128 entries

Bytes with default hashing, after growth for dynamic containers and with capacity
128 for ETL. Includes the map object, owned keys and allocator overhead; excludes
the shared query fixtures. All string rows except Roo (SmallString) use std::string.

| Container | Random uint32 | String, 12 characters | String, 48 characters |
|---|---:|---:|---:|
| Roo | 2,388 | 7,508 | 16,196 |
| Abseil flat | 2,476 | 7,468 | 16,172 |
| std::unordered_map | 4,752 | 7,856 | 16,516 |
| Abseil node | 4,476 | 7,100 | 15,720 |
| std::map | 5,160 | 7,720 | 16,424 |
| Tessil robin_map | 3,264 | 8,768 | 17,520 |
| unordered_dense | 2,684 | 5,436 | 13,952 |
| ETL (fixed N) | 2,724 | 5,668 | 14,116 |
| Boost flat | 2,348 | 7,212 | 15,916 |
| emhash5 | 3,252 | 8,756 | 17,476 |
| Roo (SmallString) | — | 5,460 | 14,676 |

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
committed Roo baseline and the SmallString and benchmark working-tree changes by exact
source hashes. Historical absolute paths identify inputs, not required checkout
paths. To repeat on hardware, follow the [build/capture instructions](../../README.md).
Full flash backups, dependency trees and firmware binaries are not published.
